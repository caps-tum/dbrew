/**
 * This file is part of DBrew, the dynamic binary rewriting library.
 *
 * (c) 2015-2016, Josef Weidendorfer <josef.weidendorfer@gmx.de>
 *
 * DBrew is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * DBrew is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DBrew.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dbrew.h"
#include "emulate.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "decode.h"
#include "engine.h"
#include "instr.h"
#include "printer.h"



/*------------------------------------------------------------*/
/* x86_64 capturing emulator
 * trace execution in the emulator to capture code to generate
 *
 * We maintain states (known/static vs unknown/dynamic at capture time)
 * for registers and values on stack. To be able to do the latter, we
 * assume that the known values on stack do not get changed by
 * memory writes with dynamic address. This assumption should be fine,
 * as such behavior is dangerous and potentially a bug.
 *
 * At branches to multiple possible targets, we need to travers each path by
 * saving emulator state. After emulating one path, we roll back and
 * go the other path. As this may happen recursively, we do a kind of
 * back-tracking, with emulator states stored as stacks.
 * To allow for fast saving/restoring of emulator states, each part of
 * the emulation state (registers, bytes on stack) is given by a
 * EmuStateEntry (linked) list with the current value/state in front.
 * Saving copies the complete EmuState, inheriting the individual states.
 */

static char captureState2Char(CaptureState s)
{
    assert((s >= 0) && (s < CS_Max));
    assert(CS_Max == 5);
    return "-DSR2"[s];
}

static Bool csIsStatic(CaptureState s)
{
    if ((s == CS_STATIC) || (s == CS_STATIC2)) return True;
    return False;
}



static EmuValue emuValue(uint64_t v, ValType t, CaptureState s)
{
    EmuValue ev;
    ev.val = v;
    ev.type = t;
    ev.state = s;

    return ev;
}

static EmuValue staticEmuValue(uint64_t v, ValType t)
{
    EmuValue ev;
    ev.val = v;
    ev.type = t;
    ev.state = CS_STATIC;

    return ev;
}

void resetEmuState(EmuState* es)
{
    int i;
    static Reg calleeSave[] = {
        Reg_BP, Reg_BX, Reg_12, Reg_13, Reg_14, Reg_15, Reg_None };

    es->parent = 0;

    for(i=0; i<Reg_Max; i++) {
        es->reg[i] = 0;
        es->reg_state[i] = CS_DEAD;
    }

    for(i=0; i<FT_Max; i++) {
        es->flag[i] = False;
        es->flag_state[i] = CS_DEAD;
    }

    for(i=0; i< es->stackSize; i++)
        es->stack[i] = 0;
    for(i=0; i< es->stackSize; i++)
        es->stackState[i] = CS_DEAD;

    // use real addresses for now
    es->stackStart = (uint64_t) es->stack;
    es->stackTop = es->stackStart + es->stackSize;
    es->stackAccessed = es->stackTop;

    // calling convention:
    //  rbp, rbx, r12-r15 have to be preserved by callee
    for(i=0; calleeSave[i] != Reg_None; i++)
        es->reg_state[calleeSave[i]] = CS_DYNAMIC;
    // RIP always known
    es->reg_state[Reg_IP] = CS_STATIC;

    es->depth = 0;
}

EmuState* allocEmuState(int size)
{
    EmuState* es;

    es = (EmuState*) malloc(sizeof(EmuState));
    es->stackSize = size;
    es->stack = (uint8_t*) malloc(size);
    es->stackState = (CaptureState*) malloc(sizeof(CaptureState) * size);

    return es;
}

void freeEmuState(Rewriter* r)
{
    if (!r->es) return;

    free(r->es->stack);
    free(r->es->stackState);
    free(r->es);
    r->es = 0;
}

// are the capture states of a memory resource from different EmuStates equal?
// this is required for compatibility of generated code points, and
// compatibility is needed to be able to jump between such code points
static Bool csIsEqual(EmuState* es1, CaptureState s1, uint64_t v1,
               EmuState* es2, CaptureState s2, uint64_t v2)
{
    // normalize meta states: CS_STATIC2 is equivalent to CS_STATIC
    if (s1 == CS_STATIC2) s1 = CS_STATIC;
    if (s2 == CS_STATIC2) s2 = CS_STATIC;
    // handle DEAD equal to DYNAMIC (no need to distinguish)
    if (s1 == CS_DEAD) s1 = CS_DYNAMIC;
    if (s2 == CS_DEAD) s2 = CS_DYNAMIC;

    if (s1 != s2) return False;

    switch(s1) {
    case CS_STATIC:
        // for static capture states, values have to be equal
        return (v1 == v2);

    case CS_STACKRELATIVE:
        // FIXME: in reality: same offset from a versioned anchor
        // for now: assume same anchor version (within same rewriting action)
        if (es1->parent != es2->parent) return False;
        return (v1 == v2);

    default:
        break;
    }
    return True;
}

// states are equal if metainformation is equal and static data is the same
static Bool esIsEqual(EmuState* es1, EmuState* es2)
{
    int i;

    // same state for registers?
    for(i = Reg_AX; i <= Reg_15; i++) {
        if (!csIsEqual(es1, es1->reg_state[i], es1->reg[i],
                       es2, es2->reg_state[i], es2->reg[i]))
            return False;
    }

    // same state for flag registers?
    for(i = 0; i < FT_Max; i++) {
        if (!csIsEqual(es1, es1->flag_state[i], es1->flag[i],
                       es2, es2->flag_state[i], es2->flag[i]))
            return False;
    }

    // for equality, must be at same call depth
    if (es1->depth != es2->depth) return False;

    // Stack
    // all known data has to be the same
    if (es1->stackSize < es2->stackSize) {
        int diff = es2->stackSize - es1->stackSize;
        // stack of es2 is larger: bottom should not be static
        for(i = 0; i < diff; i++) {
            if (csIsStatic(es2->stackState[i]))
                return False;
        }
        // check for equal state at byte granularity
        for(i = 0; i < es1->stackSize; i++) {
            if (!csIsEqual(es1, es1->stackState[i], es1->stack[i],
                           es2, es2->stackState[i+diff], es2->stack[i+diff]))
                return False;
        }
    }
    else {
        // es1 larger
        int diff = es1->stackSize - es2->stackSize;
        // bottom of es1 should not be static
        for(i = 0; i < diff; i++) {
            if (csIsStatic(es1->stackState[i]))
                return False;
        }
        // check for equal state at byte granularity
        for(i = 0; i < es2->stackSize; i++) {
            if (!csIsEqual(es1, es1->stackState[i+diff], es1->stack[i+diff],
                           es2, es2->stackState[i], es2->stack[i]))
                return False;
        }
    }

    return True;
}

static void copyEmuState(EmuState* dst, EmuState* src)
{
    int i;

    dst->parent = src->parent;

    for(i=0; i<Reg_Max; i++) {
        dst->reg[i] = src->reg[i];
        dst->reg_state[i] = src->reg_state[i];
    }

    for(i = 0; i < FT_Max; i++) {
        dst->flag[i] = src->flag[i];
        dst->flag_state[i] = src->flag_state[i];
    }

    dst->stackTop = src->stackTop;
    dst->stackAccessed = src->stackAccessed;
    if (src->stackSize < dst->stackSize) {
        // stack to restore is smaller than at destination:
        // fill start of destination with DEAD entries
        int diff = dst->stackSize - src->stackSize;

        dst->stackStart = src->stackStart - diff;
        for(i = 0; i < diff; i++) {
            dst->stackState[i] = CS_DEAD;
            dst->stack[i] = 0;
        }
        for(i = 0; i < src->stackSize; i++) {
            dst->stack[i+diff] = src->stack[i];
            dst->stackState[i+diff] = src->stackState[i];
        }
    }
    else {
        // stack to restore is larger than at destination:
        // make sure that start of source was never accessed
        int diff = src->stackSize - dst->stackSize;
        assert(src->stackAccessed - src->stackStart >= diff);

        dst->stackStart = src->stackStart + diff;
        for(i = 0; i < dst->stackSize; i++) {
            dst->stack[i] = src->stack[i+diff];
            dst->stackState[i] = src->stackState[i+diff];
        }
    }
    assert(dst->stackTop == dst->stackStart + dst->stackSize);

    dst->depth = src->depth;
    for(i = 0; i < src->depth; i++)
        dst->ret_stack[i] = src->ret_stack[i];
}

