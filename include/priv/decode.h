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

#ifndef DECODE_H
#define DECODE_H

#include "common.h"

#include <stdint.h>

typedef struct _DContext DContext;

Instr* nextInstr(Rewriter* r, uint64_t a, int len);
Instr* addSimple(Rewriter* r, DContext* c, InstrType it, ValType vt);
Instr* addUnaryOp(Rewriter* r, DContext* c, InstrType it, Operand* o);

Instr* addBinaryOp(Rewriter* r, DContext* c,
                   InstrType it, ValType vt, Operand* o1, Operand* o2);

Instr* addTernaryOp(Rewriter* r, DContext* c,
                    InstrType it, ValType vt,
                    Operand* o1, Operand* o2, Operand* o3);

#endif // DECODE_H
