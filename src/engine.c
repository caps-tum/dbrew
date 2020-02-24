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
#include "expr.h"
#include "error.h"


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
    r->vreq = VR_None;
    r->vectorsize = 16;
    r->es = 0;
    r->next = 0;
    r->ePool = 0;

    // optimization passes
    r->addInliningHints = true;
    r->doCopyPass = true;

    // default: debug off
    r->showDecoding = false;
    r->showEmuState = false;
    r->showEmuSteps = false;
    r->showOptSteps = false;

    // default: assembly printer shows bytes
    r->printBytes = true;

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
    r->genOrderCount = 0;

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

    if (r->ePool == 0)
        r->ePool = expr_allocPool(1000);
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
    expr_freePool(r->ePool);

    free(r);
}


/*----------------------------------------------------------
 * Rewrite engine
 *
 * Trace/emulate binary code of configured function and capture
 * instructions which need to be kept in the rewritten version.
 * This needs to keep track of the status of values stored in registers
 * (including flags) and on the stack.
 * Captured instructions are collected in multiple CBBs.
 */


/* See dbrew_emulate to see how to call this from a function
 * which acts almost as drop-in replacement (only one additional par).
 *
 * The state can be accessed as c->es afterwards (e.g. for the return
 * value of the emulated function)
 */