static EmuState* cloneEmuState(EmuState* src)
{
    EmuState* dst;

    // allocate only stack space that was accessed in source
    dst = allocEmuState(src->stackTop - src->stackAccessed);
    copyEmuState(dst, src);

    // remember that we cloned dst from src
    dst->parent = src;

    return dst;
}

// checks current state against already saved states, and returns an ID
// (which is the index in the saved state list of the rewriter)
int saveEmuState(Rewriter* r)
{
    int i;

    printf("Saving current emulator state: ");
    //printStaticEmuState(r->es, -1);
    for(i = 0; i < r->savedStateCount; i++) {
        //printf("Check ES %d\n", i);
        //printStaticEmuState(r->savedState[i], i);
        if (esIsEqual(r->es, r->savedState[i])) {
            printf("already existing, esID %d\n", i);
            return i;
        }
    }
    printf("new with esID %d\n", i);
    assert(i < SAVEDSTATE_MAX);
    r->savedState[i] = cloneEmuState(r->es);
    r->savedStateCount++;

    return i;
}

void restoreEmuState(Rewriter* r, int esID)
{
    assert((esID >= 0) && (esID < r->savedStateCount));
    assert(r->savedState[esID] != 0);
    copyEmuState(r->es, r->savedState[esID]);
}

static const char* flagName(int f)
{
    switch(f) {
    case FT_Zero:     return "ZF";
    case FT_Carry:    return "CF";
    case FT_Sign:     return "SF";
    case FT_Overflow: return "OF";
    case FT_Parity:   return "PF";
    }
    assert(0);
}

void printEmuState(EmuState* es)
{
    int i, spOff, spMin, spMax, o, oo;

    printf("Emulation State:\n");

    printf("  Call stack (current depth %d): ", es->depth);
    for(i=0; i<es->depth; i++)
        printf(" %p", (void*) es->ret_stack[i]);
    printf("%s\n", (es->depth == 0) ? " (empty)":"");

    printf("  Registers:\n");
    for(i=Reg_AX; i<Reg_8; i++) {
        printf("    %%%-3s = 0x%016lx %c", regName(i, OT_Reg64),
               es->reg[i], captureState2Char( es->reg_state[i] ));
        printf("    %%%-3s = 0x%016lx %c\n", regName(i+8, OT_Reg64),
               es->reg[i+8], captureState2Char( es->reg_state[i+8] ));
    }
    printf("    %%%-3s = 0x%016lx %c\n", regName(Reg_IP, OT_Reg64),
           es->reg[Reg_IP], captureState2Char( es->reg_state[Reg_IP] ));

    printf("  Flags: ");
    for(i = 0; i < FT_Max; i++) {
        if (i>0) printf("  ");
        printf("%s %d %c", flagName(i), es->flag[i],
               captureState2Char(es->flag_state[i]));
    }
    printf("\n");

    spOff = es->reg[Reg_SP] - es->stackStart;
    spMax = spOff /8*8 + 40;
    spMin = spOff /8*8 - 32;
    if (es->stackStart + spMin < es->stackAccessed)
        spMin = (es->stackAccessed - es->stackStart)/8*8;
    if (es->stackStart + spMax > es->stackTop)
        spMax = es->stackSize -1;
    if (spMin >= spMax)
        printf("  Stack: (empty)\n");
    else {
        printf("  Stack:\n");
        for(o = spMin; o < spMax; o += 8) {
            printf("   %016lx ", (uint64_t) (es->stackStart + o));
            for(oo = o; oo < o+8 && oo <= spMax; oo++) {
                printf(" %s%02x %c", (oo == spOff) ? "*" : " ", es->stack[oo],
                       captureState2Char(es->stackState[oo]));
            }
            printf("\n");
        }
        printf("   %016lx  %s\n",
               (uint64_t) (es->stackStart + o), (o == spOff) ? "*" : " ");
    }
}

// print only state information important to distinguish for capturing
void printStaticEmuState(EmuState* es, int esID)
{
    int i, c, cc;

    printf("Emulation Static State (esID %d, call depth %d):\n",
           esID, es->depth);

    printf("  Registers: ");
    c = 0;
    for(i=Reg_AX; i<=Reg_15; i++) {
        if (es->reg_state[i] == CS_DEAD) continue;
        if (es->reg_state[i] == CS_DYNAMIC) continue;

        if (c>0) printf(", ");
        switch(es->reg_state[i]) {
        case CS_STATIC:
        case CS_STATIC2:
            printf("%%%s (0x%lx)", regName(i, OT_Reg64), es->reg[i]);
            break;
        case CS_STACKRELATIVE:
            printf("%%%s (R %ld)",
                   regName(i, OT_Reg64), es->reg[i] - es->stackTop);
            break;
        default: assert(0);
        }
        c++;
    }
    if (c>0)
        printf("\n");
    else
        printf("(none)\n");

    printf("  Flags: ");
    c = 0;
    for(i = 0; i < FT_Max; i++) {
        if (!csIsStatic(es->flag_state[i])) continue;
        if (c>0) printf(", ");
        printf("%s (%d)", flagName(i), es->flag[i]);
        c++;
    }
    if (c>0)
        printf("\n");
    else
        printf("(none)\n");

    printf("  Stack: ");
    cc = 0;
    c = 0;
    for(i = 0; i < es->stackSize; i++) {
        if (!csIsStatic(es->stackState[i])) {
            c = 0;
            continue;
        }
        if (c == 0)
            printf("\n   %016lx ", (uint64_t) (es->stackStart + i));
        else
            printf(" ");
        printf("%02x", es->stack[i]);
        cc++;
        c++;
    }
    if (cc>0)
        printf("\n");
    else
        printf("(none)\n");
}

static char combineState(CaptureState s1, CaptureState s2,
                  Bool isSameValue)
{
    // dead/invalid: combining with something invalid makes result invalid
    if ((s1 == CS_DEAD) || (s2 == CS_DEAD)) return CS_DEAD;

    // if both are static, static-ness is preserved
    if (csIsStatic(s1) && csIsStatic(s2)) {
        if ((s1 == CS_STATIC2) || (s2 == CS_STATIC2)) return CS_STATIC2;
        return CS_STATIC;
    }

    // stack-relative handling:
    // depends on combining of sub-state of one value or combining two values
    if (isSameValue) {
        // if both are stack-relative, it is preserved
        if ((s1 == CS_STACKRELATIVE) && (s2 == CS_STACKRELATIVE))
            return CS_STACKRELATIVE;
    }
    else {
        // STACKRELATIVE is preserved if other is STATIC (FIXME: only ADD!)
        if ((s1 == CS_STACKRELATIVE) && csIsStatic(s2))
            return CS_STACKRELATIVE;
        if (csIsStatic(s1) && (s2 == CS_STACKRELATIVE))
            return CS_STACKRELATIVE;
    }

    return CS_DYNAMIC;
}

static char combineState4Flags(CaptureState s1, CaptureState s2)
{
    CaptureState s;

    s = combineState(s1, s2, 0);
    // STACKRELATIVE/STATIC2 makes no sense for flags
    if (s == CS_STACKRELATIVE) s = CS_DYNAMIC;
    if (s == CS_STATIC2) s = CS_STATIC;

    return s;
}

//---------------------------------------------------------------
// Functions to find/allocate new (captured) basic blocks (CBBs).
// A CBB is keyed by a function address and world state ID
// (actually an emulator state esID)

// remove any previously allocated CBBs (keep allocated memory space)
void resetCapturing(Rewriter* r)
{
    // only to be called after initRewriter()
    assert(r->capInstr != 0);
    assert(r->capBB != 0);

    r->capBBCount = 0;
    r->capInstrCount = 0;
    r->currentCapBB = 0;

    r->capStackTop = -1;
    r->savedStateCount = 0;
}

// return 0 if not found
CBB *findCaptureBB(Rewriter* r, uint64_t f, int esID)
{
    int i;

    for(i = 0; i < r->capBBCount; i++)
        if ((r->capBB[i].dec_addr == f) && (r->capBB[i].esID == esID))
            return &(r->capBB[i]);

    return 0;
}

