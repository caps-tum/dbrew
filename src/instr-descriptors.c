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
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "instr-descriptors.h"

#include "instr.h"


// Operand types: vector or general-purpose for operand 1 and 2
#define RT_GG 0
#define RT_GV 1
#define RT_VG 2
#define RT_VV 3

#define INSTR_M(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vti,digit,it) { OE_M, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, 0, vti, digit, 0, false, it, NULL },
#define INSTR_M1(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vti,digit,it) { OE_M1, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, 0, vti, digit, 0, false, it, NULL },
#define INSTR_Mcc(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vti,digit,it) { OE_M, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, 0, vti, digit, 0, true, it, NULL },
#define INSTR_MC(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vti,digit,it) { OE_MC, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, 0, vti, digit, 0, true, it, NULL },
#define INSTR_MI(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vti,digit,immsize,it) { OE_MI, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, 0, vti, digit, immsize, false, it, NULL },
#define INSTR_MR(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vto2,vti,it) { OE_MR, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, vto2, vti, -1, 0, false, it, NULL },
#define INSTR_MRI(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vto2,vti,immsize,it) { OE_MRI, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, vto2, vti, -1, immsize, false, it, NULL },
#define INSTR_RM(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vto2,vti,it) { OE_RM, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, vto2, vti, -1, 0, false, it, NULL },
#define INSTR_RMcc(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vto2,vti,it) { OE_RM, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, vto2, vti, -1, 0, true, it, NULL },
#define INSTR_RMI(opcCount,opc1,opc2,opc3,prefixes,regtype,vto1,vto2,vti,immsize,it) { OE_RMI, opcCount, {opc1, opc2, opc3}, prefixes, regtype, vto1, vto2, vti, -1, immsize, false, it, NULL },
#define INSTR_O(opcCount,opc1,opc2,opc3,prefixes,vti,it) { OE_O, opcCount, {opc1, opc2, opc3}, prefixes, RT_GG, 0, 0, vti, -1, 0, false, it, NULL },
#define INSTR_OI(opcCount,opc1,opc2,opc3,prefixes,vti,immsize,it) { OE_OI, opcCount, {opc1, opc2, opc3}, prefixes, RT_GG, 0, 0, vti, -1, immsize, false, it, NULL },
#define INSTR_I(opcCount,opc1,opc2,opc3,prefixes,vti,immsize,it) { OE_I, opcCount, {opc1, opc2, opc3}, prefixes, RT_GG, 0, 0, vti, -1, immsize, false, it, NULL },
#define INSTR_IA(opcCount,opc1,opc2,opc3,prefixes,vti,immsize,it) { OE_IA, opcCount, {opc1, opc2, opc3}, prefixes, RT_GG, 0, 0, vti, -1, immsize, false, it, NULL },
#define INSTR_D(opcCount,opc1,opc2,opc3,prefixes,vti,immsize,it) { OE_D, opcCount, {opc1, opc2, opc3}, prefixes, RT_GG, 0, 0, vti, -1, immsize, false, it, NULL },
#define INSTR_Dcc(opcCount,opc1,opc2,opc3,prefixes,vti,immsize,it) { OE_D, opcCount, {opc1, opc2, opc3}, prefixes, RT_GG, 0, 0, vti, -1, immsize, true, it, NULL },
#define INSTR_NP(opcCount,opc1,opc2,opc3,prefixes,vti,it) { OE_NP, opcCount, {opc1, opc2, opc3}, prefixes, RT_GG, 0, 0, vti, -1, 0, false, it, NULL },
#define INSTR_fn(opcCount,opc1,opc2,opc3,prefixes,handler) { OE_None, opcCount, {opc1, opc2, opc3}, prefixes, RT_GG, 0, 0, VT_None, -1, 0, false, IT_None, handler },
#define INSTR1_M(opc1,...) INSTR_M(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_M1(opc1,...) INSTR_M1(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_MI(opc1,...) INSTR_MI(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_MC(opc1,...) INSTR_MC(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_MR(opc1,...) INSTR_MR(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_MRI(opc1,...) INSTR_MRI(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_RM(opc1,...) INSTR_RM(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_RMI(opc1,...) INSTR_RMI(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_O(opc1,...) INSTR_O(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_OI(opc1,...) INSTR_OI(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_I(opc1,...) INSTR_I(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_IA(opc1,...) INSTR_IA(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_D(opc1,...) INSTR_D(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_Dcc(opc1,...) INSTR_Dcc(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_NP(opc1,...) INSTR_NP(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR1_fn(opc1,...) INSTR_fn(1,opc1,-1,-1,__VA_ARGS__)
#define INSTR2_M(opc2,...) INSTR_M(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_Mcc(opc2,...) INSTR_Mcc(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_MI(opc2,...) INSTR_MI(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_MR(opc2,...) INSTR_MR(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_MRI(opc2,...) INSTR_MRI(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_RM(opc2,...) INSTR_RM(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_RMcc(opc2,...) INSTR_RMcc(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_RMI(opc2,...) INSTR_RMI(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_O(opc2,...) INSTR_O(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_OI(opc2,...) INSTR_OI(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_I(opc2,...) INSTR_I(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_D(opc2,...) INSTR_D(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_Dcc(opc2,...) INSTR_Dcc(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_NP(opc2,...) INSTR_NP(2,0x0F,opc2,-1,__VA_ARGS__)
#define INSTR2_fn(opc2,...) INSTR_fn(2,0x0F,opc2,-1,__VA_ARGS__)
// Later, if we want instructions with 0f38, 0f3a
// #define INSTR3_M(opc2,opc3,...) INSTR_M(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_MI(opc2,opc3,...) INSTR_MI(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_MR(opc2,opc3,...) INSTR_MR(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_MRI(opc2,opc3,...) INSTR_MRI(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_RM(opc2,opc3,...) INSTR_RM(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_RMI(opc2,opc3,...) INSTR_RMI(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_O(opc2,opc3,...) INSTR_O(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_I(opc2,opc3,...) INSTR_I(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_D(opc2,opc3,...) INSTR_D(3,0x0F,opc2,opc3,__VA_ARGS__)
// #define INSTR3_NP(opc2,opc3,...) INSTR_NP(3,0x0F,opc2,opc3,__VA_ARGS__)