static
Error* emulateAndCapture(Rewriter* r, int parCount, uint64_t* par)
{
    // calling convention x86-64: parameters are stored in registers
    // see https://en.wikipedia.org/wiki/X86_calling_conventions
    static RegIndex parReg[6] = { RI_DI, RI_SI, RI_D, RI_C, RI_8, RI_9 };

    int i, esID;
    EmuState* es;
    DBB *dbb;
    CBB *cbb;
    Instr* instr = NULL;
    uint64_t bb_addr, nextbb_addr;
    RContext cxt;

    // init context
    cxt.r = r;
    cxt.e = 0;

    if (!r->es)
        r->es = allocEmuState(1024);
    resetEmuState(r->es);
    es = r->es;

    resetCapturing(r);
    if (r->cs)
        r->cs->used = 0;

    for(i=0;i<parCount;i++) {
        MetaState* ms = &(es->reg_state[parReg[i]]);
        es->reg[parReg[i]] = par[i];
        if (r->cc && (i<CC_MAXPARAM))
            *ms = r->cc->par_state[i];
        else
            initMetaState(ms, CS_DYNAMIC);

        ms->parDep = expr_newPar(r->ePool, i,
                                 r->cc ? r->cc->par_name[i] : 0);
    }

    es->reg[RI_SP] = (uint64_t) (es->stackStart + es->stackSize);
    initMetaState(&(es->reg_state[RI_SP]), CS_STACKRELATIVE);

    // traverse all paths and generate CBBs

    // push new CBB for c->func (as request to decode and emulate/capture
    // starting at that address)
    esID = saveEmuState(&cxt);
    cbb = (esID >= 0) ? getCaptureBB(&cxt, r->func, esID) : 0;
    if (cxt.e) return cxt.e;
    // new CBB has to be first in this rewriter (we start with it in Pass 2)
    assert(cbb == r->capBB);
    pushCaptureBB(&cxt, cbb);
    if (cxt.e) return cxt.e;
    assert(r->capStackTop == 0);

    // and start with this CBB
    bb_addr = cbb->dec_addr;
    r->currentCapBB = cbb;
    if (r->addInliningHints) {
        // hint: here starts a function, we can assume ABI calling conventions
        Instr hintInstr;
        initSimpleInstr(&hintInstr, IT_HINT_CALL);
        capture(&cxt, &hintInstr);
        if (cxt.e) return cxt.e;
    }

    if (r->showEmuSteps) {
        printf("Processing BB (%s)\n", cbb_prettyName(cbb));
        printStaticEmuState(es, cbb->esID);
    }
    if (r->showEmuState) {
        es->regIP = bb_addr;
        printEmuState(es);
    }

    while(1) {
        if (r->currentCapBB == 0) {
            // open next yet-to-be-processed CBB
            while(r->capStackTop >= 0) {
                cbb = r->capStack[r->capStackTop];
                if (cbb->endType == IT_None) break;
                // cbb already handled; go to previous item on capture stack
                r->capStackTop--;
            }
            // all paths captured?
            if (r->capStackTop < 0) break;

            assert(cbb != 0);
            assert(cbb->count == 0); // should have no instructions yet
            restoreEmuState(r, cbb->esID);
            bb_addr = cbb->dec_addr;
            r->currentCapBB = cbb;

            if (r->showEmuSteps) {
                printf("Processing BB (%s), %d BBs in queue\n",
                       cbb_prettyName(cbb), r->capStackTop);
                printStaticEmuState(es, cbb->esID);
            }
            if (r->showEmuState) {
                es->regIP = bb_addr;
                printEmuState(es);
            }
        }

        // decode and process instructions starting at bb_addr.
        // note: multiple original BBs may be combined into one CBB
        dbb = dbrew_decode(r, bb_addr);
        for(i = 0; i < dbb->count; i++) {
            instr = dbb->instr + i;

            if (r->showEmuSteps) {
                printf("Emulate '%s:", prettyAddress(instr->addr, dbb->fc));
                printf(" %s'\n", instr2string(instr, 0, dbb->fc));
            }

            // for RIP-relative accesses
            es->regIP = instr->addr + instr->len;

            cxt.exit = 0;
            processInstr(&cxt, instr);
            if (cxt.e) {
                assert(isErrorSet(cxt.e));
                r->capBBCount = 0;
                return cxt.e;
            }

            if (cxt.exit) assert(i == dbb->count - 1);
            nextbb_addr = processKnownTargets(&cxt, cxt.exit);

            if (r->showEmuState) {
                if (nextbb_addr != 0) es->regIP = nextbb_addr;
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
            captureRet(&cxt, instr, es);
            if (cxt.e) return cxt.e;

            // finish current BB, go to next path to process
            cbb = popCaptureBB(r);
            cbb->endType = IT_RET;
        }
        bb_addr = nextbb_addr;
    }
    return 0;
}

Error* vEmulateAndCapture(Rewriter* r, va_list args)
{
    static Error e;
    int i, parCount;
    uint64_t par[6];

    parCount = r->cc->parCount;
    if (parCount == -1) {
        setError(&e, ET_InvalidRequest, EM_Rewriter, r,
                 "number of parameters not set");
        return &e;
    }

    if (parCount > 6) {
        setError(&e, ET_InvalidRequest, EM_Rewriter, r,
                 "number of parameters >6 not supported");
        return &e;
    }

    for(i = 0; i < parCount; i++) {
        par[i] = va_arg(args, uint64_t);
    }

    return emulateAndCapture(r, parCount, par);
}


//----------------------------------------------------------
// example optimization passes on captured instructions
//

// test: simply copy instructions
static
Instr* optPassCopy(RContext* c, CBB* cbb)
{
    Instr *first, *instr;
    int i;

    if (cbb->count == 0) return 0;

    first = newCapInstr(c);
    if (!first) return 0;
    copyInstr(first, cbb->instr);
    for(i = 1; i < cbb->count; i++) {
        instr = newCapInstr(c);
        if (!instr) return 0;
        copyInstr(instr, cbb->instr + i);
    }
    return first;
}

static
void optPass(RContext* c, CBB* cbb)
{
    Instr* newInstrs;
    Rewriter* r = c->r;

    if (r->showOptSteps) {
        printf("Run Optimization for CBB (%lx|%d)\n",
               cbb->dec_addr, cbb->esID);
    }

    if (r->doCopyPass) {
        newInstrs = optPassCopy(c, cbb);
        if (newInstrs)
            cbb->instr = newInstrs;
    }
}


// apply optimization passes to instructions captured in vEmulateAndCapture
void runOptsOnCaptured(RContext* c)
{
    Rewriter* r = c->r;
    for(int i = 0; i < r->capBBCount; i++) {
        CBB* cbb = r->capBB + i;
        optPass(c, cbb);
        if (c->e) return;
    }
}


//----------------------------------------------------------
// generate x86 code from instructions captured in vEmulateAndCapture
//

// result in c->rewrittenFunc/rewrittenSize
void generateBinaryFromCaptured(RContext *c)
{
    CBB* cbb;

    // Pass 1: generating code for BBs without linking them

    Rewriter* r = c->r;
    uint8_t* buf0 = reserveCodeStorage(r->cs, 0);

    // align address to cacheline boundary (multiple of 64)
    int cl_off = ((uint64_t)buf0) & 63;
    if (cl_off >0) {
        useCodeStorage(r->cs, 64 - cl_off);
        buf0 = reserveCodeStorage(r->cs, 0);
    }

    int usedPass0 = r->cs->used;
    int genOrder0 = r->genOrderCount;

    assert(r->capStackTop == -1);
    assert(r->capBBCount > 0);
    // start with first CBB created
    pushCaptureBB(c, r->capBB);
    if (c->e) return;

    while(r->capStackTop >= 0) {
        cbb = r->capStack[r->capStackTop];
        r->capStackTop--;
        if (cbb->size >= 0) continue;

        assert(r->genOrderCount < GENORDER_MAX);
        r->genOrder[r->genOrderCount++] = cbb;

        Error* e = (Error*) generate(r, cbb);
        if (e) {
            assert(isErrorSet(e));
            r->generatedCodeAddr = 0;
            r->generatedCodeSize = 0;
            c->e = e;
            return;
        }

        if (instrIsJcc(cbb->endType)) {
            // FIXME: order according to branch preference
            pushCaptureBB(c, cbb->nextBranch);
            pushCaptureBB(c, cbb->nextFallThrough);
            if (c->e) return;
        }

        // add a hole with size maximally needed (shrinks in pass 2)
        // pc-relative Jcc (6) + PC-relative Jmp (5) + alignment (15) = 26
        useCodeStorage(r->cs, 26);
    }

    // Pass 2: determine trailing bytes needed for each BB

    // reuse code storage: set pointer to buf0
    // This is fine as we shrink the holes between BBs as needed
    uint8_t* buf1 = buf0;

    r->genOrder[r->genOrderCount] = 0;
    for(int i=genOrder0; i < r->genOrderCount; i++) {
        int diff;

        cbb = r->genOrder[i];
        cbb->addr2 = (uint64_t) buf1;
        buf1 += cbb->size;

        if (cbb->size > 0) {
            assert(cbb->count>0);
            assert(cbb->addr2 <= cbb->addr1);
            // copy manually, dst may overlap src!
            char* src = (char*)cbb->addr1;
            char* dst = (char*)cbb->addr2;
            for(int j=0; j<cbb->size; j++)
                dst[j] = src[j];
        }
        if (!instrIsJcc(cbb->endType)) continue;

        diff = cbb->nextBranch->addr1 - (cbb->addr1 + cbb->size);
        if ((diff > -120) && (diff < 120))
            cbb->genJcc8 = true;
        buf1 += cbb->genJcc8 ? 2 : 6;
        if (cbb->nextFallThrough != r->genOrder[i+1]) {
            cbb->genJump = true;
            buf1 += 5;
        }
    }
    if (r->showEmuSteps) {
        printf("Generated: %d bytes (pass1: %d)\n",
               (int)(buf1 - buf0), r->cs->used - usedPass0);
        //printf(" at %lx\n", (uint64_t) buf0);
    }

    // free bytes used in holes
    r->cs->used = usedPass0 + (buf1 - buf0);

    // Pass 3: fill trailing bytes with jump instructions

    for(int i=0; i < r->genOrderCount; i++) {
        uint8_t* buf;
        uint64_t buf_addr;
        int diff;

        cbb = r->genOrder[i];
        if (!instrIsJcc(cbb->endType)) continue;

        buf = (uint8_t*) (cbb->addr2 + cbb->size);
        buf_addr = (uint64_t) buf;
        if (cbb->genJcc8) {
            diff = cbb->nextBranch->addr2 - (buf_addr + 2);
            assert((diff > -128) && (diff < 127));

            switch (cbb->endType) {
            case IT_JO:  buf[0] = 0x70; break;
            case IT_JNO: buf[0] = 0x71; break;
            case IT_JC:  buf[0] = 0x72; break;
            case IT_JNC: buf[0] = 0x73; break;
            case IT_JZ:  buf[0] = 0x74; break;
            case IT_JNZ: buf[0] = 0x75; break;
            case IT_JBE: buf[0] = 0x76; break;
            case IT_JA:  buf[0] = 0x77; break;
            case IT_JS:  buf[0] = 0x78; break;
            case IT_JNS: buf[0] = 0x79; break;
            case IT_JP:  buf[0] = 0x7A; break;
            case IT_JNP: buf[0] = 0x7B; break;
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
            switch (cbb->endType) {
            case IT_JO:  buf[1] = 0x80; break;
            case IT_JNO: buf[1] = 0x81; break;
            case IT_JC:  buf[1] = 0x82; break;
            case IT_JNC: buf[1] = 0x83; break;
            case IT_JZ:  buf[1] = 0x84; break;
            case IT_JNZ: buf[1] = 0x85; break;
            case IT_JBE: buf[1] = 0x86; break;
            case IT_JA:  buf[1] = 0x87; break;
            case IT_JS:  buf[1] = 0x88; break;
            case IT_JNS: buf[1] = 0x89; break;
            case IT_JP:  buf[1] = 0x8A; break;
            case IT_JNP: buf[1] = 0x8B; break;
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

    assert(r->cs != 0);
    assert(r->cs->used > 0);

    if (r->genOrderCount > 0) {
        int usedBefore = (r->genOrder[0]->addr2 - (uint64_t) r->cs->buf);
        r->generatedCodeAddr = r->genOrder[0]->addr2;
        r->generatedCodeSize = r->cs->used - usedBefore;
    }
    else {
        r->generatedCodeAddr = 0;
        r->generatedCodeSize = 0;
    }
}
