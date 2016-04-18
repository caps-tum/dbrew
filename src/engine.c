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

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "printer.h"
#include "engine.h"
#include "emulate.h"
#include "decode.h"
#include "generate.h"


Rewriter* allocRewriter(void)
{
    Rewriter* r;
    int i;

    r = (Rewriter*) malloc(sizeof(Rewriter));

    // allocation of other members on demand, capacities may be reset

    r->decInstrCount = 0;
    r->decInstrCapacity = 0;
    r->decInstr = 0;

    r->decBBCount = 0;
    r->decBBCapacity = 0;
    r->decBB = 0;

    r->capInstrCount = 0;
    r->capInstrCapacity = 0;
    r->capInstr = 0;

    r->capBBCount = 0;
    r->capBBCapacity = 0;
    r->capBB = 0;
    r->currentCapBB = 0;
    r->capStackTop = -1;
    r->genOrderCount = 0;

    r->savedStateCount = 0;
    for(i=0; i< SAVEDSTATE_MAX; i++)
        r->savedState[i] = 0;

    r->capCodeCapacity = 0;
    r->cs = 0;
    r->generatedCodeAddr = 0;
    r->generatedCodeSize = 0;

    r->cc = 0;
    r->es = 0;

    // optimization passes
    r->addInliningHints = True;
    r->doCopyPass = True;

    // default: debug off
    r->showDecoding = False;
    r->showEmuState = False;
    r->showEmuSteps = False;

    return r;
}

void initRewriter(Rewriter* r)
{
    if (r->decInstr == 0) {
        // default
        if (r->decInstrCapacity == 0) r->decInstrCapacity = 500;
        r->decInstr = (Instr*) malloc(sizeof(Instr) * r->decInstrCapacity);
    }
    r->decInstrCount = 0;

    if (r->decBB == 0) {
        // default
        if (r->decBBCapacity == 0) r->decBBCapacity = 50;
        r->decBB = (DBB*) malloc(sizeof(DBB) * r->decBBCapacity);
    }
    r->decBBCount = 0;

    if (r->capInstr == 0) {
        // default
        if (r->capInstrCapacity == 0) r->capInstrCapacity = 500;
        r->capInstr = (Instr*) malloc(sizeof(Instr) * r->capInstrCapacity);
    }
    r->capInstrCount = 0;

    if (r->capBB == 0) {
        // default
        if (r->capBBCapacity == 0) r->capBBCapacity = 50;
        r->capBB = (CBB*) malloc(sizeof(CBB) * r->capBBCapacity);
    }
    r->capBBCount = 0;
    r->currentCapBB = 0;

    if (r->cs == 0) {
        if (r->capCodeCapacity == 0) r->capCodeCapacity = 3000;
        if (r->capCodeCapacity >0)
            r->cs = initCodeStorage(r->capCodeCapacity);
    }
    if (r->cs) {
        r->cs->used = 0;
        // any previously generated code is invalid
        r->generatedCodeAddr = 0;
        r->generatedCodeSize = 0;
    }
}

void freeRewriter(Rewriter* r)
{
    if (!r) return;

    free(r->decInstr);
    free(r->decBB);
    free(r->capInstr);
    free(r->capBB);
    free(r->cc);

    freeEmuState(r);
    if (r->cs)
        freeCodeStorage(r->cs);
    free(r);
}


//----------------------------------------------------------
// Rewrite engine

/**
 * Trace/emulate binary code of configured function and capture
 * instructions which need to be kept in the rewritten version.
 * This needs to keep track of the status of values stored in registers
 * (including flags) and on the stack.
 * Captured instructions are collected in multiple CBBs.
 *
 * See dbrew_emulate to see how to call this from a function
 * which acts almost as drop-in replacement (only one additional par).
 *
 * The state can be accessed as c->es afterwards (e.g. for the return
 * value of the emulated function)
 */
