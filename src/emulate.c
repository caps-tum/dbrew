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
#include "expr.h"
#include "error.h"
#include "vector.h"



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

void initMetaState(MetaState* ms, CaptureState cs)
{
    ms->cState = cs;
    ms->range = 0;
    ms->parDep = 0;
}


static
char captureState2Char(CaptureState cs)
{
    assert((cs >= 0) && (cs < CS_Max));
    assert(CS_Max == 5);
    return "-DSR2"[cs];
}

static
bool csIsStatic(CaptureState cs)
{
    if ((cs == CS_STATIC) || (cs == CS_STATIC2)) return true;
    return false;
}

static
bool msIsStatic(MetaState ms)
{
    if ((ms.cState == CS_STATIC) || (ms.cState == CS_STATIC2)) return true;
    return false;
}

static
bool msIsDynamic(MetaState ms)
{
    if (ms.cState == CS_DYNAMIC) return true;
    return false;
}

static
EmuValue emuValue(uint64_t v, ValType t, MetaState s)
{
    EmuValue ev;
    ev.val = v;
    ev.type = t;
    ev.state = s;

    return ev;
}

static
EmuValue staticEmuValue(uint64_t v, ValType t)
{
    EmuValue ev;
    ev.val = v;
    ev.type = t;
    initMetaState(&(ev.state), CS_STATIC);

    return ev;
}

void resetEmuState(EmuState* es)
{
    int i;
    static RegIndex calleeSave[] = {
        RI_BP, RI_B, RI_12, RI_13, RI_14, RI_15, RI_None
    };

    es->parent = 0;

    for(i=0; i < RI_GPMax; i++) {
        es->reg[i] = 0;
        initMetaState(&(es->reg_state[i]), CS_DEAD);
    }

    for(i=0; i<FT_Max; i++) {
        es->flag[i] = false;
        initMetaState(&(es->flag_state[i]), CS_DEAD);
    }

    for(i=0; i< es->stackSize; i++)
        es->stack[i] = 0;
    for(i=0; i< es->stackSize; i++)
        initMetaState(&(es->stackState[i]), CS_DEAD);

    // use real addresses for now
    es->stackStart = (uint64_t) es->stack;
    es->stackTop = es->stackStart + es->stackSize;
    es->stackAccessed = es->stackTop;

    // calling convention:
    //  rbp, rbx, r12-r15 have to be preserved by callee
    for(i=0; calleeSave[i] != RI_None; i++)
        initMetaState(&(es->reg_state[calleeSave[i]]), CS_DYNAMIC);
    // RIP always known
    initMetaState(&(es->regIP_state), CS_STATIC);

    es->depth = 0;
}

