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

#ifndef PRINTER_H
#define PRINTER_H

#include "common.h"

const char* regName(Reg r, OpType t);
char* op2string(Operand* o, ValType t);
const char* instrName(InstrType it, int* pOpCount);
char* instr2string(Instr* instr, int align);
char* bytes2string(Instr* instr, int start, int count);
char* prettyAddress(uint64_t a, FunctionConfig* fc);
void printDecodedBBs(Rewriter* c);

#endif // PRINTER_H