// FIXME: this always assumes 5 parameters
void vEmulateAndCapture(Rewriter* c, va_list args)
{
    // calling convention x86-64: parameters are stored in registers
    static Reg parReg[5] = { Reg_DI, Reg_SI, Reg_DX, Reg_CX, Reg_8 };

    int i, esID;
    uint64_t par[5];
    EmuState* es;
    DBB *dbb;
    CBB *cbb;
    Instr* instr;
    uint64_t bb_addr, nextbb_addr;

    par[0] = va_arg(args, uint64_t);
    par[1] = va_arg(args, uint64_t);
    par[2] = va_arg(args, uint64_t);
    par[3] = va_arg(args, uint64_t);
    par[4] = va_arg(args, uint64_t);

#if 0
    // with rewrite(Rewriter *r, ...)
    //
    // setup int parameters for virtual CPU according to x86_64 calling conv.
    // see https://en.wikipedia.org/wiki/X86_calling_conventions
    asm("mov %%rsi, %0;" : "=r" (par[0]) : );
    asm("mov %%rdx, %0;" : "=r" (par[1]) : );
    asm("mov %%rcx, %0;" : "=r" (par[2]) : );
    asm("mov %%r8, %0;"  : "=r" (par[3]) : );
    asm("mov %%r9, %0;"  : "=r" (par[4]) : );
#endif

    if (!c->es)
        c->es = allocEmuState(1024);
    resetEmuState(c->es);
    es = c->es;

    resetCapturing(c);
    if (c->cs)
        c->cs->used = 0;

    for(i=0;i<5;i++) {
        es->reg[parReg[i]] = par[i];
        es->reg_state[parReg[i]] = c->cc ? c->cc->par_state[i] : CS_DYNAMIC;
    }

    es->reg[Reg_SP] = (uint64_t) (es->stackStart + es->stackSize);
    es->reg_state[Reg_SP] = CS_STACKRELATIVE;

    // traverse all paths and generate CBBs

    // push new CBB for c->func (as request to decode and emulate/capture
    // starting at that address)
    esID = saveEmuState(c);
    cbb = getCaptureBB(c, c->func, esID);
    // new CBB has to be first in this rewriter (we start with it in Pass 2)
    assert(cbb = c->capBB);
    pushCaptureBB(c, cbb);
    assert(c->capStackTop == 0);

    // and start with this CBB
    bb_addr = cbb->dec_addr;
    c->currentCapBB = cbb;
    if (c->addInliningHints) {
        // hint: here starts a function, we can assume ABI calling conventions
        Instr i;
        initSimpleInstr(&i, IT_HINT_CALL);
        capture(c, &i);
    }

    if (c->showEmuSteps) {
        printf("Processing BB (%s)\n", cbb_prettyName(cbb));
        printStaticEmuState(es, cbb->esID);
    }
    if (c->showEmuState) {
        es->reg[Reg_IP] = bb_addr;
        printEmuState(es);
    }

    while(1) {
        if (c->currentCapBB == 0) {
            // open next yet-to-be-processed CBB
            while(c->capStackTop >= 0) {
                cbb = c->capStack[c->capStackTop];
                if (cbb->endType == IT_None) break;
                // cbb already handled; go to previous item on capture stack
                c->capStackTop--;
            }
            // all paths captured?
            if (c->capStackTop < 0) break;

            assert(cbb != 0);
            assert(cbb->count == 0); // should have no instructions yet
            restoreEmuState(c, cbb->esID);
            bb_addr = cbb->dec_addr;
            c->currentCapBB = cbb;

            if (c->showEmuSteps) {
                printf("Processing BB (%s), %d BBs in queue\n",
                       cbb_prettyName(cbb), c->capStackTop);
                printStaticEmuState(es, cbb->esID);
            }
            if (c->showEmuState) {
                es->reg[Reg_IP] = bb_addr;
                printEmuState(es);
            }
        }

        // decode and process instructions starting at bb_addr.
        // note: multiple original BBs may be combined into one CBB
        dbb = dbrew_decode(c, bb_addr);
        for(i = 0; i < dbb->count; i++) {
            instr = dbb->instr + i;

            if (c->showEmuSteps)
                printf("Emulate '%s: %s'\n",
                       prettyAddress(instr->addr, dbb->fc),
                       instr2string(instr, 0));

            // for RIP-relative accesses
            es->reg[Reg_IP] = instr->addr + instr->len;

            nextbb_addr = emulateInstr(c, es, instr);

            if (c->showEmuState) {
                if (nextbb_addr != 0) es->reg[Reg_IP] = nextbb_addr;
                printEmuState(es);
            }

            // side-exit taken?
            if (nextbb_addr != 0) break;
        }
        if (i == dbb->count) {
            // fall through at end of BB
            nextbb_addr = instr->addr + instr->len;
        }
        if (es->depth < 0) {
            // finish this path
            assert(instr->type == IT_RET);
            captureRet(c, instr, es);

            // go to next path to trace
            cbb = popCaptureBB(c);
            cbb->endType = IT_RET;
        }
        bb_addr = nextbb_addr;
    }
}

//----------------------------------------------------------
// example optimization passes on captured instructions
//

// test: simply copy instructions
static Instr* optPassCopy(Rewriter* r, CBB* cbb)
{
    Instr *first, *instr;
    int i;

    if (cbb->count == 0) return 0;

    first = newCapInstr(r);
    copyInstr(first, cbb->instr);
    for(i = 1; i < cbb->count; i++) {
        instr = newCapInstr(r);
        copyInstr(instr, cbb->instr + i);
    }
    return first;
}

static void optPass(Rewriter* r, CBB* cbb)
{
    Instr* newInstrs;

    printf("OPT!!\n");

    if (r->showOptSteps) {
        printf("Run Optimization for CBB (%lx|%d)\n",
               cbb->dec_addr, cbb->esID);
    }

    if (r->doCopyPass) {
        newInstrs = optPassCopy(r, cbb);
        if (newInstrs)
            cbb->instr = newInstrs;
    }
}