// allocate a BB structure to collect instructions for capturing
CBB* getCaptureBB(Rewriter* c, uint64_t f, int esID)
{
    CBB* bb;

    // already captured?
    bb = findCaptureBB(c, f, esID);
    if (bb) return bb;

    // start capturing of new BB beginning at f
    assert(c->capBBCount < c->capBBCapacity);
    bb = &(c->capBB[c->capBBCount]);
    c->capBBCount++;
    bb->dec_addr = f;
    bb->esID = esID;
    bb->fc = config_find_function(c, f);

    bb->count = 0;
    bb->instr = 0; // updated on first instruction added
    bb->nextBranch = 0;
    bb->nextFallThrough = 0;
    bb->endType = IT_None;
    bb->preferBranch = False;

    bb->size = -1; // size of 0 could be valid
    bb->addr1 = 0;
    bb->addr2 = 0;
    bb->genJcc8 = False;
    bb->genJump = False;

    return bb;
}

char* cbb_prettyName(CBB* bb)
{
    static char buf[100];
    int off;

    if ((bb->fc == 0) || (bb->fc->func > bb->dec_addr))
        off = sprintf(buf, "0x%lx", bb->dec_addr);
    else if (bb->fc->func == bb->dec_addr)
        off = sprintf(buf, "%s", bb->fc->name);
    else
        off = sprintf(buf, "%s+%lx", bb->fc->name, bb->dec_addr - bb->fc->func);

    sprintf(buf+off, "|%d", bb->esID);

    return buf;
}

int pushCaptureBB(Rewriter* r, CBB* bb)
{
    assert(r->capStackTop + 1 < CAPTURESTACK_LEN);
    r->capStackTop++;
    r->capStack[r->capStackTop] = bb;

    return r->capStackTop;
}

CBB* popCaptureBB(Rewriter* r)
{
    CBB* bb = r->currentCapBB;
    assert(r->capStack[r->capStackTop] == bb);
    r->capStackTop--;
    r->currentCapBB = 0;

    return bb;
}

Instr* newCapInstr(Rewriter* r)
{
    Instr* instr;

    assert(r->capInstrCount < r->capInstrCapacity);
    instr = r->capInstr + r->capInstrCount;
    r->capInstrCount++;

    return instr;
}

// capture a new instruction
void capture(Rewriter* r, Instr* instr)
{
    Instr* newInstr;
    CBB* cbb = r->currentCapBB;
    if (cbb == 0) return;

    if (r->showEmuSteps)
        printf("Capture '%s' (into %s + %d)\n",
               instr2string(instr, 0), cbb_prettyName(cbb), cbb->count);

    newInstr = newCapInstr(r);
    if (cbb->instr == 0) {
        cbb->instr = newInstr;
        assert(cbb->count == 0);
    }
    copyInstr(newInstr, instr);
    cbb->count++;
}


//---------------------------------------------------------
// emulator functions


// flag setting helpers

/* Setting some flags can get complicated.
 * From libx86emu/prim_ops.c (github.com/wfeldt/libx86emu)
 */
static uint32_t parity_tab[8] =
{
    0x96696996, 0x69969669, 0x69969669, 0x96696996,
    0x69969669, 0x96696996, 0x96696996, 0x69969669,
};

#define PARITY(x)   (((parity_tab[(x) / 32] >> ((x) % 32)) & 1) == 0)
#define XOR2(x)     (((x) ^ ((x)>>1)) & 0x1)

// set flags for operation "v1 - v2"
static CaptureState setFlagsSub(EmuState* es, EmuValue* v1, EmuValue* v2)
{
    CaptureState st;
    uint64_t r, bc, d, s;

    st = combineState4Flags(v1->state, v2->state);
    es->flag_state[FT_Carry]    = st;
    es->flag_state[FT_Zero]     = st;
    es->flag_state[FT_Sign]     = st;
    es->flag_state[FT_Overflow] = st;
    es->flag_state[FT_Parity]   = st;

    assert(v1->type == v2->type);

    d = v1->val;
    s = v2->val;
    r = d - s;
    bc = (r & (~d | s)) | (~d & s);

    es->flag[FT_Zero]   = (d == s);
    es->flag[FT_Parity] = PARITY(r & 0xff);
    switch(v1->type) {
    case VT_8:
        es->flag[FT_Sign]     = (r >> 7) & 1;
        es->flag[FT_Carry]    = (bc >>7) & 1;
        es->flag[FT_Overflow] = XOR2(bc >> 6);
        break;
    case VT_32:
        es->flag[FT_Sign]     = (r >> 31) & 1;
        es->flag[FT_Carry]    = (bc >>31) & 1;
        es->flag[FT_Overflow] = XOR2(bc >> 30);
        break;
    case VT_64:
        es->flag[FT_Sign]     = (r >> 63) & 1;
        es->flag[FT_Carry]    = (bc >>63) & 1;
        es->flag[FT_Overflow] = XOR2(bc >> 62);
        break;
    default: assert(0);
    }

    return st;
}

// set flags for operation "v1 + v2"
static void setFlagsAdd(EmuState* es, EmuValue* v1, EmuValue* v2)
{
    CaptureState st;
    uint64_t r, cc, d, s;

    st = combineState4Flags(v1->state, v2->state);
    es->flag_state[FT_Carry]    = st;
    es->flag_state[FT_Zero]     = st;
    es->flag_state[FT_Sign]     = st;
    es->flag_state[FT_Overflow] = st;
    es->flag_state[FT_Parity]   = st;

    assert(v1->type == v2->type);

    d = v1->val;
    s = v2->val;
    r = d + s;
    cc = (r & (~d | s)) | (~d & s);

    es->flag[FT_Parity] = PARITY(r & 0xff);
    switch(v1->type) {
    case VT_8:
        es->flag[FT_Carry]    = (cc >> 7) & 1;
        es->flag[FT_Zero]     = ((r & ((1<<8)-1)) == 0);
        es->flag[FT_Sign]     = (r >> 7) & 1;
        es->flag[FT_Overflow] = XOR2(cc >> 6);
        break;
    case VT_32:
        es->flag[FT_Carry]    = (cc >> 31) & 1;
        es->flag[FT_Zero]     = ((r & ((1l<<32)-1)) == 0);
        es->flag[FT_Sign]     = (r >> 31) & 1;
        es->flag[FT_Overflow] = XOR2(cc >> 30);
        break;
    case VT_64:
        es->flag[FT_Carry]    = (cc >> 63) & 1;
        es->flag[FT_Zero]     = (r  == 0);
        es->flag[FT_Sign]     = (r >> 63) & 1;
        es->flag[FT_Overflow] = XOR2(cc >> 62);
        break;
    default: assert(0);
    }
}

// for bitwise operations: And, Xor, Or
static CaptureState setFlagsBit(EmuState* es, InstrType it,
                         EmuValue* v1, EmuValue* v2, Bool sameOperands)
{
    CaptureState s;
    uint64_t res;

    assert(v1->type == v2->type);

    s = combineState4Flags(v1->state, v2->state);
    // xor op,op results in known zero
    if ((it == IT_XOR) && sameOperands) s = CS_STATIC;

    // carry/overflow always cleared
    es->flag[FT_Carry] = 0;
    es->flag[FT_Overflow] = 0;
    es->flag_state[FT_Carry] = CS_STATIC;
    es->flag_state[FT_Overflow] = CS_STATIC;

    es->flag_state[FT_Zero] = s;
    es->flag_state[FT_Sign] = s;
    es->flag_state[FT_Parity] = s;

    switch(it) {
    case IT_AND: res = v1->val & v2->val; break;
    case IT_XOR: res = v1->val ^ v2->val; break;
    case IT_OR:  res = v1->val | v2->val; break;
    default: assert(0);
    }

    es->flag[FT_Zero]  = (res == 0);
    es->flag[FT_Parity] = PARITY(res & 0xff);
    switch(v1->type) {
    case VT_8:
        es->flag[FT_Sign] = ((res & (1l<<7)) != 0);
        break;
    case VT_32:
        es->flag[FT_Sign] = ((res & (1l<<31)) != 0);
        break;
    case VT_64:
        es->flag[FT_Sign] = ((res & (1l<<63)) != 0);
        break;
    default: assert(0);
    }

    return s;
}