// An example how to use custom decoding functions.
// static
// int
// decode_nop_simple(uint8_t* fp, Instr* instr, const InstrDescriptor* desc, int rex, OpSegOverride segment)
// {
//     initSimpleInstr(instr, IT_NOP);
//     (void) fp;
//     (void) desc;
//     (void) rex;
//     (void) segment;
//     return 0;
// }

const InstrDescriptor instrDescriptors[] = {
INSTR1_MR(0x00, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_ADD)
INSTR1_MR(0x01, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_ADD)
INSTR1_MR(0x01, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_ADD)
INSTR1_RM(0x02, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_ADD)
INSTR1_RM(0x03, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_ADD)
INSTR1_RM(0x03, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_ADD)
INSTR1_IA(0x04, PS_None, VT_8, 8, IT_ADD)
INSTR1_IA(0x05, PS_66, VT_16, 16, IT_ADD)
INSTR1_IA(0x05, PS_None, VT_None, 32, IT_ADD)
INSTR1_MR(0x08, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_OR)
INSTR1_MR(0x09, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_OR)
INSTR1_MR(0x09, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_OR)
INSTR1_RM(0x0A, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_OR)
INSTR1_RM(0x0B, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_OR)
INSTR1_RM(0x0B, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_OR)
INSTR1_IA(0x0C, PS_None, VT_8, 8, IT_OR)
INSTR1_IA(0x0D, PS_66, VT_16, 16, IT_OR)
INSTR1_IA(0x0D, PS_None, VT_None, 32, IT_OR)

INSTR1_MR(0x10, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_ADC)
INSTR1_MR(0x11, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_ADC)
INSTR1_MR(0x11, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_ADC)
INSTR1_RM(0x12, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_ADC)
INSTR1_RM(0x13, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_ADC)
INSTR1_RM(0x13, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_ADC)
INSTR1_IA(0x14, PS_None, VT_8, 8, IT_ADC)
INSTR1_IA(0x15, PS_66, VT_16, 16, IT_ADC)
INSTR1_IA(0x15, PS_None, VT_None, 32, IT_ADC)
INSTR1_MR(0x18, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_SBB)
INSTR1_MR(0x19, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_SBB)
INSTR1_MR(0x19, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_SBB)
INSTR1_RM(0x1A, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_SBB)
INSTR1_RM(0x1B, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_SBB)
INSTR1_RM(0x1B, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_SBB)
INSTR1_IA(0x1C, PS_None, VT_8, 8, IT_SBB)
INSTR1_IA(0x1D, PS_66, VT_16, 16, IT_SBB)
INSTR1_IA(0x1D, PS_None, VT_None, 32, IT_SBB)

INSTR1_MR(0x20, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_AND)
INSTR1_MR(0x21, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_AND)
INSTR1_MR(0x21, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_AND)
INSTR1_RM(0x22, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_AND)
INSTR1_RM(0x23, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_AND)
INSTR1_RM(0x23, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_AND)
INSTR1_IA(0x24, PS_None, VT_8, 8, IT_AND)
INSTR1_IA(0x25, PS_66, VT_16, 16, IT_AND)
INSTR1_IA(0x25, PS_None, VT_None, 32, IT_AND)
INSTR1_MR(0x28, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_SUB)
INSTR1_MR(0x29, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_SUB)
INSTR1_MR(0x29, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_SUB)
INSTR1_RM(0x2A, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_SUB)
INSTR1_RM(0x2B, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_SUB)
INSTR1_RM(0x2B, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_SUB)
INSTR1_IA(0x2C, PS_None, VT_8, 8, IT_SUB)
INSTR1_IA(0x2D, PS_66, VT_16, 16, IT_SUB)
INSTR1_IA(0x2D, PS_None, VT_None, 32, IT_SUB)

INSTR1_MR(0x30, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_XOR)
INSTR1_MR(0x31, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_XOR)
INSTR1_MR(0x31, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_XOR)
INSTR1_RM(0x32, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_XOR)
INSTR1_RM(0x33, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_XOR)
INSTR1_RM(0x33, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_XOR)
INSTR1_IA(0x34, PS_None, VT_8, 8, IT_XOR)
INSTR1_IA(0x35, PS_66, VT_16, 16, IT_XOR)
INSTR1_IA(0x35, PS_None, VT_None, 32, IT_XOR)
INSTR1_MR(0x38, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_CMP)
INSTR1_MR(0x39, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_CMP)
INSTR1_MR(0x39, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_CMP)
INSTR1_RM(0x3A, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_CMP)
INSTR1_RM(0x3B, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_CMP)
INSTR1_RM(0x3B, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_CMP)
INSTR1_IA(0x3C, PS_None, VT_8, 8, IT_CMP)
INSTR1_IA(0x3D, PS_66, VT_16, 16, IT_CMP)
INSTR1_IA(0x3D, PS_None, VT_None, 32, IT_CMP)

INSTR1_O (0x50, PS_None, VT_64, IT_PUSH)
INSTR1_O (0x50, PS_66, VT_16, IT_PUSH)
INSTR1_O (0x58, PS_None, VT_64, IT_POP)
INSTR1_O (0x58, PS_66, VT_16, IT_POP)

INSTR1_RM(0x63, PS_None, RT_GG, VT_None, VT_32, VT_None, IT_MOVSX)
INSTR1_I (0x68, PS_None, VT_32, 32, IT_PUSH)
INSTR1_RMI(0x69, PS_None, RT_GG, VT_None, VT_None, VT_None, 32, IT_IMUL)
INSTR1_RMI(0x69, PS_66, RT_GG, VT_16, VT_16, VT_16, 16, IT_IMUL)
INSTR1_I (0x6A, PS_None, VT_8, 8, IT_PUSH)
INSTR1_RMI(0x6B, PS_None, RT_GG, VT_None, VT_None, VT_None, 8, IT_IMUL)
INSTR1_RMI(0x6B, PS_66, RT_GG, VT_16, VT_16, VT_16, 8, IT_IMUL)

INSTR1_Dcc(0x70, PS_None, VT_8, 8, IT_JO)

// Immediate Group 1, Intel Vol. 2C A-8
INSTR1_MI(0x80, PS_None, RT_GG, VT_8, VT_8, 0, 8, IT_ADD)
INSTR1_MI(0x80, PS_None, RT_GG, VT_8, VT_8, 1, 8, IT_OR)
INSTR1_MI(0x80, PS_None, RT_GG, VT_8, VT_8, 2, 8, IT_ADC)
INSTR1_MI(0x80, PS_None, RT_GG, VT_8, VT_8, 3, 8, IT_SBB)
INSTR1_MI(0x80, PS_None, RT_GG, VT_8, VT_8, 4, 8, IT_AND)
INSTR1_MI(0x80, PS_None, RT_GG, VT_8, VT_8, 5, 8, IT_SUB)
INSTR1_MI(0x80, PS_None, RT_GG, VT_8, VT_8, 6, 8, IT_XOR)
INSTR1_MI(0x80, PS_None, RT_GG, VT_8, VT_8, 7, 8, IT_CMP)
INSTR1_MI(0x81, PS_66, RT_GG, VT_16, VT_16, 0, 16, IT_ADD)
INSTR1_MI(0x81, PS_None, RT_GG, VT_None, VT_None, 0, 32, IT_ADD)
INSTR1_MI(0x81, PS_66, RT_GG, VT_16, VT_16, 1, 16, IT_OR)
INSTR1_MI(0x81, PS_None, RT_GG, VT_None, VT_None, 1, 32, IT_OR)
INSTR1_MI(0x81, PS_66, RT_GG, VT_16, VT_16, 2, 16, IT_ADC)
INSTR1_MI(0x81, PS_None, RT_GG, VT_None, VT_None, 2, 32, IT_ADC)
INSTR1_MI(0x81, PS_66, RT_GG, VT_16, VT_16, 3, 16, IT_SBB)
INSTR1_MI(0x81, PS_None, RT_GG, VT_None, VT_None, 3, 32, IT_SBB)
INSTR1_MI(0x81, PS_66, RT_GG, VT_16, VT_16, 4, 16, IT_AND)
INSTR1_MI(0x81, PS_None, RT_GG, VT_None, VT_None, 4, 32, IT_AND)
INSTR1_MI(0x81, PS_66, RT_GG, VT_16, VT_16, 5, 16, IT_SUB)
INSTR1_MI(0x81, PS_None, RT_GG, VT_None, VT_None, 5, 32, IT_SUB)
INSTR1_MI(0x81, PS_66, RT_GG, VT_16, VT_16, 6, 16, IT_XOR)
INSTR1_MI(0x81, PS_None, RT_GG, VT_None, VT_None, 6, 32, IT_XOR)
INSTR1_MI(0x81, PS_66, RT_GG, VT_16, VT_16, 7, 16, IT_CMP)
INSTR1_MI(0x81, PS_None, RT_GG, VT_None, VT_None, 7, 32, IT_CMP)
INSTR1_MI(0x83, PS_66, RT_GG, VT_16, VT_16, 0, 8, IT_ADD)
INSTR1_MI(0x83, PS_None, RT_GG, VT_None, VT_None, 0, 8, IT_ADD)
INSTR1_MI(0x83, PS_66, RT_GG, VT_16, VT_16, 1, 8, IT_OR)
INSTR1_MI(0x83, PS_None, RT_GG, VT_None, VT_None, 1, 8, IT_OR)
INSTR1_MI(0x83, PS_66, RT_GG, VT_16, VT_16, 2, 8, IT_ADC)
INSTR1_MI(0x83, PS_None, RT_GG, VT_None, VT_None, 2, 8, IT_ADC)
INSTR1_MI(0x83, PS_66, RT_GG, VT_16, VT_16, 3, 8, IT_SBB)
INSTR1_MI(0x83, PS_None, RT_GG, VT_None, VT_None, 3, 8, IT_SBB)
INSTR1_MI(0x83, PS_66, RT_GG, VT_16, VT_16, 4, 8, IT_AND)
INSTR1_MI(0x83, PS_None, RT_GG, VT_None, VT_None, 4, 8, IT_AND)
INSTR1_MI(0x83, PS_66, RT_GG, VT_16, VT_16, 5, 8, IT_SUB)
INSTR1_MI(0x83, PS_None, RT_GG, VT_None, VT_None, 5, 8, IT_SUB)
INSTR1_MI(0x83, PS_66, RT_GG, VT_16, VT_16, 6, 8, IT_XOR)
INSTR1_MI(0x83, PS_None, RT_GG, VT_None, VT_None, 6, 8, IT_XOR)
INSTR1_MI(0x83, PS_66, RT_GG, VT_16, VT_16, 7, 8, IT_CMP)
INSTR1_MI(0x83, PS_None, RT_GG, VT_None, VT_None, 7, 8, IT_CMP)

INSTR1_MR(0x84, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_TEST)
INSTR1_MR(0x85, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_TEST)
INSTR1_MR(0x85, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_TEST)
INSTR1_MR(0x88, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_MOV)
INSTR1_MR(0x89, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_MOV)
INSTR1_MR(0x89, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_MOV)
INSTR1_RM(0x8A, PS_None, RT_GG, VT_8, VT_8, VT_8, IT_MOV)
INSTR1_RM(0x8B, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_MOV)
INSTR1_RM(0x8B, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_MOV)
INSTR1_RM(0x8D, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_LEA)
INSTR1_RM(0x8D, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_LEA)
INSTR1_M (0x8F, PS_66, RT_GG, VT_16, VT_16, 0, IT_POP)
INSTR1_M (0x8F, PS_None, RT_GG, VT_64, VT_64, 0, IT_POP)
// INSTR1_fn(0x90, PS_None, decode_nop_simple)
INSTR1_NP(0x90, PS_None, VT_None, IT_NOP)
INSTR1_NP(0x98, PS_None, VT_Implicit, IT_CLTQ)
INSTR1_NP(0x99, PS_None, VT_Implicit, IT_CQTO)

INSTR1_IA(0xA8, PS_None, VT_8, 8, IT_TEST)
INSTR1_IA(0xA9, PS_66, VT_16, 16, IT_TEST)
INSTR1_IA(0xA9, PS_None, VT_None, 32, IT_TEST)

INSTR1_OI(0xB0, PS_None, VT_8, 8, IT_MOV)
INSTR1_OI(0xB8, PS_66, VT_16, 16, IT_MOV)
INSTR1_OI(0xB8, PS_None, VT_None, 0, IT_MOV)

// Shift Group 2, Intel Vol. 2C A-19
INSTR1_MI(0xC0, PS_None, RT_GG, VT_8, VT_8, 4, 8, IT_SHL)
INSTR1_MI(0xC1, PS_66, RT_GG, VT_16, VT_16, 4, 8, IT_SHL)
INSTR1_MI(0xC1, PS_None, RT_GG, VT_None, VT_None, 4, 8, IT_SHL)
INSTR1_MI(0xC0, PS_None, RT_GG, VT_8, VT_8, 5, 8, IT_SHR)
INSTR1_MI(0xC1, PS_66, RT_GG, VT_16, VT_16, 5, 8, IT_SHR)
INSTR1_MI(0xC1, PS_None, RT_GG, VT_None, VT_None, 5, 8, IT_SHR)
INSTR1_MI(0xC0, PS_None, RT_GG, VT_8, VT_8, 7, 8, IT_SAR)
INSTR1_MI(0xC1, PS_66, RT_GG, VT_16, VT_16, 7, 8, IT_SAR)
INSTR1_MI(0xC1, PS_None, RT_GG, VT_None, VT_None, 7, 8, IT_SAR)

INSTR1_NP(0xC3, PS_None, VT_None, IT_RET)
INSTR1_NP(0xC3, PS_F3, VT_None, IT_RET)
INSTR1_MI(0xC6, PS_None, RT_GG, VT_8, VT_8, 0, 8, IT_MOV)
INSTR1_MI(0xC7, PS_66, RT_GG, VT_16, VT_16, 0, 16, IT_MOV)
INSTR1_MI(0xC7, PS_None, RT_GG, VT_None, VT_None, 0, 32, IT_MOV)
INSTR1_NP(0xC9, PS_66, VT_16, IT_LEAVE)
INSTR1_NP(0xC9, PS_None, VT_None, IT_LEAVE)

// Shift Group 2, Intel Vol. 2C A-19
INSTR1_M1(0xD0, PS_None, RT_GG, VT_8, VT_8, 4, IT_SHL)
INSTR1_M1(0xD1, PS_66, RT_GG, VT_16, VT_16, 4, IT_SHL)
INSTR1_M1(0xD1, PS_None, RT_GG, VT_None, VT_None, 4, IT_SHL)
INSTR1_MC(0xD2, PS_None, RT_GG, VT_8, VT_8, 4, IT_SHL)
INSTR1_MC(0xD3, PS_66, RT_GG, VT_16, VT_16, 4, IT_SHL)
INSTR1_MC(0xD3, PS_None, RT_GG, VT_None, VT_None, 4, IT_SHL)
INSTR1_M1(0xD0, PS_None, RT_GG, VT_8, VT_8, 5, IT_SHR)
INSTR1_M1(0xD1, PS_66, RT_GG, VT_16, VT_16, 5, IT_SHR)
INSTR1_M1(0xD1, PS_None, RT_GG, VT_None, VT_None, 5, IT_SHR)
INSTR1_MC(0xD2, PS_None, RT_GG, VT_8, VT_8, 5, IT_SHR)
INSTR1_MC(0xD3, PS_66, RT_GG, VT_16, VT_16, 5, IT_SHR)
INSTR1_MC(0xD3, PS_None, RT_GG, VT_None, VT_None, 5, IT_SHR)
INSTR1_M1(0xD0, PS_None, RT_GG, VT_8, VT_8, 7, IT_SAR)
INSTR1_M1(0xD1, PS_66, RT_GG, VT_16, VT_16, 7, IT_SAR)
INSTR1_M1(0xD1, PS_None, RT_GG, VT_None, VT_None, 7, IT_SAR)
INSTR1_MC(0xD2, PS_None, RT_GG, VT_8, VT_8, 7, IT_SAR)
INSTR1_MC(0xD3, PS_66, RT_GG, VT_16, VT_16, 7, IT_SAR)
INSTR1_MC(0xD3, PS_None, RT_GG, VT_None, VT_None, 7, IT_SAR)

INSTR1_D (0xE8, PS_None, VT_32, 32, IT_CALL)
INSTR1_D (0xE9, PS_None, VT_32, 32, IT_JMP)
INSTR1_D (0xEB, PS_None, VT_8, 8, IT_JMP)
INSTR1_MI(0xF6, PS_None, RT_GG, VT_8, VT_8, 0, 8, IT_TEST)
INSTR1_M (0xF6, PS_None, RT_GG, VT_8, VT_8, 2, IT_NOT)
INSTR1_M (0xF6, PS_None, RT_GG, VT_8, VT_8, 3, IT_NEG)
INSTR1_M (0xF6, PS_None, RT_GG, VT_8, VT_8, 4, IT_MUL)
INSTR1_M (0xF6, PS_None, RT_GG, VT_8, VT_8, 5, IT_IMUL)
INSTR1_M (0xF6, PS_None, RT_GG, VT_8, VT_8, 6, IT_DIV)
INSTR1_M (0xF6, PS_None, RT_GG, VT_8, VT_8, 7, IT_IDIV1)
INSTR1_MI(0xF7, PS_66, RT_GG, VT_16, VT_16, 0, 16, IT_TEST)
INSTR1_MI(0xF7, PS_None, RT_GG, VT_Implicit, VT_None, 0, 32, IT_TEST)
INSTR1_M (0xF7, PS_66, RT_GG, VT_16, VT_16, 2, IT_NOT)
INSTR1_M (0xF7, PS_None, RT_GG, VT_Implicit, VT_None, 2, IT_NOT)
INSTR1_M (0xF7, PS_66, RT_GG, VT_16, VT_16, 3, IT_NEG)
INSTR1_M (0xF7, PS_None, RT_GG, VT_Implicit, VT_None, 3, IT_NEG)
INSTR1_M (0xF7, PS_66, RT_GG, VT_16, VT_16, 4, IT_MUL)
INSTR1_M (0xF7, PS_None, RT_GG, VT_Implicit, VT_None, 4, IT_MUL)
INSTR1_M (0xF7, PS_66, RT_GG, VT_16, VT_16, 5, IT_IMUL)
INSTR1_M (0xF7, PS_None, RT_GG, VT_Implicit, VT_None, 5, IT_IMUL)
INSTR1_M (0xF7, PS_66, RT_GG, VT_16, VT_16, 6, IT_DIV)
INSTR1_M (0xF7, PS_None, RT_GG, VT_Implicit, VT_None, 6, IT_DIV)
INSTR1_M (0xF7, PS_66, RT_GG, VT_16, VT_16, 7, IT_IDIV1)
INSTR1_M (0xF7, PS_None, RT_GG, VT_Implicit, VT_None, 7, IT_IDIV1)

INSTR1_M (0xFE, PS_None, RT_GG, VT_8, VT_8, 0, IT_INC)
INSTR1_M (0xFE, PS_None, RT_GG, VT_8, VT_8, 1, IT_DEC)
INSTR1_M (0xFF, PS_None, RT_GG, VT_Implicit, VT_None, 0, IT_INC)
INSTR1_M (0xFF, PS_66, RT_GG, VT_16, VT_16, 0, IT_INC)
INSTR1_M (0xFF, PS_None, RT_GG, VT_Implicit, VT_None, 1, IT_DEC)
INSTR1_M (0xFF, PS_66, RT_GG, VT_16, VT_16, 1, IT_DEC)
INSTR1_M (0xFF, PS_None, RT_GG, VT_64, VT_64, 2, IT_CALL)
INSTR1_M (0xFF, PS_None, RT_GG, VT_64, VT_64, 4, IT_JMPI)
INSTR1_M (0xFF, PS_66, RT_GG, VT_16, VT_16, 6, IT_PUSH)
INSTR1_M (0xFF, PS_None, RT_GG, VT_64, VT_64, 6, IT_PUSH)

INSTR2_M (0x1F, PS_None, RT_VV, VT_None, VT_None, 0, IT_NOP)
INSTR2_M (0x1F, PS_66, RT_VV, VT_16, VT_16, 0, IT_NOP)
INSTR2_RMcc(0x40, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_CMOVO)
INSTR2_RMcc(0x40, PS_None, RT_GG, VT_None, VT_None, VT_None, IT_CMOVO)
INSTR2_Dcc(0x80, PS_None, VT_32, 32, IT_JO)
INSTR2_Mcc(0x90, PS_None, RT_GG, VT_8, VT_Implicit, -1, IT_SETO)
INSTR2_RM(0xAF, PS_66, RT_GG, VT_16, VT_16, VT_Implicit, IT_IMUL)
INSTR2_RM(0xAF, PS_None, RT_GG, VT_None, VT_None, VT_Implicit, IT_IMUL)
INSTR2_RM(0xB6, PS_66, RT_GG, VT_16, VT_8, VT_16, IT_MOVZX)
INSTR2_RM(0xB6, PS_None, RT_GG, VT_None, VT_8, VT_None, IT_MOVZX)
INSTR2_RM(0xB7, PS_None, RT_GG, VT_None, VT_16, VT_None, IT_MOVZX)
INSTR2_RM(0xBC, PS_66, RT_GG, VT_16, VT_16, VT_16, IT_BSF)
INSTR2_RM(0xBC, PS_None, RT_GG, VT_None, VT_None, VT_Implicit, IT_BSF)
INSTR2_RM(0xBE, PS_66, RT_GG, VT_16, VT_8, VT_16, IT_MOVSX)
INSTR2_RM(0xBE, PS_None, RT_GG, VT_None, VT_8, VT_None, IT_MOVSX)
INSTR2_RM(0xBF, PS_None, RT_GG, VT_None, VT_16, VT_None, IT_MOVSX)

// SSE Instructions
INSTR2_RM(0x10, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_MOVSS)
INSTR2_RM(0x10, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVSD)
INSTR2_RM(0x10, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVUPS)
INSTR2_RM(0x10, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVUPD)
INSTR2_MR(0x11, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_MOVSS)
INSTR2_MR(0x11, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVSD)
INSTR2_MR(0x11, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVUPS)
INSTR2_MR(0x11, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVUPD)
INSTR2_RM(0x12, PS_None, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVLPS)
INSTR2_RM(0x12, PS_66, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVLPD)
INSTR2_MR(0x13, PS_None, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVLPS)
INSTR2_MR(0x13, PS_66, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVLPD)
INSTR2_RM(0x14, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_UNPCKLPS)
INSTR2_RM(0x14, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_UNPCKLPD)
INSTR2_RM(0x15, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_UNPCKHPS)
INSTR2_RM(0x15, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_UNPCKHPD)
INSTR2_RM(0x16, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVHPS)
INSTR2_RM(0x16, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVHPD)
INSTR2_MR(0x17, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVHPS)
INSTR2_MR(0x17, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVHPD)
INSTR2_RM(0x28, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVAPS)
INSTR2_RM(0x28, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVAPD)
INSTR2_MR(0x29, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVAPS)
INSTR2_MR(0x29, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVAPD)
INSTR2_MR(0x2E, PS_None, RT_VV, VT_32, VT_32, VT_Implicit, IT_UCOMISS)
INSTR2_MR(0x2E, PS_66, RT_VV, VT_64, VT_64, VT_Implicit, IT_UCOMISD)
INSTR2_MR(0x2F, PS_None, RT_VV, VT_32, VT_32, VT_Implicit, IT_COMISS)
INSTR2_MR(0x2F, PS_66, RT_VV, VT_64, VT_64, VT_Implicit, IT_COMISD)
INSTR2_RM(0x51, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_SQRTSS)
INSTR2_RM(0x51, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_SQRTSD)
INSTR2_RM(0x51, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_SQRTPS)
INSTR2_RM(0x51, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_SQRTPD)
INSTR2_RM(0x52, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_RSQRTSS)
INSTR2_RM(0x52, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_RSQRTPS)
INSTR2_RM(0x53, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_RCPSS)
INSTR2_RM(0x53, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_RCPPS)
INSTR2_RM(0x54, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_ANDPS)
INSTR2_RM(0x54, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_ANDPD)
INSTR2_RM(0x55, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_ANDNPS)
INSTR2_RM(0x55, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_ANDNPD)
INSTR2_RM(0x56, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_ORPS)
INSTR2_RM(0x56, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_ORPD)
INSTR2_RM(0x57, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_XORPS)
INSTR2_RM(0x57, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_XORPD)
INSTR2_RM(0x58, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_ADDSS)
INSTR2_RM(0x58, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_ADDSD)
INSTR2_RM(0x58, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_ADDPS)
INSTR2_RM(0x58, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_ADDPD)
INSTR2_RM(0x59, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_MULSS)
INSTR2_RM(0x59, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_MULSD)
INSTR2_RM(0x59, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MULPS)
INSTR2_RM(0x59, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MULPD)
INSTR2_RM(0x5C, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_SUBSS)
INSTR2_RM(0x5C, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_SUBSD)
INSTR2_RM(0x5C, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_SUBPS)
INSTR2_RM(0x5C, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_SUBPD)
INSTR2_RM(0x5D, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_MINSS)
INSTR2_RM(0x5D, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_MINSD)
INSTR2_RM(0x5D, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MINPS)
INSTR2_RM(0x5D, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MINPD)
INSTR2_RM(0x5E, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_DIVSS)
INSTR2_RM(0x5E, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_DIVSD)
INSTR2_RM(0x5E, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_DIVPS)
INSTR2_RM(0x5E, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_DIVPD)
INSTR2_RM(0x5F, PS_F3, RT_VV, VT_32, VT_32, VT_Implicit, IT_MAXSS)
INSTR2_RM(0x5F, PS_F2, RT_VV, VT_64, VT_64, VT_Implicit, IT_MAXSD)
INSTR2_RM(0x5F, PS_None, RT_VV, VT_128, VT_128, VT_Implicit, IT_MAXPS)
INSTR2_RM(0x5F, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MAXPD)
INSTR2_RM(0x6E, PS_66, RT_VG, VT_None, VT_None, VT_None, IT_MOVQ)
INSTR2_RM(0x6F, PS_F3, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVDQU)
INSTR2_RM(0x6F, PS_None, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVQ)
INSTR2_RM(0x6F, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVDQA)
INSTR2_RM(0x74, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_PCMPEQB)
INSTR2_RM(0x75, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_PCMPEQW)
INSTR2_RM(0x76, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_PCMPEQD)
INSTR2_RM(0x7C, PS_F2, RT_VV, VT_128, VT_128, VT_Implicit, IT_HADDPS)
INSTR2_RM(0x7C, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_HADDPD)
INSTR2_RM(0x7D, PS_F2, RT_VV, VT_128, VT_128, VT_Implicit, IT_HSUBPS)
INSTR2_RM(0x7D, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_HSUBPD)
INSTR2_RM(0x7E, PS_F3, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVQ)
INSTR2_MR(0x7E, PS_66, RT_GV, VT_None, VT_None, VT_None, IT_MOVQ)
INSTR2_MR(0x7F, PS_F3, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVDQU)
INSTR2_MR(0x7F, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_MOVDQA)
INSTR2_RM(0xD0, PS_F2, RT_VV, VT_128, VT_128, VT_Implicit, IT_ADDSUBPS)
INSTR2_RM(0xD0, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_ADDSUBPD)
INSTR2_RM(0xD4, PS_None, RT_VV, VT_64, VT_64, VT_Implicit, IT_PADDQ)
INSTR2_RM(0xD4, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_PADDQ)
INSTR2_MR(0xD6, PS_66, RT_VV, VT_64, VT_64, VT_Implicit, IT_MOVQ)
INSTR2_RM(0xDA, PS_None, RT_VV, VT_64, VT_64, VT_Implicit, IT_PMINUB)
INSTR2_RM(0xDA, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_PMINUB)
INSTR2_RM(0xEF, PS_None, RT_VV, VT_64, VT_64, VT_Implicit, IT_PXOR)
INSTR2_RM(0xEF, PS_66, RT_VV, VT_128, VT_128, VT_Implicit, IT_PXOR)

// Terminator
{ OE_Invalid, 0, {0, 0, 0}, 0, RT_GG, 0, 0, 0, -1, 0, false, IT_Invalid, NULL }
};