EmuState* allocEmuState(int size)
{
    EmuState* es;

    es = (EmuState*) malloc(sizeof(EmuState));
    es->stackSize = size;
    es->stack = (uint8_t*) malloc(size);
    es->stackState = (MetaState*) malloc(sizeof(MetaState) * size);

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
static
bool csIsEqual(EmuState* es1, CaptureState s1, uint64_t v1,
               EmuState* es2, CaptureState s2, uint64_t v2)
{
    // normalize meta states: CS_STATIC2 is equivalent to CS_STATIC
    if (s1 == CS_STATIC2) s1 = CS_STATIC;
    if (s2 == CS_STATIC2) s2 = CS_STATIC;
    // DEAD means "not initialized". This is different from DYNAMIC!

    if (s1 != s2) return false;

    // both have same meta-state
    switch(s1) {
    case CS_STATIC:
        // for static capture states, values have to be equal
        return (v1 == v2);

    case CS_STACKRELATIVE:
        // FIXME: in reality: same offset from a versioned anchor
        // for now: assume same anchor version (within same rewriting action)
        if (es1->parent != es2->parent) return false;
        return (v1 == v2);

    default:
        // any two DEAD registers are equal
        break;
    }
    return true;
}

// states are equal if metainformation is equal and static data is the same
static
bool esIsEqual(EmuState* es1, EmuState* es2)
{
    int i;

    // same state for registers?
    for(i = 0; i < RI_GPMax; i++) {
        if (!csIsEqual(es1, es1->reg_state[i].cState, es1->reg[i],
                       es2, es2->reg_state[i].cState, es2->reg[i]))
            return false;
    }

    // same state for flag registers?
    for(i = 0; i < FT_Max; i++) {
        if (!csIsEqual(es1, es1->flag_state[i].cState, es1->flag[i],
                       es2, es2->flag_state[i].cState, es2->flag[i]))
            return false;
    }

    // for equality, must be at same call depth
    if (es1->depth != es2->depth) return false;

    // Stack
    // all known data has to be the same
    if (es1->stackSize < es2->stackSize) {
        int diff = es2->stackSize - es1->stackSize;
        // stack of es2 is larger: bottom should not be static
        for(i = 0; i < diff; i++) {
            if (msIsStatic(es2->stackState[i]))
                return false;
        }
        // check for equal state at byte granularity
        for(i = 0; i < es1->stackSize; i++) {
            if (!csIsEqual(es1, es1->stackState[i].cState, es1->stack[i],
                           es2, es2->stackState[i+diff].cState, es2->stack[i+diff]))
                return false;
        }
    }
    else {
        // es1 larger
        int diff = es1->stackSize - es2->stackSize;
        // bottom of es1 should not be static
        for(i = 0; i < diff; i++) {
            if (msIsStatic(es1->stackState[i]))
                return false;
        }
        // check for equal state at byte granularity
        for(i = 0; i < es2->stackSize; i++) {
            if (!csIsEqual(es1, es1->stackState[i+diff].cState, es1->stack[i+diff],
                           es2, es2->stackState[i].cState, es2->stack[i]))
                return false;
        }
    }

    return true;
}

static
void copyEmuState(EmuState* dst, EmuState* src)
{
    int i;

    dst->parent = src->parent;

    for(i=0; i < RI_GPMax; i++) {
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
            initMetaState(&(dst->stackState[i]), CS_DEAD);
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
        uint64_t diff = src->stackSize - dst->stackSize;
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

static
EmuState* cloneEmuState(EmuState* src)
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
int saveEmuState(RContext* c)
{
    static Error e;
    int i;
    Rewriter* r = c->r;

    if (r->showEmuSteps)
        printf("Saving current emulator state: ");
    //printStaticEmuState(r->es, -1);
    for(i = 0; i < r->savedStateCount; i++) {
        //printf("Check ES %d\n", i);
        //printStaticEmuState(r->savedState[i], i);
        if (esIsEqual(r->es, r->savedState[i])) {
            if (r->showEmuSteps)
                printf("already existing, esID %d\n", i);
            return i;
        }
    }
    if (r->showEmuSteps)
        printf("new with esID %d\n", i);
    if (i >= SAVEDSTATE_MAX) {
        setError(&e, ET_BufferOverflow, EM_Rewriter, r,
                 "Too many different emulation states");
        c->e = &e;
        return -1;
    }
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

static
const char* flagName(int f)
{
    switch(f) {
    case FT_Zero:     return "ZF";
    case FT_Carry:    return "CF";
    case FT_Sign:     return "SF";
    case FT_Overflow: return "OF";
    case FT_Parity:   return "PF";
    default: assert(0);
    }
    return "(unknown)";
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
    for(i=0; i < RI_GPMax; i++) {
        MetaState* ms = &(es->reg_state[i]);
        printf("    %%%-3s = 0x%016lx %c",
               regNameI(RT_GP64, (RegIndex)i),
               es->reg[i], captureState2Char( ms->cState ));
        if (ms->range)
            printf(", range %s", expr_toString(ms->range));
        if (ms->parDep)
            printf(", parDep %s", expr_toString(ms->parDep));
        printf("\n");
    }
    printf("    %%%-3s = 0x%016lx %c\n", "rip",
           es->regIP, captureState2Char( es->regIP_state.cState ));

    printf("  Flags: ");
    for(i = 0; i < FT_Max; i++) {
        if (i>0) printf("  ");
        printf("%s %d %c", flagName(i), es->flag[i],
               captureState2Char(es->flag_state[i].cState));
    }
    printf("\n");

    spOff = es->reg[RI_SP] - es->stackStart;
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
                       captureState2Char(es->stackState[oo].cState));
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
    for(i = 0; i < RI_GPMax; i++) {
        if (es->reg_state[i].cState == CS_DEAD) continue;
        if (es->reg_state[i].cState == CS_DYNAMIC) continue;

        if (c>0) printf(", ");
        switch(es->reg_state[i].cState) {
        case CS_STATIC:
        case CS_STATIC2:
            printf("%%%s (0x%lx)",
                   regNameI(RT_GP64, (RegIndex)i), es->reg[i]);
            break;
        case CS_STACKRELATIVE:
            printf("%%%s (R %ld)",
                   regNameI(RT_GP64, (RegIndex)i), es->reg[i] - es->stackTop);
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
        if (!msIsStatic(es->flag_state[i])) continue;
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
        if (!msIsStatic(es->stackState[i])) {
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

static
CaptureState combineState(CaptureState s1, CaptureState s2,
                          bool isSameValue)
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

static
CaptureState combineState4Flags(CaptureState s1, CaptureState s2)
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
static
CBB *findCaptureBB(Rewriter* r, uint64_t f, int esID)
{
    int i;

    for(i = 0; i < r->capBBCount; i++)
        if ((r->capBB[i].dec_addr == f) && (r->capBB[i].esID == esID))
            return &(r->capBB[i]);

    return 0;
}

// allocate a BB structure to collect instructions for capturing
CBB* getCaptureBB(RContext* c, uint64_t f, int esID)
{
    CBB* bb;
    Rewriter* r = c->r;

    // already captured?
    bb = findCaptureBB(r, f, esID);
    if (bb) return bb;

    // start capturing of new BB beginning at f
    if (r->capBBCount >= r->capBBCapacity) {
        static Error e;
        setError(&e, ET_BufferOverflow, EM_Rewriter, r,
                 "Too many captured blocks");
        c->e = &e;
        return 0;
    }
    bb = &(r->capBB[r->capBBCount]);
    r->capBBCount++;
    bb->dec_addr = f;
    bb->esID = esID;
    bb->fc = config_find_function(r, f);

    bb->count = 0;
    bb->instr = 0; // updated on first instruction added
    bb->nextBranch = 0;
    bb->nextFallThrough = 0;
    bb->endType = IT_None;
    bb->preferBranch = false;

    bb->size = -1; // size of 0 could be valid
    bb->addr1 = 0;
    bb->addr2 = 0;
    bb->genJcc8 = false;
    bb->genJump = false;

    bb->generatorData = 0;

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

    if (bb->esID >=0)
        sprintf(buf+off, "|%d", bb->esID);

    return buf;
}

int pushCaptureBB(RContext* c, CBB* bb)
{
    Rewriter* r = c->r;
    if (r->capStackTop + 1 >= CAPTURESTACK_LEN) {
        static Error e;
        setError(&e, ET_BufferOverflow, EM_Rewriter, r,
                 "Too many blocks on capture stack");
        c->e = &e;
        return -1;
    }
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

Instr* newCapInstr(RContext* c)
{
    Instr* instr;
    Rewriter* r = c->r;

    if (r->capInstrCount >= r->capInstrCapacity) {
        static Error e;
        setError(&e, ET_BufferOverflow, EM_Capture, r,
                 "Too many captured instructions");
        c->e = &e;
        return 0;
    }
    instr = r->capInstr + r->capInstrCount;
    r->capInstrCount++;

    return instr;
}

// capture a new instruction
void capture(RContext* c, Instr* instr)
{
    Instr* newInstr;
    Rewriter* r = c->r;
    CBB* cbb = r->currentCapBB;
    if (cbb == 0) return;

    if (r->showEmuSteps)
        printf("Capture '%s' (into %s + %d)\n",
               instr2string(instr, 0, 0), cbb_prettyName(cbb), cbb->count);

    newInstr = newCapInstr(c);
    if (c->e) return;
    if (cbb->instr == 0) {
        cbb->instr = newInstr;
        assert(cbb->count == 0);
    }
    copyInstr(newInstr, instr);
    cbb->count++;
}

// clone a decoded BB as a CBB
//
// By-pass emulation by directly forwarding all decoded instructions
// from a given DBB to newly created CBB. Used for debugging/testing.
CBB* createCBBfromDBB(Rewriter* r, DBB* src)
{
    RContext cxt;
    cxt.r = r;
    cxt.e = 0;

    // a CBB starting from an undefined emulator state (esID -1)
    CBB* cbb = getCaptureBB(&cxt, (uint64_t) src->addr, -1);
    r->currentCapBB = cbb;
    for(int i = 0; i < src->count; i++)
        capture(&cxt, src->instr + i);
    r->currentCapBB = 0;
    return cbb;
}


//---------------------------------------------------------
// emulator functions


// flag setting helpers

/* Setting some flags can get complicated.
 * From libx86emu/prim_ops.c (github.com/wfeldt/libx86emu)
 */
static
uint32_t parity_tab[8] =
{
    0x96696996, 0x69969669, 0x69969669, 0x96696996,
    0x69969669, 0x96696996, 0x96696996, 0x69969669,
};

#define PARITY(x)   (((parity_tab[(x) / 32] >> ((x) % 32)) & 1) == 0)
#define XOR2(x)     (((x) ^ ((x)>>1)) & 0x1)


static
void setFlagsState(EmuState* es, int flagSet, CaptureState cs)
{
    if (flagSet & FS_Carry)
        initMetaState(&(es->flag_state[FT_Carry]   ), cs);
    if (flagSet & FS_Zero)
        initMetaState(&(es->flag_state[FT_Zero]    ), cs);
    if (flagSet & FS_Sign)
        initMetaState(&(es->flag_state[FT_Sign]    ), cs);
    if (flagSet & FS_Overflow)
        initMetaState(&(es->flag_state[FT_Overflow]), cs);
    if (flagSet & FS_Parity)
        initMetaState(&(es->flag_state[FT_Parity]  ), cs);
}


// set flags for operation "v1 - v2"
static
CaptureState setFlagsSub(EmuState* es, EmuValue* v1, EmuValue* v2)
{
    CaptureState st;
    uint64_t r, bc, d, s;

    st = combineState4Flags(v1->state.cState, v2->state.cState);
    setFlagsState(es, FS_CZSOP, st);

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
static
void setFlagsAdd(EmuState* es, EmuValue* v1, EmuValue* v2)
{
    CaptureState st;
    uint64_t r, cc, d, s;

    st = combineState4Flags(v1->state.cState, v2->state.cState);
    setFlagsState(es, FS_CZSOP, st);

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
static
CaptureState setFlagsBit(EmuState* es, InstrType it,
                         EmuValue* v1, EmuValue* v2, bool sameOperands)
{

    CaptureState s;
    uint64_t res;

    assert(v1->type == v2->type);

    s = combineState4Flags(v1->state.cState, v2->state.cState);
    // xor op,op results in known zero
    if ((it == IT_XOR) && sameOperands) s = CS_STATIC;

    // carry/overflow always cleared
    es->flag[FT_Carry] = 0;
    es->flag[FT_Overflow] = 0;
    setFlagsState(es, FS_CO, CS_STATIC);

    setFlagsState(es, FS_ZSP, s);

    switch(it) {
    case IT_AND: res = v1->val & v2->val; break;
    case IT_XOR: res = v1->val ^ v2->val; break;
    case IT_OR:  res = v1->val | v2->val; break;
    default: assert(0);
    }

    es->flag[FT_Parity] = PARITY(res & 0xff);
    switch(v1->type) {
    case VT_8:
        es->flag[FT_Zero] = ((res & ((1<<8)-1)) == 0);
        es->flag[FT_Sign] = ((res & (1l<<7)) != 0);
        break;
    case VT_32:
        es->flag[FT_Zero] = ((res & ((1l<<32)-1)) == 0);
        es->flag[FT_Sign] = ((res & (1l<<31)) != 0);
        break;
    case VT_64:
        es->flag[FT_Zero] = (res == 0);
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
static
bool getStackOffset(EmuState* es, EmuValue* addr, EmuValue* off)
{
    if ((addr->val >= es->stackStart) && (addr->val < es->stackTop)) {
        CaptureState s;

        off->type = VT_32;
        off->val = addr->val - es->stackStart;
        s = (addr->state.cState == CS_STACKRELATIVE) ? CS_STATIC : CS_DYNAMIC;
        initMetaState(&(off->state), s);
        return true;
    }
    return false;
}

static
CaptureState getStackState(EmuState* es, EmuValue* off)
{
    CaptureState cs;

    cs = CS_DYNAMIC;
    if (off->state.cState == CS_STATIC) {
        if (off->val >= (uint64_t) es->stackSize) cs = CS_DEAD;
        if (off->val < es->stackAccessed - es->stackStart) cs = CS_DEAD;
        else
            return es->stackState[off->val].cState;
    }
    return cs;
}

static
void getStackValue(EmuState* es, EmuValue* v, EmuValue* off)
{
    int i, count;
    CaptureState state;

    assert(off->val < (uint64_t) es->stackSize);

    switch(v->type) {
    case VT_16:
        v->val = *(uint16_t*) (es->stack + off->val);
        count = 2;
        break;

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

    if (off->state.cState == CS_STATIC) {
        state = getStackState(es, off);
        for(i=1; i<count; i++)
            state = combineState(state, es->stackState[off->val + i].cState, 1);
    }
    else
        state = CS_DYNAMIC;

    initMetaState(&(v->state), state);
}


static
void setStackValue(EmuState* es, EmuValue* v, EmuValue* off)
{
    uint16_t* a16;
    uint32_t* a32;
    uint64_t* a64;
    int i, count;

    switch(v->type) {
    case VT_16:
        a16 = (uint16_t*) (es->stack + off->val);
        *a16 = (uint16_t) v->val;
        count = 2;
        break;

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

    if (off->state.cState == CS_STATIC) {
        for(i=0; i<count; i++)
            es->stackState[off->val + i] = v->state;
    }

    if (es->stackStart + off->val < es->stackAccessed)
        es->stackAccessed = es->stackStart + off->val;
}

static
void getRegValue(EmuValue* v, EmuState* es, Reg r, ValType t)
{
    assert(regIsGP(r));
    assert(regValType(r) == t);
    v->type = t;
    v->val = es->reg[r.ri];
    v->state = es->reg_state[r.ri];
}

static
void getMemValue(EmuValue* v, EmuValue* addr, EmuState* es, ValType t,
                 bool shouldBeStack)
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
    initMetaState(&(v->state), CS_DYNAMIC);
    // explicit request to make memory access result static
    if (addr->state.cState == CS_STATIC2) v->state.cState = CS_STATIC2;

    v->type = t;
    switch(t) {
    case VT_8:  v->val = *(uint8_t*) addr->val; break;
    case VT_16: v->val = *(uint16_t*) addr->val; break;
    case VT_32: v->val = *(uint32_t*) addr->val; break;
    case VT_64: v->val = *(uint64_t*) addr->val; break;
    default: assert(0);
    }
}

// reading memory using segment override (fs/gs)
static
void getSegMemValue(EmuValue* v, EmuValue* addr, ValType t, OpSegOverride s)
{
    assert(s != OSO_None);
    uint32_t v32;
    uint8_t v8;

    // memory accessed via fs/gs always is dynamic
    initMetaState(&(v->state), CS_DYNAMIC);
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

static
void setMemValue(EmuValue* v, EmuValue* addr, EmuState* es, ValType t,
                 int shouldBeStack)
{
    EmuValue off;
    uint16_t* a16;
    uint32_t* a32;
    uint64_t* a64;
    bool isOnStack;

    assert(v->type == t);
    isOnStack = getStackOffset(es, addr, &off);
    if (isOnStack) {
        setStackValue(es, v, &off);
        return;
    }

    assert(!shouldBeStack);

    switch(t) {
    case VT_16:
        a16 = (uint16_t*) addr->val;
        *a16 = (uint16_t) v->val;

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
static
void addRegToValue(EmuValue* v, EmuState* es, Reg r, int scale)
{
    if (r.rt == RT_None) return;
    assert(r.rt == RT_GP64);

    v->state.cState = combineState(v->state.cState,
                                   es->reg_state[r.ri].cState, 0);
    v->val += scale * es->reg[r.ri];
}

// get resulting address (and state) for memory operands
// this cannot be used with fs/gs segment override
static
void getOpAddr(EmuValue* v, EmuState* es, Operand* o)
{
    assert(opIsInd(o));
    assert(o->seg == OSO_None);

    v->type = VT_64;
    v->val = o->val;
    initMetaState(&(v->state), CS_STATIC);

    if (o->reg.rt != RT_None)
        addRegToValue(v, es, o->reg, 1);

    if (o->scale > 0)
        addRegToValue(v, es, o->ireg, o->scale);
}

// returned value v should be casted to expected type (8/16/32 bit)
static
void getOpValue(EmuValue* v, EmuState* es, Operand* o)
{
    EmuValue addr;

    switch(o->type) {
    case OT_Imm8:
    case OT_Imm16:
    case OT_Imm32:
    case OT_Imm64:
        *v = staticEmuValue(o->val, opValType(o));
        return;

    case OT_Reg8:
        v->type = VT_8;
        assert(regValType(o->reg) == VT_8);
        v->val = (uint8_t) es->reg[o->reg.ri];
        v->state = es->reg_state[o->reg.ri];
        return;

    case OT_Reg16:
        v->type = VT_16;
        assert(regValType(o->reg) == VT_16);
        v->val = (uint16_t) es->reg[o->reg.ri];
        v->state = es->reg_state[o->reg.ri];
        return;

    case OT_Reg32:
        v->type = VT_32;
        assert(regValType(o->reg) == VT_32);
        v->val = (uint32_t) es->reg[o->reg.ri];
        v->state = es->reg_state[o->reg.ri];
        return;

    case OT_Reg64:
        v->type = VT_64;
        assert(regValType(o->reg) == VT_64);
        v->val = (uint64_t) es->reg[o->reg.ri];
        v->state = es->reg_state[o->reg.ri];
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
static
void setOpValue(EmuValue* v, EmuState* es, Operand* o)
{
    EmuValue addr;

    assert(v->type == opValType(o));
    switch(o->type) {
    case OT_Reg8:
        assert(regValType(o->reg) == VT_8);
        es->reg[o->reg.ri] = (uint8_t) v->val;
        es->reg_state[o->reg.ri] = v->state;
        return;

    case OT_Reg16:
        assert(regValType(o->reg) == VT_16);
        es->reg[o->reg.ri] = (uint16_t) v->val;
        es->reg_state[o->reg.ri] = v->state;
        return;

    case OT_Reg32:
        assert(regValType(o->reg) == VT_32);
        es->reg[o->reg.ri] = (uint32_t) v->val;
        es->reg_state[o->reg.ri] = v->state;
        return;

    case OT_Reg64:
        assert(regValType(o->reg) == VT_64);
        es->reg[o->reg.ri] = v->val;
        es->reg_state[o->reg.ri] = v->state;
        return;

    case OT_Ind32:
    case OT_Ind64:
        getOpAddr(&addr, es, o);
        setMemValue(v, &addr, es, opValType(o), 0);
        return;

    default: assert(0);
    }
}

// Do we track the state for a value pointed to by an operand?
// Returns false for memory locations not on stack or when the stack offset
//  is not static/known.
static
bool opStateIsTracked(EmuState* es, Operand* o)
{
    EmuValue addr;
    EmuValue off;
    bool isOnStack;

    // never should be called with immediate ops (do not point to location)
    assert(!opIsImm(o));
    if (opIsGPReg(o)) return 1;

    getOpAddr(&addr, es, o);
    isOnStack = getStackOffset(es, &addr, &off);
    if (!isOnStack) return 0;
    return msIsStatic(off.state);
}

// apply known state to memory operand (this modifies the operand in-place)
static
void applyStaticToInd(Operand* o, EmuState* es)
{
    if (!opIsInd(o)) return;

    if ((o->reg.rt == RT_GP64) && msIsStatic(es->reg_state[o->reg.ri])) {
        o->val += es->reg[o->reg.ri];
        o->reg.rt = RT_None;
    }
    if ((o->reg.rt == RT_IP) && msIsStatic(es->regIP_state)) {
        o->val += es->regIP;
        o->reg.rt = RT_None;
    }

    if (o->scale > 0) {
        assert(o->ireg.rt == RT_GP64);
        if (msIsStatic(es->reg_state[o->ireg.ri])) {
            o->val += o->scale * es->reg[o->ireg.ri];
            o->scale = 0;
        }
    }
}


// capture processing for instruction types

// both MOV and MOVSX (sign extend 32->64)
static
void captureMov(RContext* c, Instr* orig, EmuState* es, EmuValue* res)
{
    Instr i;
    Operand *o;

    // data movement from orig->src to orig->dst, value is res

    if (res->state.cState == CS_DEAD) return;

    o = &(orig->src);
    if (msIsStatic(res->state)) {
        // no need to update data if capture state is maintained
        if (opStateIsTracked(es, &(orig->dst))) return;

        // source is static, use immediate
        o = getImmOp(res->type, res->val);
    }
    initBinaryInstr(&i, orig->type, orig->vtype, &(orig->dst), o);
    applyStaticToInd(&(i.dst), es);
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

static
void captureCMov(RContext* c, Instr* orig, EmuState* es,
                 EmuValue* res, MetaState cState, bool cond)
{
    Instr i;

    // data movement from orig->src to orig->dst, value is res
    // but only if condition is true

    // cmov always has a register as destination
    assert(opIsReg(&(orig->dst)));

    if (msIsStatic(cState)) {
        if (cond) {
            // use IT_MOV instead of IT_CMOVcc as orig instruction
            initBinaryInstr(&i, IT_MOV, orig->vtype,
                            &(orig->dst), &(orig->src));
            captureMov(c, &i, es, res);
        }
        return;
    }
    // condition state is unknown

    if (res->state.cState == CS_DEAD) return;

    if (msIsStatic(res->state)) {
        // we need to be prepared that there may be a move happening
        // need to update source with known value as it may be moved
        initBinaryInstr(&i, IT_MOV, res->type,
                        &(orig->src), getImmOp(res->type, res->val));
        applyStaticToInd(&(i.src), es);
        capture(c, &i);

        // resulting value becomes unknown, even if source was static
        initMetaState(&(res->state), CS_DYNAMIC);
    }
    initBinaryInstr(&i, orig->type, orig->vtype, &(orig->dst), &(orig->src));
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

static
void captureIDiv(RContext* c, Instr* orig, CaptureState resCState, EmuState* es)
{
    Instr i;
    EmuValue v;

    // capture if not static
    if (csIsStatic(resCState)) return;


    getOpValue(&v, es, &(orig->dst));
    if (msIsStatic(v.state)) {
        // divide by 1 can be skipped
        if (v.val == 1) return;

        initBinaryInstr(&i, IT_MOV, v.type,
                        &(orig->dst), getImmOp(v.type, v.val));
        applyStaticToInd(&(i.dst), es); // dst may be memory on stack!
        capture(c, &i);
    }

    // update ax/dx with known values, use type from idiv operand
    if (msIsStatic(es->reg_state[RI_A])) {
        Reg reg = getReg(getGPRegType(v.type), RI_A);
        initBinaryInstr(&i, IT_MOV, v.type,
                        getRegOp(reg), getImmOp(v.type, es->reg[RI_A]));
        capture(c, &i);
    }
    if (msIsStatic(es->reg_state[RI_D])) {
        Reg reg = getReg(getGPRegType(v.type), RI_D);
        initBinaryInstr(&i, IT_MOV, v.type,
                        getRegOp(reg), getImmOp(v.type, es->reg[RI_D]));
        capture(c, &i);
    }

    initUnaryInstr(&i, orig->type, &(orig->dst));
    applyStaticToInd(&(i.dst), es);
    capture(c, &i);
}

// dst = dst op src
static
void captureBinaryOp(RContext* c, Instr* orig, EmuState* es, EmuValue* res)
{
    EmuValue opval;
    Instr i;
    Operand *o;

    if (res->state.cState == CS_DEAD) return;

    if (msIsStatic(res->state)) {
        // force results to become unknown?
        if (c->r->cc->force_unknown[es->depth]) {
            initMetaState(&(res->state), CS_DYNAMIC);
        }
        else {
            // no need to update data if capture state is maintained
            if (opStateIsTracked(es, &(orig->dst))) return;
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
    if (opStateIsTracked(es, &(orig->dst)) && msIsStatic(opval.state)) {

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
    if (msIsStatic(opval.state)) {
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
static
void captureUnaryOp(RContext* c, Instr* orig, EmuState* es, EmuValue* res)
{
    Instr i;

    if (msIsStatic(res->state)) return;

    initUnaryInstr(&i, orig->type, &(orig->dst));
    applyStaticToInd(&(i.dst), es);
    capture(c, &i);
}

static
void captureLea(RContext* c, Instr* orig, EmuState* es, EmuValue* res)
{
    Instr i;

    assert(opIsReg(&(orig->dst)));
    if (msIsStatic(res->state)) {
        if (c->r->cc->force_unknown[es->depth]) {
            // force results to become unknown => load value into dest

            initMetaState(&(res->state), CS_DYNAMIC);
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

static
void captureCmp(RContext* c, Instr* orig, EmuState* es, CaptureState cs)
{
    EmuValue opval;
    Instr i;
    Operand *o;

    if (csIsStatic(cs)) return;

    getOpValue(&opval, es, &(orig->dst));
    if (msIsStatic(opval.state)) {
        // cannot replace dst with imm: no such encoding => update dst
        initBinaryInstr(&i, IT_MOV, opval.type,
                        &(orig->dst), getImmOp(opval.type, opval.val));
        capture(c, &i);
    }

    o = &(orig->src);
    getOpValue(&opval, es, &(orig->src));
    if (msIsStatic(opval.state))
        o = getImmOp(opval.type, opval.val);

    initBinaryInstr(&i, IT_CMP, orig->vtype, &(orig->dst), o);
    applyStaticToInd(&(i.dst), es);
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

static
void captureTest(RContext* c, Instr* orig, EmuState* es, CaptureState cs)
{
    Instr i;

    if (csIsStatic(cs)) return;

    initBinaryInstr(&i, IT_TEST, orig->vtype, &(orig->dst), &(orig->src));
    applyStaticToInd(&(i.dst), es);
    applyStaticToInd(&(i.src), es);
    capture(c, &i);
}

void captureRet(RContext* c, Instr* orig, EmuState* es)
{
    EmuValue v;
    Instr i;

    // when returning an integer: if AX state is static, load constant
    if (!c->r->cc->hasReturnFP) {
        Reg reg = getReg(getGPRegType(VT_64), RI_A);
        getRegValue(&v, es, reg, VT_64);
        if (msIsStatic(v.state)) {
            initBinaryInstr(&i, IT_MOV, VT_64,
                            getRegOp(reg), getImmOp(v.type, v.val));
            capture(c, &i);
        }
    }
    capture(c, orig);
}

// helper for capturePassThrough: do capture state modifications
// if provided as meta information (e.g. setting values in locations unknown)
static
void processPassThrough(Instr* i, EmuState* es)
{
    assert(i->ptLen >0);
    if (i->ptSChange == SC_None) return;

    switch(i->dst.type) {
    case OT_Reg32:
    case OT_Reg64:
        if (opIsGPReg(&(i->dst)))
            initMetaState(&(es->reg_state[i->dst.reg.ri]), CS_DYNAMIC);
        break;

        // memory locations not handled yet

    default: assert(0);
    }
}

static
void captureVec(RContext* c, Instr* orig, EmuState* es)
{
    Instr i;
    OperandEncoding oe = OE_None;

    switch(orig->type) {
    case IT_ADDSS:
    case IT_ADDSD:
    case IT_ADDPS:
    case IT_ADDPD:
        oe = OE_RM;
        break;
    default: assert(0);
    }

    // we need to apply static information to memory addressing

    initSimpleInstr(&i, orig->type);
    i.vtype  = orig->vtype;
    i.form   = orig->form;

    switch(oe) {
    case OE_MR:
        assert(opIsReg(&(orig->dst)) || opIsInd(&(orig->dst)));
        assert(opIsReg(&(orig->src)));

        copyOperand( &(i.dst), &(orig->dst));
        copyOperand( &(i.src), &(orig->src));
        applyStaticToInd(&(i.dst), es);
        break;

    case OE_RM:
        assert(opIsReg(&(orig->src)) || opIsInd(&(orig->src)));
        assert(opIsReg(&(orig->dst)));

        copyOperand( &(i.dst), &(orig->dst));
        copyOperand( &(i.src), &(orig->src));
        applyStaticToInd(&(i.src), es);
        break;

    default: assert(0);
    }
    capture(c, &i);
}


static
void capturePassThrough(RContext* c, Instr* orig, EmuState* es)
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
    i.ptVexP = orig->ptVexP;
    for(int j=0; j<orig->ptLen; j++)
        i.ptOpc[j] = orig->ptOpc[j];

    switch(orig->ptEnc) {
    case OE_None:
        break;

    case OE_MR:
        assert(opIsReg(&(orig->dst)) || opIsInd(&(orig->dst)));
        assert(opIsReg(&(orig->src)));

        i.form = OF_2;
        copyOperand( &(i.dst), &(orig->dst));
        copyOperand( &(i.src), &(orig->src));
        applyStaticToInd(&(i.dst), es);
        break;

    case OE_RVM:
        // used with AVX, 2nd operand is vector register
        assert(opIsReg(&(orig->dst)));
        assert(opIsReg(&(orig->src)));
        assert(opIsReg(&(orig->src2)) || opIsInd(&(orig->src2)));

        i.form = OF_3;
        copyOperand( &(i.dst), &(orig->dst));
        copyOperand( &(i.src), &(orig->src));
        copyOperand( &(i.src2), &(orig->src2));
        applyStaticToInd(&(i.src2), es);
        break;

    case OE_RM:
        assert(opIsReg(&(orig->dst)));
        assert(opIsReg(&(orig->src)) || opIsInd(&(orig->src)));

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
static
void captureJcc(RContext* c, InstrType it,
                uint64_t branchTarget, uint64_t fallthroughTarget,
                bool didBranch)
{
    CBB *cbb, *cbbBR, *cbbFT;
    int esID;
    Rewriter* r = c->r;

    // do not end BB and assume jump fixed?
    if (r->cc->branches_known) return;

    cbb = popCaptureBB(r);
    cbb->endType = it;
    // use observed behavior from trace as hint for code generation
    cbb->preferBranch = didBranch;

    esID = saveEmuState(c);
    cbbFT = getCaptureBB(c, fallthroughTarget, esID);
    cbbBR = getCaptureBB(c, branchTarget, esID);
    if (c->e) return;

    cbb->nextFallThrough = cbbFT;
    cbb->nextBranch = cbbBR;

    // entry pushed last will be processed first
    if (didBranch) {
        pushCaptureBB(c, cbbFT);
        pushCaptureBB(c, cbbBR);
    }
    else {
        pushCaptureBB(c, cbbBR);
        pushCaptureBB(c, cbbFT);
    }
    if (c->e) return;

    // current CBB should be closed.
    // (we have to open a new CBB before allowing new instructions to capture)
    assert(r->currentCapBB == 0);
}


//----------------------------------------------------------
// Emulator for instruction types

static
void setEmulatorError(RContext* c, Instr* instr,
                      ErrorType et, const char* d)
{
    static Error e;
    static char buf[100];

    if (d == 0) {
        d = buf;
        if (et == ET_UnsupportedInstr)
            sprintf(buf, "Instruction not implemented for %s",
                    instr2string(instr, 0, 0));
        else if (et == ET_UnsupportedOperands)
            sprintf(buf, "Operand types not implemented for %s",
                    instr2string(instr, 0, 0));
        else
            d = 0;
    }
    setError(&e, et, EM_Emulator, c->r, d);
    c->e = &e;
}

static
void emulateRet(RContext* c, Instr* instr)
{
    Rewriter* r = c->r;
    EmuState* es = c->r->es;

    // all caller-save / parameter registers become dead, but not return
    // TODO: includes vector registers xmm1-15
    static RegIndex ri[8] =
    { RI_DI, RI_SI, RI_D, RI_C, RI_8, RI_9, RI_10, RI_11 };
    for(int i=0; i<8; i++)
        initMetaState(&(es->reg_state[ri[i]]), CS_DEAD);

    if (r->addInliningHints) {
        Instr i;
        initSimpleInstr(&i, IT_HINT_RET);
        capture(c, &i);
    }

    es->depth--;
    if (es->depth >= 0) {
        EmuValue v, addr;

        // pop return address from stack
        addr = emuValue(es->reg[RI_SP], VT_64, es->reg_state[RI_SP]);
        getMemValue(&v, &addr, es, VT_64, 1);
        es->reg[RI_SP] += 8;

        if (v.val != es->ret_stack[es->depth]) {
            setEmulatorError(c, instr,
                             ET_BadOperands, "Return address modified");
            return;
        }
        // return to address
        c->exit = es->ret_stack[es->depth];
    }
}

// process an instruction
// if this changes control flow, c.exit is set accordingly
void processInstr(RContext* c, Instr* instr)
{
    EmuValue vres, v1, v2, addr;
    CaptureState cs;
    ValType vt;

    Rewriter* r = c->r;
    EmuState* es = c->r->es;

    if (instr->ptLen > 0) {
        // memory addressing in captured instructions depends on emu state
        capturePassThrough(c, instr, es);
        return;
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
        assert(v1.type == v2.type); // should not happen, internal error

        switch(vt) {
        case VT_32:
            v1.val = ((uint32_t) v1.val + (uint32_t) v2.val);
            break;

        case VT_64:
            v1.val = v1.val + v2.val;
            break;

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }

        cs = combineState(v1.state.cState, v2.state.cState, 0);
        initMetaState(&(v1.state), cs);
        // for capture we need state of original dst, do it before setting dst
        captureBinaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;

    case IT_CALL: {
        // TODO: keep call. For now, we always inline
        getOpValue(&v1, es, &(instr->dst));
        if (es->depth >= MAX_CALLDEPTH) {
            setEmulatorError(c, instr, ET_BufferOverflow,
                             "Call depth too deep");
            return;
        }
        if (!msIsStatic(v1.state)) {
            // call target must be known
            setEmulatorError(c, instr, ET_UnsupportedOperands,
                             "Call to unknown target not supported");
            return;
        }

        Instr i;
        Operand o;

        // push address of instruction after CALL onto stack
        copyOperand(&o, getImmOp(VT_64, instr->addr + instr->len));
        initUnaryInstr(&i, IT_PUSH, &o);
        processInstr(c, &i);
        if (c->e) return; // error

        es->ret_stack[es->depth++] = o.val;

        if (r->addInliningHints) {
            initSimpleInstr(&i, IT_HINT_CALL);
            capture(c, &i);
        }

        // address to jump to
        c->exit = v1.val;
        break;
    }

    case IT_CLTQ:
        switch(instr->vtype) {
        case VT_32:
            es->reg[RI_A] = (int32_t) (int16_t) es->reg[RI_A];
            break;
        case VT_64:
            es->reg[RI_A] = (int64_t) (int32_t) es->reg[RI_A];
            break;
        default: assert(0);
        }
        if (!msIsStatic(es->reg_state[RI_A]))
            capture(c, instr);
        break;

    case IT_CQTO:
        switch(instr->vtype) {
        case VT_64:
            // sign-extend eax to edx:eax
            es->reg[RI_D] = (es->reg[RI_A] & (1<<30)) ? ((uint32_t)-1) : 0;
            break;
        case VT_128:
            // sign-extend rax to rdx:rax
            es->reg[RI_D] = (es->reg[RI_A] & (1ul<<62)) ? ((uint64_t)-1) : 0;
            break;
        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }
        es->reg_state[RI_D] = es->reg_state[RI_A];
        if (!msIsStatic(es->reg_state[RI_A]))
            capture(c, instr);
        break;

    case IT_CMOVZ: case IT_CMOVNZ:
    case IT_CMOVC: case IT_CMOVNC:
    case IT_CMOVO: case IT_CMOVNO:
    case IT_CMOVS: case IT_CMOVNS:
    {
        FlagType ft;
        bool cond;
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
        if (cond == true) setOpValue(&v1, es, &(instr->dst));
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
        cs = setFlagsSub(es, &v1, &v2);
        captureCmp(c, instr, es, cs);
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

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
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

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }

        // optimization: multiply with static 0 results in static 0
        if ((msIsStatic(v1.state) && (v1.val == 0)) ||
            (msIsStatic(v2.state) && (v2.val == 0)))
            cs = CS_STATIC;
        else
            cs = combineState(v1.state.cState, v2.state.cState, 0);
        initMetaState(&(vres.state), cs);

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
        cs = combineState(es->reg_state[RI_D].cState,
                          es->reg_state[RI_A].cState, 1);
        cs = combineState(cs, v1.state.cState, 0);

        switch(instr->dst.type) {
        case OT_Reg32:
        case OT_Ind32:
            v = (es->reg[RI_D] << 32) + (es->reg[RI_A] & ((1ul<<32)-1) );
            v1.val = (int32_t) v1.val;
            quRes = v / v1.val;
            assert(quRes < (1u<<31)); // fits into 32 bit (TODO: raise exc)
            modRes = v % v1.val;
            break;

        case OT_Reg64:
        case OT_Ind64:
            // FIXME: should use rdx
            quRes = es->reg[RI_A] / v1.val;
            modRes = es->reg[RI_A] % v1.val;
            break;

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }

        captureIDiv(c, instr, cs, es);

        es->reg[RI_A] = quRes;
        es->reg[RI_D] = modRes;
        initMetaState(&(es->reg_state[RI_D]), cs);
        initMetaState(&(es->reg_state[RI_A]), cs);
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

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }
        captureUnaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;

    case IT_JO:
        if (msIsDynamic(es->flag_state[FT_Overflow])) {
            captureJcc(c, IT_JO, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Overflow]);
        }
        if (es->flag[FT_Overflow] == true)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JNO:
        if (msIsDynamic(es->flag_state[FT_Overflow])) {
            captureJcc(c, IT_JNO, instr->dst.val, instr->addr + instr->len,
                        !es->flag[FT_Overflow]);
        }
        if (es->flag[FT_Overflow] == false)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JC:
        if (msIsDynamic(es->flag_state[FT_Carry])) {
            captureJcc(c, IT_JC, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Carry]);
        }
        if (es->flag[FT_Carry] == true)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JNC:
        if (msIsDynamic(es->flag_state[FT_Carry])) {
            captureJcc(c, IT_JNC, instr->dst.val, instr->addr + instr->len,
                        !es->flag[FT_Carry]);
        }
        if (es->flag[FT_Carry] == false)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JZ:
        if (msIsDynamic(es->flag_state[FT_Zero])) {
            captureJcc(c, IT_JZ, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Zero]);
        }
        if (es->flag[FT_Zero] == true)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JNZ:
        if (msIsDynamic(es->flag_state[FT_Zero])) {
            captureJcc(c, IT_JNZ, instr->dst.val, instr->addr + instr->len,
                        !es->flag[FT_Zero]);
        }
        if (es->flag[FT_Zero] == false)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JBE:
        if (msIsDynamic(es->flag_state[FT_Carry]) ||
            msIsDynamic(es->flag_state[FT_Zero])) {
            captureJcc(c, IT_JLE, instr->dst.val, instr->addr + instr->len,
                        es->flag[FT_Carry] || es->flag[FT_Zero]);
        }
        if ((es->flag[FT_Carry] == true) ||
            (es->flag[FT_Zero] == true))
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JA:
        if (msIsDynamic(es->flag_state[FT_Carry]) ||
            msIsDynamic(es->flag_state[FT_Zero])) {
            captureJcc(c, IT_JG, instr->dst.val, instr->addr + instr->len,
                       !es->flag[FT_Carry] && !es->flag[FT_Zero]);
        }
        if ((es->flag[FT_Carry] == false) &&
            (es->flag[FT_Zero] == false))
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JS:
        if (msIsDynamic(es->flag_state[FT_Sign])) {
            captureJcc(c, IT_JS, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Sign]);
        }
        if (es->flag[FT_Sign] == true)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JNS:
        if (msIsDynamic(es->flag_state[FT_Sign])) {
            captureJcc(c, IT_JNS, instr->dst.val, instr->addr + instr->len,
                        !es->flag[FT_Sign]);
        }
        if (es->flag[FT_Sign] == false)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JP:
        if (msIsDynamic(es->flag_state[FT_Parity])) {
            captureJcc(c, IT_JP, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Parity]);
        }
        if (es->flag[FT_Parity] == true)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JNP:
        if (msIsDynamic(es->flag_state[FT_Parity])) {
            captureJcc(c, IT_JNP, instr->dst.val, instr->addr + instr->len,
                        !es->flag[FT_Parity]);
        }
        if (es->flag[FT_Parity] == false)
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JLE:
        if (msIsDynamic(es->flag_state[FT_Zero]) ||
            msIsDynamic(es->flag_state[FT_Sign])) {
            captureJcc(c, IT_JLE, instr->dst.val, instr->addr + instr->len,
                        es->flag[FT_Zero] || es->flag[FT_Sign]);
        }
        if ((es->flag[FT_Zero] == true) ||
            (es->flag[FT_Sign] == true)) c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JG:
        if (msIsDynamic(es->flag_state[FT_Zero]) ||
            msIsDynamic(es->flag_state[FT_Sign])) {
            captureJcc(c, IT_JG, instr->dst.val, instr->addr + instr->len,
                       !es->flag[FT_Zero] && !es->flag[FT_Sign]);
        }
        if ((es->flag[FT_Zero] == false) &&
            (es->flag[FT_Sign] == false)) c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JL:
        if (msIsDynamic(es->flag_state[FT_Sign]) ||
            msIsDynamic(es->flag_state[FT_Overflow])) {
            captureJcc(c, IT_JL, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Sign] != es->flag[FT_Overflow]);
        }
        if (es->flag[FT_Sign] != es->flag[FT_Overflow])
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JGE:
        if (msIsDynamic(es->flag_state[FT_Sign]) ||
            msIsDynamic(es->flag_state[FT_Overflow])) {
            captureJcc(c, IT_JGE, instr->dst.val, instr->addr + instr->len,
                       es->flag[FT_Sign] == es->flag[FT_Overflow]);
        }
        if (es->flag[FT_Sign] == es->flag[FT_Overflow])
            c->exit = instr->dst.val;
        else
            c->exit = instr->addr + instr->len;
        break;

    case IT_JMP:
        if (instr->dst.type != OT_Imm64) {
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }

        // address to jump to
        c->exit = instr->dst.val;
        break;

    case IT_JMPI:
        getOpValue(&v1, es, &(instr->dst));

        switch(instr->dst.type) {
        case OT_Reg64: break;
        case OT_Ind64:
            getOpAddr(&v2, es, &(instr->dst));
            if (msIsStatic(v2.state)) {
                // Assume indirect jump with target at constant address
                // in memory to be constant: follow resolved PLT entries
                v1.state.cState = CS_STATIC;
            }
            break;
        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }

        if (!msIsStatic(v1.state)) {
            // call target must be known
            setEmulatorError(c, instr, ET_BufferOverflow,
                             "Call to unknown target not supported");
            return;
        }
        c->exit = v1.val; // address to jump to
        break;

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
        Operand src, dst;

        // mov rbp,rsp (restore stack pointer)
        copyOperand( &src, getRegOp(getReg(RT_GP64, RI_BP)) );
        copyOperand( &dst, getRegOp(getReg(RT_GP64, RI_SP)) );
        initBinaryInstr(&i, IT_MOV, VT_None, &dst, &src);
        processInstr(c, &i);
        if (c->e) return; // error

        // pop rbp
        initUnaryInstr(&i, IT_POP, getRegOp(getReg(RT_GP64, RI_BP)));
        processInstr(c, &i);
        if (c->e) return; // error
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

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
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

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }
        captureUnaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;


    case IT_POP:
        switch(instr->dst.type) {
        case OT_Reg16:
            addr = emuValue(es->reg[RI_SP], VT_64, es->reg_state[RI_SP]);
            getMemValue(&v1, &addr, es, VT_16, 1);
            setOpValue(&v1, es, &(instr->dst));
            es->reg[RI_SP] += 2;
            if (!msIsStatic(v1.state))
                capture(c, instr);
            break;

        case OT_Reg64:
            addr = emuValue(es->reg[RI_SP], VT_64, es->reg_state[RI_SP]);
            getMemValue(&v1, &addr, es, VT_64, 1);
            setOpValue(&v1, es, &(instr->dst));
            es->reg[RI_SP] += 8;
            if (!msIsStatic(v1.state))
                capture(c, instr);
            break;

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }
        break;

    case IT_PUSH:
        switch(instr->dst.type) {
        case OT_Ind16:
        case OT_Reg16:
        case OT_Imm16:
            es->reg[RI_SP] -= 2;
            addr = emuValue(es->reg[RI_SP], VT_64, es->reg_state[RI_SP]);
            getOpValue(&v1, es, &(instr->dst));
            setMemValue(&v1, &addr, es, VT_16, 1);
            if (!msIsStatic(v1.state))
                capture(c, instr);
            break;

        case OT_Ind64:
        case OT_Reg64:
        case OT_Imm64:
        case OT_Imm8:
        case OT_Imm32:
            es->reg[RI_SP] -= 8;
            addr = emuValue(es->reg[RI_SP], VT_64, es->reg_state[RI_SP]);
            getOpValue(&v1, es, &(instr->dst));

            // Sign-extend 8-bit and 32-bit immediate values to 64-bit
            switch(v1.type) {
            case VT_8:
                v1.val = (int64_t) (int8_t) v1.val;
                break;
            case VT_32:
                v1.val = (int64_t) (int32_t) v1.val;
                break;
            case VT_64:
                break;
            default:
                assert(0);
            }

            v1.type = VT_64;
            setMemValue(&v1, &addr, es, VT_64, 1);
            if (!msIsStatic(v1.state))
                capture(c, instr);
            break;

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }
        break;

    case IT_RET:
        emulateRet(c, instr);
        break;

    case IT_SHL:
    case IT_SHR:
    case IT_SAR:
        // FIXME: do flags (shifting into CF, set OF)
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        switch(opValType(&(instr->dst))) {
        case VT_8:
            switch (instr->type) {
            case IT_SHL: v1.val = (uint8_t) (v1.val << (v2.val & 7)); break;
            case IT_SHR: v1.val = (uint8_t) (v1.val >> (v2.val & 7)); break;
            case IT_SAR:
                v1.val = (uint8_t) ((int8_t)v1.val >> (v2.val & 7)); break;
            default: assert(0);
            }
            break;

        case VT_16:
            switch (instr->type) {
            case IT_SHL: v1.val = (uint16_t) (v1.val << (v2.val & 15)); break;
            case IT_SHR: v1.val = (uint16_t) (v1.val >> (v2.val & 15)); break;
            case IT_SAR:
                v1.val = (uint16_t) ((int16_t)v1.val >> (v2.val & 15)); break;
            default: assert(0);
            }
            break;

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

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }
        v1.state.cState = combineState(v1.state.cState, v2.state.cState, 0);
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

        default:
            setEmulatorError(c, instr, ET_UnsupportedOperands, 0);
            return;
        }

        v1.state.cState = combineState(v1.state.cState, v2.state.cState, 0);
        // for capturing we need state of original dst, do before setting dst
        captureBinaryOp(c, instr, es, &v1);
        setOpValue(&v1, es, &(instr->dst));
        break;

    case IT_TEST:
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        assert(v1.type == v2.type);
        cs = setFlagsBit(es, IT_AND, &v1, &v2, false);
        captureTest(c, instr, es, cs);
        break;

    case IT_XOR:
    case IT_OR:
    case IT_AND:
        getOpValue(&v1, es, &(instr->dst));
        getOpValue(&v2, es, &(instr->src));

        assert(v1.type == v2.type);
        v1.state.cState = setFlagsBit(es, instr->type, &v1, &v2,
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

    case IT_ADDSS:
    case IT_ADDSD:
    case IT_ADDPS:
    case IT_ADDPD:
        // just always capture without emulation
        captureVec(c, instr, es);
        break;

    default:
        setEmulatorError(c, instr, ET_UnsupportedInstr, 0);
    }
}

// process call or jump to known location
// this may result in a redirection by returning another target address
uint64_t processKnownTargets(RContext* c, uint64_t f)
{
    EmuState* es = c->r->es;

    // special handling for known functions
    if ((f == (uint64_t) makeDynamic) &&
            msIsStatic(es->reg_state[RI_DI])) {
        // update register value to static value
        Instr i;
        initBinaryInstr(&i, IT_MOV, VT_64,
                        getRegOp(getReg(RT_GP64, RI_DI)),
                        getImmOp(VT_64, es->reg[RI_DI]));
        capture(c, &i);
        initMetaState(&(es->reg_state[RI_DI]), CS_DYNAMIC);
    }

    if (f == (uint64_t) makeStatic) {
        initMetaState(&(es->reg_state[RI_DI]), CS_STATIC2);
    }

    // vector API
    if ( (f == (uint64_t) dbrew_apply4_R8V8) ||
         (f == (uint64_t) dbrew_apply4_R8V8V8) ||
         (f == (uint64_t) dbrew_apply4_R8P8) )
        return handleVectorCall(c->r, f, es);

    return f;
}