// helpers for capture processing

// if addr on stack, return true and stack offset in <off>,
//  otherwise return false
// the returned offset is static only if address is stack-relative
static Bool getStackOffset(EmuState* es, EmuValue* addr, EmuValue* off)
{
    if ((addr->val >= es->stackStart) && (addr->val < es->stackTop)) {
        off->type = VT_32;
        off->state = (addr->state == CS_STACKRELATIVE) ? CS_STATIC : CS_DYNAMIC;
        off->val = addr->val - es->stackStart;
        return True;
    }
    return False;
}

static CaptureState getStackState(EmuState* es, EmuValue* off)
{
    if (off->state == CS_STATIC) {
        if (off->val >= es->stackSize) return CS_DEAD;
        if (off->val < es->stackAccessed - es->stackStart) return CS_DEAD;
        return es->stackState[off->val];
    }
    return CS_DYNAMIC;
}

static void getStackValue(EmuState* es, EmuValue* v, EmuValue* off)
{
    int i, count;
    CaptureState state;

    assert((off->val >= 0) && (off->val < es->stackSize));

    switch(v->type) {
    case VT_32:
        v->val = *(uint32_t*) (es->stack + off->val);
        count = 4;
        break;

    case VT_64:
        v->val = *(uint64_t*) (es->stack + off->val);
        count = 8;
        break;

    default: assert(0);
    }

    if (off->state == CS_STATIC) {
        state = getStackState(es, off);
        for(i=1; i<count; i++)
            state = combineState(state, es->stackState[off->val + i], 1);
    }
    else
        state = CS_DYNAMIC;

    v->state = state;
}


static void setStackValue(EmuState* es, EmuValue* v, EmuValue* off)
{
    uint32_t* a32;
    uint64_t* a64;
    int i, count;

    switch(v->type) {
    case VT_32:
        a32 = (uint32_t*) (es->stack + off->val);
        *a32 = (uint32_t) v->val;
        count = 4;
        break;

    case VT_64:
        a64 = (uint64_t*) (es->stack + off->val);
        *a64 = (uint64_t) v->val;
        count = 8;
        break;

    default: assert(0);
    }

    if (off->state == CS_STATIC) {
        for(i=0; i<count; i++)
            es->stackState[off->val + i] = v->state;
    }

    if (es->stackStart + off->val < es->stackAccessed)
        es->stackAccessed = es->stackStart + off->val;
}

static void getRegValue(EmuValue* v, EmuState* es, Reg r, ValType t)
{
    v->type = t;
    v->val = es->reg[r];
    v->state = es->reg_state[r];
}

static void setRegValue(EmuValue* v, EmuState* es, Reg r, ValType t)
{
    assert(v->type == t);
    es->reg[r] = v->val;
    es->reg_state[r] = v->state;
}

static void getMemValue(EmuValue* v, EmuValue* addr, EmuState* es, ValType t,
                 Bool shouldBeStack)
{
    EmuValue off;
    int isOnStack;

    isOnStack = getStackOffset(es, addr, &off);
    if (isOnStack) {
        v->type = t;
        getStackValue(es, v, &off);
        return;
    }

    assert(!shouldBeStack);
    v->state = CS_DYNAMIC;
    // explicit request to make memory access result static
    if (addr->state == CS_STATIC2) v->state = CS_STATIC2;

    v->type = t;
    switch(t) {
    case VT_8:  v->val = *(uint8_t*) addr->val; break;
    case VT_32: v->val = *(uint32_t*) addr->val; break;
    case VT_64: v->val = *(uint64_t*) addr->val; break;
    default: assert(0);
    }
}

// reading memory using segment override (fs/gs)
static void getSegMemValue(EmuValue* v, EmuValue* addr, ValType t, OpSegOverride s)
{
    assert(s != OSO_None);
    uint32_t v32;
    uint8_t v8;

    // memory accessed via fs/gs always is dynamic
    v->state = CS_DYNAMIC;
    v->type = t;
    switch(t) {
    case VT_8:
        switch(s) {
        case OSO_UseFS:
            __asm__ ("mov %%fs:(%1),%0" : "=r" (v8) : "r" (addr->val) );
            break;
        default: assert(0);
        }
        v->val = v8;
        break;

    case VT_32:
        switch(s) {
        case OSO_UseFS:
            __asm__ ("mov %%fs:(%1),%0" : "=r" (v32) : "r" (addr->val) );
            break;
        default: assert(0);
        }
        v->val = v32;
        break;

    case VT_64:
        switch(s) {
        case OSO_UseFS:
            __asm__ ("mov %%fs:(%1),%0" : "=r" (v->val) : "r" (addr->val) );
            break;
        default: assert(0);
        }
        break;

    default: assert(0);
    }
}

static void setMemValue(EmuValue* v, EmuValue* addr, EmuState* es, ValType t,
                 int shouldBeStack)
{
    EmuValue off;
    uint32_t* a32;
    uint64_t* a64;
    Bool isOnStack;

    assert(v->type == t);
    isOnStack = getStackOffset(es, addr, &off);
    if (isOnStack) {
        setStackValue(es, v, &off);
        return;
    }

    assert(!shouldBeStack);

    switch(t) {
    case VT_32:
        a32 = (uint32_t*) addr->val;
        *a32 = (uint32_t) v->val;
        break;

    case VT_64:
        a64 = (uint64_t*) addr->val;
        *a64 = (uint64_t) v->val;
        break;

    default: assert(0);
    }
}

// helper for getOpAddr()
static void addRegToValue(EmuValue* v, EmuState* es, Reg r, int scale)
{
    if (r == Reg_None) return;

    v->state = combineState(v->state, es->reg_state[r], 0);
    v->val += scale * es->reg[r];
}

// get resulting address (and state) for memory operands
// this cannot be used with fs/gs segment override
static void getOpAddr(EmuValue* v, EmuState* es, Operand* o)
{
    assert(opIsInd(o));
    assert(o->seg == OSO_None);

    v->type = VT_64;
    v->val = o->val;
    v->state = CS_STATIC;

    if (o->reg != Reg_None)
        addRegToValue(v, es, o->reg, 1);

    if (o->scale > 0)
        addRegToValue(v, es, o->ireg, o->scale);
}

// returned value v should be casted to expected type (8/16/32 bit)
static void getOpValue(EmuValue* v, EmuState* es, Operand* o)
{
    EmuValue addr;

    switch(o->type) {
    case OT_Imm8:
    case OT_Imm16:
    case OT_Imm32:
    case OT_Imm64:
        *v = staticEmuValue(o->val, opValType(o));
        return;

    case OT_Reg32:
        v->type = VT_32;
        v->val = (uint32_t) es->reg[o->reg];
        v->state = es->reg_state[o->reg];
        return;

    case OT_Reg64:
        v->type = VT_64;
        v->val = (uint64_t) es->reg[o->reg];
        v->state = es->reg_state[o->reg];
        return;

    case OT_Ind8:
    case OT_Ind32:
    case OT_Ind64:
        if (o->seg != OSO_None) {
            // access memory with segment override (fs:/gs:)
            // get offset within the segment
            Operand noSegOp;
            copyOperand(&noSegOp, o);
            noSegOp.seg = OSO_None;
            getOpAddr(&addr, es, &noSegOp);

            getSegMemValue(v, &addr, opValType(o), o->seg);
        }
        else {
            getOpAddr(&addr, es, o);
            getMemValue(v, &addr, es, opValType(o), 0);
        }
        return;

    default: assert(0);
    }
}

// only the bits of v are used which are required for operand type
static void setOpValue(EmuValue* v, EmuState* es, Operand* o)
{
    EmuValue addr;

    assert(v->type == opValType(o));
    switch(o->type) {
    case OT_Reg32:
        es->reg[o->reg] = (uint32_t) v->val;
        es->reg_state[o->reg] = v->state;
        return;

    case OT_Reg64:
        es->reg[o->reg] = v->val;
        es->reg_state[o->reg] = v->state;
        return;

    case OT_Ind32:
    case OT_Ind64:
        getOpAddr(&addr, es, o);
        setMemValue(v, &addr, es, opValType(o), 0);
        return;

    default: assert(0);
    }
}

