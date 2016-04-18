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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "buffers.h"
#include "common.h"
#include "instr.h"
#include "printer.h"
#include "decode.h"
#include "emulate.h"
#include "engine.h"
#include "generate.h"

/**
 * Functions which may be used in code to be rewritten
*/

// mark a passed-through value as dynamic

__attribute__ ((noinline))
uint64_t makeDynamic(uint64_t v)
{
    return v;
}

// mark a passed-through value as static

__attribute__ ((noinline))
uint64_t makeStatic(uint64_t v)
{
    return v;
}


/**
 * DBrew API functions
 */

Rewriter* dbrew_new(void)
{
    return allocRewriter();
}

void dbrew_free(Rewriter* r)
{
    freeRewriter(r);
}


/*------------------------------------------------------------*/
/* x86_64 Analyzers
 */


void dbrew_decode_print(Rewriter* c, uint64_t f, int count)
{
    DBB* dbb;
    int decoded = 0;

    c->decBBCount = 0;
    while(decoded < count) {
        dbb = dbrew_decode(c, f + decoded);
        decoded += dbb->size;
    }
    printDecodedBBs(c);
}


void dbrew_set_decoding_capacity(Rewriter* r,
                                 int instrCapacity, int bbCapacity)
{
    r->decInstrCapacity = instrCapacity;
    free(r->decInstr);
    r->decInstr = 0;

    r->decBBCapacity = bbCapacity;
    free(r->decBB);
    r->decBB = 0;
}

void dbrew_set_capture_capacity(Rewriter* r,
                                int instrCapacity, int bbCapacity,
                                int codeCapacity)
{
    r->capInstrCapacity = instrCapacity;
    free(r->capInstr);
    r->capInstr = 0;

    r->capBBCapacity = bbCapacity;
    free(r->capBB);
    r->capBB = 0;

    if (r->cs)
        freeCodeStorage(r->cs);
    r->cs = 0;
    r->capCodeCapacity = codeCapacity;
}


void dbrew_set_function(Rewriter* rewriter, uint64_t f)
{
    rewriter->func = f;

    // reset all decoding/state
    initRewriter(rewriter);
    dbrew_config_reset(rewriter);

    freeEmuState(rewriter);
}

void dbrew_verbose(Rewriter* rewriter,
                  Bool decode, Bool emuState, Bool emuSteps)
{
    rewriter->showDecoding = decode;
    rewriter->showEmuState = emuState;
    rewriter->showEmuSteps = emuSteps;
}

void dbrew_optverbose(Rewriter* r, Bool v)
{
    r->showOptSteps = v;
}

uint64_t dbrew_generated_code(Rewriter* r)
{
    return r->generatedCodeAddr;
}

int dbrew_generated_size(Rewriter* r)
{
    return r->generatedCodeSize;
}

static void freeCode(Rewriter* c)
{
    if (c->cs)
        freeCodeStorage(c->cs);

    free(c->cc);
    free(c->es);

    free(c->decBB);
    free(c->decInstr);
    free(c);
}


//-----------------------------------------------------------------
// convenience functions, using defaults

Rewriter* defaultRewriter = 0;

static Rewriter* getDefaultRewriter(void)
{
    if (!defaultRewriter)
        defaultRewriter = dbrew_new();

    return defaultRewriter;
}

void dbrew_def_verbose(Bool decode, Bool emuState, Bool emuSteps)
{
    dbrew_verbose(getDefaultRewriter(), decode, emuState, emuSteps);
}

// Act as drop-in replacement assuming the function is returning an integer
// This does not make use of captured code
uint64_t dbrew_emulate(Rewriter* r, ...)
{
    va_list argptr;

    va_start(argptr, r);
    vEmulateAndCapture(r, argptr);
    va_end(argptr);

    // integer return value is in RAX according to calling convention
    return r->es->reg[Reg_AX];
}

uint64_t dbrew_rewrite(Rewriter* r, ...)
{
    va_list argptr;

    va_start(argptr, r);
    vEmulateAndCapture(r, argptr);
    va_end(argptr);

    runOptsOnCaptured(r);
    generateBinaryFromCaptured(r);

    return r->generatedCodeAddr;
}

uint64_t dbrew_rewrite_func(uint64_t f, ...)
{
    Rewriter* r;
    va_list argptr;

    r = getDefaultRewriter();
    dbrew_set_function(r, f);

    va_start(argptr, f);
    vEmulateAndCapture(r, argptr);
    va_end(argptr);

    runOptsOnCaptured(r);
    generateBinaryFromCaptured(r);

    return r->generatedCodeAddr;
}