// apply optimization passes to instructions captured in vEmulateAndCapture
void runOptsOnCaptured(Rewriter* r)
{
    for(int i = 0; i < r->capBBCount; i++) {
        CBB* cbb = r->capBB + i;
        optPass(r, cbb);
    }
}


//----------------------------------------------------------
// generate x86 code from instructions captured in vEmulateAndCapture
//

// result in c->rewrittenFunc/rewrittenSize
void generateBinaryFromCaptured(Rewriter* c)
{
    CBB* cbb;

    // Pass 1: generating code for BBs without linking them

    assert(c->capStackTop == -1);
    // start with first CBB created
    pushCaptureBB(c, c->capBB);
    while(c->capStackTop >= 0) {
        cbb = c->capStack[c->capStackTop];
        c->capStackTop--;
        if (cbb->size >= 0) continue;

        assert(c->genOrderCount < GENORDER_MAX);
        c->genOrder[c->genOrderCount++] = cbb;
        generate(c, cbb);

        if (instrIsJcc(cbb->endType)) {
            // FIXME: order according to branch preference
            pushCaptureBB(c, cbb->nextBranch);
            pushCaptureBB(c, cbb->nextFallThrough);
        }
    }

    // Pass 2: determine trailing bytes needed for each BB

    c->genOrder[c->genOrderCount] = 0;
    for(int i=0; i < c->genOrderCount; i++) {
        uint8_t* buf;
        int diff;

        cbb = c->genOrder[i];
        buf = useCodeStorage(c->cs, cbb->size);
        cbb->addr2 = (uint64_t) buf;
        if (cbb->size > 0) {
            assert(cbb->count>0);
            memcpy(buf, (char*)cbb->addr1, cbb->size);
        }
        if (!instrIsJcc(cbb->endType)) continue;

        diff = cbb->nextBranch->addr1 - (cbb->addr1 + cbb->size);
        if ((diff > -120) && (diff < 120))
            cbb->genJcc8 = True;
        useCodeStorage(c->cs, cbb->genJcc8 ? 2 : 6);
        if (cbb->nextFallThrough != c->genOrder[i+1]) {
            cbb->genJump = True;
            useCodeStorage(c->cs, 5);
        }
    }

    // Pass 3: fill trailing bytes with jump instructions

    for(int i=0; i < c->genOrderCount; i++) {
        uint8_t* buf;
        uint64_t buf_addr;
        int diff;

        cbb = c->genOrder[i];
        if (!instrIsJcc(cbb->endType)) continue;

        buf = (uint8_t*) (cbb->addr2 + cbb->size);
        buf_addr = (uint64_t) buf;
        if (cbb->genJcc8) {
            diff = cbb->nextBranch->addr2 - (buf_addr + 2);
            assert((diff > -128) && (diff < 127));

            switch(cbb->endType) {
            case IT_JE:  buf[0] = 0x74; break;
            case IT_JNE: buf[0] = 0x75; break;
            case IT_JP:  buf[0] = 0x7A; break;
            case IT_JL:  buf[0] = 0x7C; break;
            case IT_JGE: buf[0] = 0x7D; break;
            case IT_JLE: buf[0] = 0x7E; break;
            case IT_JG:  buf[0] = 0x7F; break;
            default: assert(0);
            }
            buf[1] = (int8_t) diff;
            buf += 2;
        }
        else {
            diff = cbb->nextBranch->addr2 - (buf_addr + 6);
            buf[0] = 0x0F;
            switch(cbb->endType) {
            case IT_JE:  buf[1] = 0x84; break;
            case IT_JNE: buf[1] = 0x85; break;
            case IT_JP:  buf[1] = 0x8A; break;
            case IT_JL:  buf[1] = 0x8C; break;
            case IT_JGE: buf[1] = 0x8D; break;
            case IT_JLE: buf[1] = 0x8E; break;
            case IT_JG:  buf[1] = 0x8F; break;
            default: assert(0);
            }
            *(int32_t*)(buf+2) = diff;
            buf += 6;
        }
        if (cbb->genJump) {
            buf_addr = (uint64_t) buf;
            diff = cbb->nextFallThrough->addr2 - (buf_addr + 5);
            buf[0] = 0xE9;
            *(int32_t*)(buf+1) = diff;
            buf += 5;
        }
    }

    assert(c->cs != 0);
    assert(c->cs->used > 0);

    if (c->genOrderCount > 0) {
        int usedBefore = (c->genOrder[0]->addr2 - (uint64_t) c->cs->buf);
        c->generatedCodeAddr = c->genOrder[0]->addr2;
        c->generatedCodeSize = c->cs->used - usedBefore;
    }
    else {
        c->generatedCodeAddr = 0;
        c->generatedCodeSize = 0;
    }
}