// Do we maintain capture state for a value pointed to by an operand?
// Returns false for memory locations not on stack or when stack offset
//  is not static/known.
static Bool keepsCaptureState(EmuState* es, Operand* o)
{
    EmuValue addr;
    EmuValue off;
    Bool isOnStack;

    // never should be called with immediate ops (do not point to location)
    assert(!opIsImm(o));
    if (opIsGPReg(o)) return 1;

    getOpAddr(&addr, es, o);
    isOnStack = getStackOffset(es, &addr, &off);
    if (!isOnStack) return 0;
    return csIsStatic(off.state);
}

// apply known state to memory operand (this modifies the operand in-place)
static void applyStaticToInd(Operand* o, EmuState* es)
{
    if (!opIsInd(o)) return;

    if ((o->reg != Reg_None) && csIsStatic(es->reg_state[o->reg])) {
        o->val += es->reg[o->reg];
        o->reg = Reg_None;
    }
    if ((o->scale > 0) && csIsStatic(es->reg_state[o->ireg])) {
        o->val += o->scale * es->reg[o->ireg];
        o->scale = 0;
    }
}


// capture processing for instruction types

// both MOV and MOVSX (sign extend 32->64)
static void captureMov(Rewriter* c, Instr* orig, EmuState* es, EmuValue* res)
{
    Instr i;
    Operand *o;

    // data movement from orig->src to orig->dst, value is res

    if (res->state == CS_DEAD) return;

    o = &(orig->src);
    if (csIsStatic(res->state)) {
        // no need to update data if capture state is maintained
        if (keepsCaptureState(es, &(orig->dst))) return;

        // source is static, use immediate
        o = getImmOp(res->type, res->val);
    }
    initBinaryInstr(&i, orig->type, orig->vtype, &(orig->dst), o);
    applyStaticToInd(&(i.dst), es);
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

static void captureCMov(Rewriter* c, Instr* orig, EmuState* es,
                 EmuValue* res, CaptureState cState, Bool cond)
{
    Instr i;

    // data movement from orig->src to orig->dst, value is res
    // but only if condition is true

    // cmov always has a register as destination
    assert(opIsReg(&(orig->dst)));

    if (csIsStatic(cState)) {
        if (cond) captureMov(c, orig, es, res);
        return;
    }
    // condition state is unknown

    if (res->state == CS_DEAD) return;

    if (csIsStatic(res->state)) {
        // we need to be prepared that there may be a move happening
        // need to update source with known value as it may be moved
        initBinaryInstr(&i, IT_MOV, res->type,
                        &(orig->src), getImmOp(res->type, res->val));
        applyStaticToInd(&(i.src), es);
        capture(c, &i);

        // resulting value becomes unknown, even if source was static
        res->state = CS_DYNAMIC;
    }
    initBinaryInstr(&i, orig->type, orig->vtype, &(orig->dst), &(orig->src));
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

static void captureIDiv(Rewriter* r, Instr* orig, CaptureState resState, EmuState* es)
{
    Instr i;
    EmuValue v;

    // capture if not static
    if (csIsStatic(resState)) return;


    getOpValue(&v, es, &(orig->dst));
    if (csIsStatic(v.state)) {
        // divide by 1 can be skipped
        if (v.val == 1) return;

        initBinaryInstr(&i, IT_MOV, v.type,
                        &(orig->dst), getImmOp(v.type, v.val));
        applyStaticToInd(&(i.dst), es); // dst may be memory on stack!
        capture(r, &i);
    }

    // update ax/dx with known values, use type from idiv operand
    if (csIsStatic(es->reg_state[Reg_AX])) {
        initBinaryInstr(&i, IT_MOV, v.type,
                        getRegOp(v.type, Reg_AX),
                        getImmOp(v.type, es->reg[Reg_AX]));
        capture(r, &i);
    }
    if (csIsStatic(es->reg_state[Reg_DX])) {
        initBinaryInstr(&i, IT_MOV, v.type,
                        getRegOp(v.type, Reg_DX),
                        getImmOp(v.type, es->reg[Reg_DX]));
        capture(r, &i);
    }

    initUnaryInstr(&i, orig->type, &(orig->dst));
    applyStaticToInd(&(i.dst), es);
    capture(r, &i);
}

// dst = dst op src
static void captureBinaryOp(Rewriter* c, Instr* orig, EmuState* es, EmuValue* res)
{
    EmuValue opval;
    Instr i;
    Operand *o;

    if (res->state == CS_DEAD) return;

    if (csIsStatic(res->state)) {
        // force results to become unknown?
        if (c->cc->force_unknown[es->depth]) {
            res->state = CS_DYNAMIC;
        }
        else {
            // no need to update data if capture state is maintained
            if (keepsCaptureState(es, &(orig->dst))) return;
        }
        // if result is known and goes to memory, generate imm store
        initBinaryInstr(&i, IT_MOV, res->type,
                        &(orig->dst), getImmOp(res->type, res->val));
        applyStaticToInd(&(i.dst), es);
        capture(c, &i);
        return;
    }

    // if dst (= 2.op) known/constant and a reg/stack, we need to update it
    // example: %eax += %ebx with %eax known to be 5  =>  %eax=5, %eax+=%ebx
    getOpValue(&opval, es, &(orig->dst));
    if (keepsCaptureState(es, &(orig->dst)) && csIsStatic(opval.state)) {

        // - instead of adding src to 0, we can move the src to dst
        // - instead of multiply src with 1, move
        // TODO: mulitply with 0: here too late, state of result gets static
        if (((orig->type == IT_ADD) && (opval.val == 0)) ||
            ((orig->type == IT_IMUL) && (opval.val == 1))) {
            initBinaryInstr(&i, IT_MOV, opval.type,
                            &(orig->dst), &(orig->src));
            applyStaticToInd(&(i.dst), es);
            applyStaticToInd(&(i.src), es);
            capture(c, &i);
            return;
        }

        initBinaryInstr(&i, IT_MOV, opval.type,
                        &(orig->dst), getImmOp(opval.type, opval.val));
        capture(c, &i);
    }

    o = &(orig->src);
    getOpValue(&opval, es, &(orig->src));
    if (csIsStatic(opval.state)) {
        // if 1st source (=src) is known/constant and a reg, make it immediate

        if (((orig->type == IT_ADD) && (opval.val == 0)) ||
            ((orig->type == IT_SHL) && (opval.val == 0)) ||
            ((orig->type == IT_SHR) && (opval.val == 0)) ||
            ((orig->type == IT_SAR) && (opval.val == 0)) ||
            ((orig->type == IT_IMUL) && (opval.val == 1))) {
            // adding 0 / multiplying with 1 changes nothing...
            return;
        }
        o = getImmOp(opval.type, opval.val);
    }
    initBinaryInstr(&i, orig->type, res->type, &(orig->dst), o);
    applyStaticToInd(&(i.dst), es);
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

// dst = unary-op dst
static void captureUnaryOp(Rewriter* c, Instr* orig, EmuState* es, EmuValue* res)
{
    Instr i;

    if (csIsStatic(res->state)) return;

    initUnaryInstr(&i, orig->type, &(orig->dst));
    applyStaticToInd(&(i.dst), es);
    capture(c, &i);
}

static void captureLea(Rewriter* c, Instr* orig, EmuState* es, EmuValue* res)
{
    Instr i;

    assert(opIsReg(&(orig->dst)));
    if (csIsStatic(res->state)) {
        if (c->cc->force_unknown[es->depth]) {
            // force results to become unknown => load value into dest

            res->state = CS_DYNAMIC;
            initBinaryInstr(&i, IT_MOV, res->type,
                            &(orig->dst), getImmOp(res->type, res->val));
            capture(c, &i);
        }
        return;
    }
    initBinaryInstr(&i, IT_LEA, orig->vtype, &(orig->dst), &(orig->src));
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

static void captureCmp(Rewriter* c, Instr* orig, EmuState* es, CaptureState s)
{
    EmuValue opval;
    Instr i;
    Operand *o;

    if (csIsStatic(s)) return;

    getOpValue(&opval, es, &(orig->dst));
    if (csIsStatic(opval.state)) {
        // cannot replace dst with imm: no such encoding => update dst
        initBinaryInstr(&i, IT_MOV, opval.type,
                        &(orig->dst), getImmOp(opval.type, opval.val));
        capture(c, &i);
    }

    o = &(orig->src);
    getOpValue(&opval, es, &(orig->src));
    if (csIsStatic(opval.state))
        o = getImmOp(opval.type, opval.val);

    initBinaryInstr(&i, IT_CMP, orig->vtype, &(orig->dst), o);
    applyStaticToInd(&(i.dst), es);
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

static void captureTest(Rewriter* c, Instr* orig, EmuState* es, CaptureState s)
{
    Instr i;

    if (csIsStatic(s)) return;

    initBinaryInstr(&i, IT_TEST, orig->vtype, &(orig->dst), &(orig->src));
    applyStaticToInd(&(i.dst), es);
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

void captureRet(Rewriter* c, Instr* orig, EmuState* es)
{
    EmuValue v;
    Instr i;

    // when returning an integer: if AX state is static, load constant
    if (!c->cc->hasReturnFP) {
        getRegValue(&v, es, Reg_AX, VT_64);
        if (csIsStatic(v.state)) {
            initBinaryInstr(&i, IT_MOV, VT_64,
                            getRegOp(VT_64, Reg_AX), getImmOp(v.type, v.val));
            capture(c, &i);
        }
    }
    capture(c, orig);
}

// helper for capturePassThrough: do capture state modifications
// if provided as meta information (e.g. setting values in locations unknown)
static void processPassThrough(Instr* i, EmuState* es)
{
    assert(i->ptLen >0);
    if (i->ptSChange == SC_None) return;

    switch(i->dst.type) {
    case OT_Reg32:
    case OT_Reg64:
        if (opIsGPReg(&(i->dst)))
            es->reg_state[i->dst.reg] = CS_DYNAMIC;
        break;

        // memory locations not handled yet

    default: assert(0);
    }
}

static void capturePassThrough(Rewriter* c, Instr* orig, EmuState* es)
{
    Instr i;

    // pass-through: may have influence to emu state
    processPassThrough(orig, es);

    assert(orig->ptLen >0);
    initSimpleInstr(&i, orig->type);
    i.vtype  = orig->vtype;

    i.ptLen  = orig->ptLen;
    i.ptEnc  = orig->ptEnc;
    i.ptPSet = orig->ptPSet;
    for(int j=0; j<orig->ptLen; j++)
        i.ptOpc[j] = orig->ptOpc[j];

    switch(orig->ptEnc) {
    case OE_MR:
        assert(opIsReg(&(orig->dst)) || opIsInd(&(orig->dst)));
        assert(opIsReg(&(orig->src)));

        i.form = OF_2;
        copyOperand( &(i.dst), &(orig->dst));
        copyOperand( &(i.src), &(orig->src));
        applyStaticToInd(&(i.dst), es);
        break;

    case OE_RM:
        assert(opIsReg(&(orig->src)) || opIsInd(&(orig->src)));
        assert(opIsReg(&(orig->dst)));

        i.form = OF_2;
        copyOperand( &(i.dst), &(orig->dst));
        copyOperand( &(i.src), &(orig->src));
        applyStaticToInd(&(i.src), es);
        break;

    default: assert(0);
    }
    capture(c, &i);
}

// this ends a captured BB, queuing new paths to be traced
static void captureJcc(Rewriter* r, InstrType it,
                uint64_t branchTarget, uint64_t fallthroughTarget,
                Bool didBranch)
{
    CBB *cbb, *cbbBR, *cbbFT;
    int esID;

    // do not end BB and assume jump fixed?
    if (r->cc->branches_known) return;

    cbb = popCaptureBB(r);
    cbb->endType = it;
    // use observed behavior from trace as hint for code generation
    cbb->preferBranch = didBranch;

    esID = saveEmuState(r);
    cbbFT = getCaptureBB(r, fallthroughTarget, esID);
    cbbBR = getCaptureBB(r, branchTarget, esID);
    cbb->nextFallThrough = cbbFT;
    cbb->nextBranch = cbbBR;

    // entry pushed last will be processed first
    if (didBranch) {
        pushCaptureBB(r, cbbFT);
        pushCaptureBB(r, cbbBR);
    }
    else {
        pushCaptureBB(r, cbbBR);
        pushCaptureBB(r, cbbFT);
    }
    // current CBB should be closed.
    // (we have to open a new CBB before allowing new instructions to capture)
    assert(r->currentCapBB == 0);
}


//----------------------------------------------------------
// Emulator for instruction types



// return 0 to fall through to next instruction, or return address to jump to
uint64_t emulateInstr(Rewriter* c, EmuState* es, Instr* instr)
{
    EmuValue vres, v1, v2, addr;
    CaptureState s;
    ValType vt;

    if (instr->ptLen > 0) {
        // memory addressing in captured instructions depends on emu state
        capturePassThrough(c, instr, es);
        return 0;
    }

    switch(instr->type) {

    case IT_ADD:
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        vt = opValType(&(instr->dst));
        // sign-extend src/v2 if needed
        if (instr->src.type == OT_Imm8) {
            // sign-extend to 64bit (may be cutoff later)
            v2.val = (int64_t) (int8_t) v2.val;
            v2.type = vt;
        }

        setFlagsAdd(es, &v1, &v2);
        assert(v1.type == v2.type);

        switch(vt) {
        case VT_32:
            v1.val = ((uint32_t) v1.val + (uint32_t) v2.val);
            break;

        case VT_64:
            v1.val = v1.val + v2.val;
            break;

        default:assert(0);
        }

        v1.state = combineState(v1.state, v2.state, 0);
        // for capture we need state of original dst, do it before setting dst
        captureBinaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;

    case IT_CALL:
        // TODO: keep call. For now, we always inline
        getOpValue(&v1, es, &(instr->dst));
        assert(es->depth < MAX_CALLDEPTH);
        assert(csIsStatic(v1.state)); // call target must be known

        // push address of instruction after CALL onto stack
        es->reg[Reg_SP] -= 8;
        addr = emuValue(es->reg[Reg_SP], VT_64, es->reg_state[Reg_SP]);
        v2.state = CS_DYNAMIC;
        v2.type = VT_64;
        v2.val = instr->addr + instr->len;
        setMemValue(&v2, &addr, es, VT_64, 1);

        es->ret_stack[es->depth++] = v2.val;

        if (c->addInliningHints) {
            Instr i;
            initSimpleInstr(&i, IT_HINT_CALL);
            capture(c, &i);
        }

        // special handling for known functions
        if ((v1.val == (uint64_t) makeDynamic) &&
            csIsStatic(es->reg_state[Reg_DI])) {
            // update register value to static value
            Instr i;
            initBinaryInstr(&i, IT_MOV, VT_64,
                            getRegOp(VT_64, Reg_DI),
                            getImmOp(VT_64, es->reg[Reg_DI]));
            capture(c, &i);
            es->reg_state[Reg_DI] = CS_DYNAMIC;
        }
        if (v1.val == (uint64_t) makeStatic)
            es->reg_state[Reg_DI] = CS_STATIC2;

        // address to jump to
        return v1.val;

    case IT_CLTQ:
        switch(instr->vtype) {
        case VT_32:
            es->reg[Reg_AX] = (int32_t) (int16_t) es->reg[Reg_AX];
            break;
        case VT_64:
            es->reg[Reg_AX] = (int64_t) (int32_t) es->reg[Reg_AX];
            break;
        default: assert(0);
        }
        if (!csIsStatic(es->reg_state[Reg_AX]))
            capture(c, instr);
        break;

    case IT_CQTO:
        switch(instr->vtype) {
        case VT_64:
            // sign-extend eax to edx:eax
            es->reg[Reg_DX] = (es->reg[Reg_AX] & (1<<30)) ? ((uint32_t)-1) : 0;
            break;
        case VT_128:
            // sign-extend rax to rdx:rax
            es->reg[Reg_DX] = (es->reg[Reg_AX] & (1ul<<62)) ? ((uint64_t)-1) : 0;
            break;
        default: assert(0);
        }
        es->reg_state[Reg_DX] = es->reg_state[Reg_AX];
        if (!csIsStatic(es->reg_state[Reg_AX]))
            capture(c, instr);
        break;

    case IT_CMOVZ: case IT_CMOVNZ:
    case IT_CMOVC: case IT_CMOVNC:
    case IT_CMOVO: case IT_CMOVNO:
    case IT_CMOVS: case IT_CMOVNS:
    {
        FlagType ft;
        Bool cond;
        switch(instr->type) {
        case IT_CMOVZ:  ft = FT_Zero;     cond =  es->flag[ft]; break;
        case IT_CMOVNZ: ft = FT_Zero;     cond = !es->flag[ft]; break;
        case IT_CMOVC:  ft = FT_Carry;    cond =  es->flag[ft]; break;
        case IT_CMOVNC: ft = FT_Carry;    cond = !es->flag[ft]; break;
        case IT_CMOVO:  ft = FT_Overflow; cond =  es->flag[ft]; break;
        case IT_CMOVNO: ft = FT_Overflow; cond = !es->flag[ft]; break;
        case IT_CMOVS:  ft = FT_Sign;     cond =  es->flag[ft]; break;
        case IT_CMOVNS: ft = FT_Sign;     cond = !es->flag[ft]; break;
        default: assert(0);
        }
        assert(opValType(&(instr->src)) == opValType(&(instr->dst)));
        getOpValue(&v1, es, &(instr->src));
        captureCMov(c, instr, es, &v1, es->flag_state[ft], cond);
        // FIXME? if cond state unknown, set destination state always to unknown
        if (cond == True) setOpValue(&v1, es, &(instr->dst));
        break;
    }

    case IT_CMP:
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        vt = opValType(&(instr->dst));
        // sign-extend src/v2 if needed
        if (instr->src.type == OT_Imm8) {
            // sign-extend to 64bit (may be cutoff later)
            v2.val = (int64_t) (int8_t) v2.val;
            v2.type = vt;
        }
        s = setFlagsSub(es, &v1, &v2);
        captureCmp(c, instr, es, s);
        break;

    case IT_DEC:
        getOpValue(&v1, es, &(instr->dst));
        switch(instr->dst.type) {
        case OT_Reg32:
        case OT_Ind32:
            v1.val = (uint32_t)(((int32_t) v1.val) - 1);
            break;

        case OT_Reg64:
        case OT_Ind64:
            v1.val = (uint64_t)(((int64_t) v1.val) - 1);
            break;

        default:assert(0);
        }
        captureUnaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;

    case IT_IMUL:
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        assert(opIsGPReg(&(instr->dst)));
        assert(v1.type == v2.type);
        switch(instr->src.type) {
        case OT_Reg32:
        case OT_Ind32:
            vres.type = VT_32;
            vres.val = (uint64_t) ((int32_t) v1.val * (int32_t) v2.val);
            break;

        case OT_Reg64:
        case OT_Ind64:
            vres.type = VT_64;
            vres.val = (uint64_t) ((int64_t) v1.val * (int64_t) v2.val);
            break;

        default:assert(0);
        }

        // optimization: muliply with static 0 results in static 0
        if ((csIsStatic(v1.state) && (v1.val == 0)) ||
            (csIsStatic(v2.state) && (v2.val == 0)))
            vres.state = CS_STATIC;
        else
            vres.state = combineState(v1.state, v2.state, 0);

        // for capture we need state of dst, do before setting dst
        captureBinaryOp(c, instr, es, &vres);
        setOpValue(&vres, es, &(instr->dst));
        break;

    case IT_IDIV1: {
        uint64_t v, quRes, modRes;

        // FIXME: Set flags!
        getOpValue(&v1, es, &(instr->dst));
        // TODO: raise "division by 0" exception
        assert(v1.val != 0);
        s = combineState(es->reg_state[Reg_DX], es->reg_state[Reg_AX], 1);
        s = combineState(s, v1.state, 0);

        switch(instr->dst.type) {
        case OT_Reg32:
        case OT_Ind32:
            v = (es->reg[Reg_DX] << 32) + (es->reg[Reg_AX] & ((1ul<<32)-1) );
            v1.val = (int32_t) v1.val;
            quRes = v / v1.val;
            assert(quRes < (1u<<31)); // fits into 32 bit (TODO: raise exc)
            modRes = v % v1.val;
            break;

        case OT_Reg64:
        case OT_Ind64:
            // FIXME: should use rdx
            quRes = es->reg[Reg_AX] / v1.val;
            modRes = es->reg[Reg_AX] % v1.val;
            break;

        default:assert(0);
        }

        captureIDiv(c, instr, s, es);

        es->reg[Reg_AX] = quRes;
        es->reg[Reg_DX] = modRes;
        es->reg_state[Reg_DX] = s;
        es->reg_state[Reg_AX] = s;
        break;
    }
    case IT_INC:
        getOpValue(&v1, es, &(instr->dst));
        switch(instr->dst.type) {
        case OT_Reg32:
        case OT_Ind32:
            v1.val = (uint32_t)(((int32_t) v1.val) + 1);
            break;

        case OT_Reg64:
        case OT_Ind64:
            v1.val = (uint64_t)(((int64_t) v1.val) + 1);
            break;

        default:assert(0);
        }
        captureUnaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;

    case IT_JE:
        if (es->flag_state[FT_Zero] != CS_STATIC) {
            captureJcc(c, IT_JE, instr->dst.val, instr->addr + instr->len,
                        es->flag[FT_Zero]);
        }
        if (es->flag[FT_Zero] == True) return instr->dst.val;
        return instr->addr + instr->len;

    case IT_JNE:
        if (es->flag_state[FT_Zero] != CS_STATIC) {
            captureJcc(c, IT_JNE, instr->dst.val, instr->addr + instr->len,
                        !es->flag[FT_Zero]);
        }
        if (es->flag[FT_Zero] == False) return instr->dst.val;
        return instr->addr + instr->len;

    case IT_JLE:
        if ((es->flag_state[FT_Zero] != CS_STATIC) ||
            (es->flag_state[FT_Sign] != CS_STATIC)) {
            captureJcc(c, IT_JLE, instr->dst.val, instr->addr + instr->len,
                        es->flag[FT_Zero] || es->flag[FT_Sign]);
        }
        if ((es->flag[FT_Zero] == True) ||
            (es->flag[FT_Sign] == True)) return instr->dst.val;
        return instr->addr + instr->len;

    case IT_JG:
        if ((es->flag_state[FT_Zero] != CS_STATIC) ||
            (es->flag_state[FT_Sign] != CS_STATIC)) {
            captureJcc(c, IT_JG, instr->dst.val, instr->addr + instr->len,
                       !es->flag[FT_Zero] && !es->flag[FT_Sign]);
        }
        if ((es->flag[FT_Zero] == False) &&
            (es->flag[FT_Sign] == False)) return instr->dst.val;
        return instr->addr + instr->len;

    case IT_JL:
        if ((es->flag_state[FT_Sign] != CS_STATIC) ||
            (es->flag_state[FT_Overflow] != CS_STATIC)) {
            captureJcc(c, IT_JL, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Sign] != es->flag[FT_Overflow]);
        }
        if (es->flag[FT_Sign] != es->flag[FT_Overflow]) return instr->dst.val;
        return instr->addr + instr->len;

    case IT_JGE:
        if ((es->flag_state[FT_Sign] != CS_STATIC) ||
            (es->flag_state[FT_Overflow] != CS_STATIC)) {
            captureJcc(c, IT_JGE, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Sign] == es->flag[FT_Overflow]);
        }
        if (es->flag[FT_Sign] == es->flag[FT_Overflow]) return instr->dst.val;
        return instr->addr + instr->len;

    case IT_JP:
        if (es->flag_state[FT_Parity] != CS_STATIC) {
            captureJcc(c, IT_JP, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Parity]);
        }
        if (es->flag[FT_Parity] == True) return instr->dst.val;
        return instr->addr + instr->len;

    case IT_JMP:
        assert(instr->dst.type == OT_Imm64);

        // address to jump to
        return instr->dst.val;

    case IT_JMPI:
        getOpValue(&v1, es, &(instr->dst));

        switch(instr->dst.type) {
        case OT_Reg64: break;
        case OT_Ind64:
            getOpAddr(&v2, es, &(instr->dst));
            if (csIsStatic(v2.state)) {
                // Assume indirect jump with target at constant address
                // in memory to be constant: follow resolved PLT entries
                v1.state = CS_STATIC;
            }
            break;
        default: assert(0);
        }

        assert(csIsStatic(v1.state));
        return v1.val; // address to jump to

    case IT_LEA:
        switch(instr->dst.type) {
        case OT_Reg32:
        case OT_Reg64:
            assert(opIsInd(&(instr->src)));
            getOpAddr(&v1, es, &(instr->src));
            if (opValType(&(instr->dst)) == VT_32) {
                v1.val = (uint32_t) v1.val;
                v1.type = VT_32;
            }
            captureLea(c, instr, es, &v1);
            // may overwrite a state needed for correct capturing
            setOpValue(&v1, es, &(instr->dst));
            break;

        default:assert(0);
        }
        break;

    case IT_LEAVE: {
        // leave = mov rbp,rsp + pop rbp

        Instr i;
        // mov rbp,rsp
        initSimpleInstr(&i, IT_MOV);
        copyOperand( &(i.src), getRegOp(VT_64, Reg_BP) );
        copyOperand( &(i.dst), getRegOp(VT_64, Reg_SP) );
        getOpValue(&v1, es, &(i.src));
        setOpValue(&v1, es, &(i.dst));
        captureMov(c, &i, es, &v1);
        // pop rbp
        initUnaryInstr(&i, IT_POP, getRegOp(VT_64, Reg_BP));
        addr = emuValue(es->reg[Reg_SP], VT_64, es->reg_state[Reg_SP]);
        getMemValue(&v1, &addr, es, VT_64, 1);
        setOpValue(&v1, es, &(i.dst));
        es->reg[Reg_SP] += 8;
        if (!csIsStatic(v1.state))
            capture(c, &i);
        break;
    }

    case IT_MOV:
    case IT_MOVSX: // converting move
        switch(instr->src.type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Imm32: {
            ValType dst_t = opValType(&(instr->dst));
            assert(dst_t == VT_32 || dst_t == VT_64);
            getOpValue(&v1, es, &(instr->src));
            if (dst_t == VT_64) {
                // also a regular mov may sign-extend: imm32->64
                // assert(instr->type == IT_MOVSX);
                // sign extend lower 32 bit to 64 bit
                v1.val = (int64_t) (int32_t) v1.val;
                v1.type = VT_64;
            }
            captureMov(c, instr, es, &v1);
            setOpValue(&v1, es, &(instr->dst));
            break;
        }

        case OT_Reg64:
        case OT_Ind64:
        case OT_Imm64:
            assert(opValType(&(instr->dst)) == VT_64);
            getOpValue(&v1, es, &(instr->src));
            captureMov(c, instr, es, &v1);
            setOpValue(&v1, es, &(instr->dst));
            break;

        default:assert(0);
        }
        break;

    case IT_NOP:
        // nothing to do
        break;

    case IT_NEG:
        getOpValue(&v1, es, &(instr->dst));
        switch(instr->dst.type) {
        case OT_Reg32:
        case OT_Ind32:
            v1.val = (uint32_t)(- ((int32_t) v1.val));
            break;


        case OT_Reg64:
        case OT_Ind64:
            v1.val = (uint64_t)(- ((int64_t) v1.val));
            break;

        default:assert(0);
        }
        captureUnaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;


    case IT_POP:
        switch(instr->dst.type) {
        case OT_Reg32:
            addr = emuValue(es->reg[Reg_SP], VT_64, es->reg_state[Reg_SP]);
            getMemValue(&v1, &addr, es, VT_32, 1);
            setOpValue(&v1, es, &(instr->dst));
            es->reg[Reg_SP] += 4;
            if (!csIsStatic(v1.state))
                capture(c, instr);
            break;

        case OT_Reg64:
            addr = emuValue(es->reg[Reg_SP], VT_64, es->reg_state[Reg_SP]);
            getMemValue(&v1, &addr, es, VT_64, 1);
            setOpValue(&v1, es, &(instr->dst));
            es->reg[Reg_SP] += 8;
            if (!csIsStatic(v1.state))
                capture(c, instr);
            break;

        default: assert(0);
        }
        break;

    case IT_PUSH:
        switch(instr->dst.type) {
        case OT_Reg32:
        case OT_Imm32:
            es->reg[Reg_SP] -= 4;
            addr = emuValue(es->reg[Reg_SP], VT_64, es->reg_state[Reg_SP]);
            getOpValue(&v1, es, &(instr->dst));
            setMemValue(&v1, &addr, es, VT_32, 1);
            if (!csIsStatic(v1.state))
                capture(c, instr);
            break;

        case OT_Reg64:
            es->reg[Reg_SP] -= 8;
            addr = emuValue(es->reg[Reg_SP], VT_64, es->reg_state[Reg_SP]);
            getOpValue(&v1, es, &(instr->dst));
            setMemValue(&v1, &addr, es, VT_64, 1);
            if (!csIsStatic(v1.state))
                capture(c, instr);
            break;

        default: assert(0);
        }
        break;

    case IT_RET:
        if (c->addInliningHints) {
            Instr i;
            initSimpleInstr(&i, IT_HINT_RET);
            capture(c, &i);
        }

        es->depth--;
        if (es->depth >= 0) {
            // pop return address from stack
            addr = emuValue(es->reg[Reg_SP], VT_64, es->reg_state[Reg_SP]);
            getMemValue(&v1, &addr, es, VT_64, 1);
            es->reg[Reg_SP] += 8;

            assert(v1.val == es->ret_stack[es->depth]);
            return es->ret_stack[es->depth];
        }
        break;

    case IT_SHL:
    case IT_SHR:
    case IT_SAR:
        // FIXME: do flags (shifting into CF, set OF)
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        switch(opValType(&(instr->dst))) {
        case VT_32:
            switch (instr->type) {
            case IT_SHL: v1.val = (uint32_t) (v1.val << (v2.val & 31)); break;
            case IT_SHR: v1.val = (uint32_t) (v1.val >> (v2.val & 31)); break;
            case IT_SAR:
                v1.val = (uint32_t) ((int32_t)v1.val >> (v2.val & 31)); break;
            default: assert(0);
            }
            break;

        case VT_64:
            switch (instr->type) {
            case IT_SHL: v1.val = v1.val << (v2.val & 63); break;
            case IT_SHR: v1.val = v1.val >> (v2.val & 63); break;
            case IT_SAR: v1.val = ((int64_t)v1.val >> (v2.val & 63)); break;
            default: assert(0);
            }
            break;

        default: assert(0);
        }
        v1.state = combineState(v1.state, v2.state, 0);
        captureBinaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;

    case IT_SUB:
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        vt = opValType(&(instr->dst));
        // sign-extend src/v2 if needed
        if (instr->src.type == OT_Imm8) {
            // sign-extend to 64bit (may be cutoff later)
            v2.val = (int64_t) (int8_t) v2.val;
            v2.type = vt;
        }
        else if (instr->src.type == OT_Imm32 && vt == VT_64) {
            // sign-extend to 64bit (may be cutoff later)
            v2.val = (int64_t) (int32_t) v2.val;
            v2.type = vt;
        }

        setFlagsSub(es, &v1, &v2);
        assert(v1.type == v2.type);

        switch(vt) {
        case VT_32:
            v1.val = ((uint32_t) v1.val - (uint32_t) v2.val);
            break;

        case VT_64:
            v1.val = v1.val - v2.val;
            break;

        default: assert(0);
        }

        v1.state = combineState(v1.state, v2.state, 0);
        // for capturing we need state of original dst, do before setting dst
        captureBinaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;

    case IT_TEST:
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        assert(v1.type == v2.type);
        s = setFlagsBit(es, IT_AND, &v1, &v2, False);
        captureTest(c, instr, es, s);
        break;

    case IT_XOR:
    case IT_OR:
    case IT_AND:
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        assert(v1.type == v2.type);
        v1.state = setFlagsBit(es, instr->type, &v1, &v2,
                               opIsEqual(&(instr->dst), &(instr->src)));
        switch(instr->type) {
        case IT_AND: v1.val = v1.val & v2.val; break;
        case IT_XOR: v1.val = v1.val ^ v2.val; break;
        case IT_OR:  v1.val = v1.val | v2.val; break;
        default: assert(0);
        }
        // for capturing we need state of original dst
        captureBinaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;


    default: assert(0);
    }
    return 0;
}



