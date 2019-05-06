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


/**
 * Replacement function stubs
 *
 * This file contains functions for generating instructions which "fakes" the
 * return value of a function which is replaced by an intrinsic in order to
 * ensure a function coterminoum when the replacing intrinsic has different
 * return value semantics
 */

#include <assert.h>

#include "stubs.h"
#include "instr.h"
#include "emulate.h"

void captureStub(RContext* c, InstrType it) {

    switch (it) {
    case IT_LIBC_MEMCPY: {
        Instr i;
        Operand src;
        Operand dst;
        copyOperand(&src, getRegOp(getReg(RT_GP64, RI_DI)));
        copyOperand(&dst, getRegOp(getReg(RT_GP64, RI_A)));
        initBinaryInstr(&i, IT_MOV, VT_64, &dst, &src);
        captureGenerated(c, &i);
    }
        break;
    default:
        assert(0);
    }
}
