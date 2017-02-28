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

/* For now, decoded instructions in DBrew are x86-64 */

#ifndef INSTR_H
#define INSTR_H

#include "dbrew.h"
#include "expr.h"

#include <stdint.h>

/**
 * Declarations used to store decoded instructions
 *
 * This currently is Intel64-specific (also called amd64 / x86_64)
 */


/* A register is identified by a register type and an index into an array
 * of similar registers.
 *
 * Types are related to how they are used in instruction encodings, with the
 * indexes being part of the encoding. Registers of different type may
 * overlap each other.
*/
typedef enum _RegType {
    RT_None = 0,
    RT_GP8Leg, // general purpose 8bit 80x86 legacy registers (8 regs)
    RT_GP8,    // low 8 bits of 64bit general purpose registers (16 regs)
    RT_GP16,   // low 16 bits of 64bit general purpose registers (16 regs)
    RT_GP32,   // low 32 bits of 64bit general purpose registers (16 regs)
    RT_GP64,   // 64bit general purpose registers (16 regs)
    RT_Flag,   // 1-bit, part of EFLAGS register
    RT_IP,     // own type due to its speciality: instruction pointer (1 reg)
    RT_X87,    // 80-bit floating point registers (8 regs in x87 FP stack)
    RT_MMX,    // 64bit MMX vector registers (8 regs: mm0 - mm7)
    RT_XMM,    // 128bit SSE vector registers (16 regs: xmm0 - xmm15)
    RT_YMM,    // 256bit AVX vector registers (16 regs: ymm0 - ymm15)
    RT_ZMM,    // 512bit AVX512 vector registers (32 regs: zmm0 - zmm31)
    RT_Max
} RegType;

// Names for register indexes. Warning: indexes for different types overlap!
typedef enum _RegIndex {
    RI_None = 100, // assume no register type has more than 100 regs

    // for RT_GP8Leg (1st 8 from x86, but can address 16 regs in 64bit mode)
    RI_AL = 0, RI_CL, RI_DL, RI_BL, RI_AH, RI_CH, RI_DH, RI_BH,
    RI_R8L, RI_9L, RI_10L, RI_11L, RI_12L, RI_13L, RI_14L, RI_15L,

    // for RT_GP{8,16,32,64}, order according to usage in encoding
    RI_A = 0, RI_C, RI_D, RI_B, RI_SP, RI_BP, RI_SI, RI_DI,
    RI_8, RI_9, RI_10, RI_11, RI_12, RI_13, RI_14, RI_15,
    RI_GPMax, // useful for allocation of GP register space

    // for RT_Flag
    RI_Carry = 0, RI_Zero, RI_Sign, RI_Overflow, RI_Parity,
    RI_FlMax,

    // for RT_X87 FPU register stack
    RI_ST0 = 0, RI_ST1, RI_ST2, RI_ST3, RI_ST4, RI_ST5, RI_ST6, RI_ST7,
    RI_STMax,

    // for RT_MMX
    RI_MM0 = 0, RI_MM1, RI_MM2, RI_MM3, RI_MM4, RI_MM5, RI_MM6, RI_MM7,
    RI_MMMax,

    // for RT_XMM SSE
    RI_XMM0 = 0, RI_XMM1, RI_XMM2, RI_XMM3,
    RI_XMM4, RI_XMM5, RI_XMM6, RI_XMM7,
    RI_XMM8, RI_XMM9, RI_XMM10, RI_XMM11,
    RI_XMM12, RI_XMM13, RI_XMM14, RI_XMM15,
    RI_XMMMax,

    // for RT_YMM AVX
    RI_YMM0 = 0, RI_YMM1, RI_YMM2, RI_YMM3,
    RI_YMM4, RI_YMM5, RI_YMM6, RI_YMM7,
    RI_YMM8, RI_YMM9, RI_YMM10, RI_YMM11,
    RI_YMM12, RI_YMM13, RI_YMM14, RI_YMM15,
    RI_YMMMax,

    // for RT_ZMM AVX512
    RI_ZMM0 = 0, RI_ZMM1, RI_ZMM2, RI_ZMM3,
    RI_ZMM4, RI_ZMM5, RI_ZMM6, RI_ZMM7,
    RI_ZMM8, RI_ZMM9, RI_ZMM10, RI_ZMM11,
    RI_ZMM12, RI_ZMM13, RI_ZMM14, RI_ZMM15,
    RI_ZMM16, RI_ZMM17, RI_ZMM18, RI_ZMM19,
    RI_ZMM20, RI_ZMM21, RI_ZMM22, RI_ZMM23,
    RI_ZMM24, RI_ZMM25, RI_ZMM26, RI_ZMM27,
    RI_ZMM28, RI_ZMM29, RI_ZMM30, RI_ZMM31,
    RI_ZMMMax,
} RegIndex;

typedef struct _Reg
{
    RegType rt;
    RegIndex ri;
} Reg;


// enum for instruction types, based on Intel SDM
typedef enum _InstrType {
    IT_None = 0, IT_Invalid,
    // Hints: not actual instructions
    IT_HINT_CALL, // starting inlining of another function at this point
    IT_HINT_RET,  // ending inlining at this point
    //
    IT_NOP,
    IT_CLTQ, IT_CWTL, IT_CQTO,
    IT_PUSH, IT_PUSHF, IT_PUSHFQ, IT_POP, IT_POPF, IT_POPFQ, IT_LEAVE,
    IT_MOV, IT_MOVD, IT_MOVQ, IT_MOVSX, IT_LEA, IT_MOVZX,
    IT_NEG, IT_NOT, IT_INC, IT_DEC,
    IT_ADD, IT_ADC, IT_SUB, IT_SBB,
    IT_IMUL, IT_IDIV1, IT_MUL, IT_DIV,
    IT_XOR, IT_AND, IT_OR,
    IT_CMP, IT_TEST, IT_BSF,
    IT_SHL, IT_SHR, IT_SAR,

    IT_CALL, IT_RET, IT_JMP, IT_JMPI,

    IT_JO, IT_JNO, IT_JC, IT_JNC, IT_JZ, IT_JNZ, IT_JBE, IT_JA,
    IT_JS, IT_JNS, IT_JP, IT_JNP, IT_JL, IT_JGE, IT_JLE, IT_JG,

    IT_CMOVO, IT_CMOVNO, IT_CMOVC, IT_CMOVNC,
    IT_CMOVZ, IT_CMOVNZ, IT_CMOVBE, IT_CMOVA,
    IT_CMOVS, IT_CMOVNS, IT_CMOVP, IT_CMOVNP,
    IT_CMOVL, IT_CMOVGE, IT_CMOVLE, IT_CMOVG,

    IT_SETO, IT_SETNO, IT_SETC, IT_SETNC, IT_SETZ, IT_SETNZ, IT_SETBE, IT_SETA,
    IT_SETS, IT_SETNS, IT_SETP, IT_SETNP, IT_SETL, IT_SETGE, IT_SETLE, IT_SETG,

    // SSE Move
    IT_MOVSS, IT_MOVSD, IT_MOVUPS, IT_MOVUPD, IT_MOVAPS, IT_MOVAPD,
    IT_MOVDQU, IT_MOVDQA, IT_MOVLPD, IT_MOVLPS, IT_MOVHPD, IT_MOVHPS,
    // SSE Unpack
    IT_UNPCKLPS, IT_UNPCKLPD, IT_UNPCKHPS, IT_UNPCKHPD,
    // SSE FP arithmetic
    IT_ADDSS, IT_ADDSD, IT_ADDPS, IT_ADDPD,
    IT_SUBSS, IT_SUBSD, IT_SUBPS, IT_SUBPD,
    IT_MULSS, IT_MULSD, IT_MULPS, IT_MULPD,
    IT_DIVSS, IT_DIVSD, IT_DIVPS, IT_DIVPD,
    IT_XORPS, IT_XORPD, IT_ORPS, IT_ORPD,
    IT_ANDPS, IT_ANDPD, IT_ANDNPS, IT_ANDNPD,
    IT_MAXSS, IT_MAXSD, IT_MAXPS, IT_MAXPD,
    IT_MINSS, IT_MINSD, IT_MINPS, IT_MINPD,
    IT_SQRTSS, IT_SQRTSD, IT_SQRTPS, IT_SQRTPD,
    IT_COMISS, IT_COMISD, IT_UCOMISS, IT_UCOMISD,
    IT_ADDSUBPS, IT_ADDSUBPD,
    IT_HADDPS, IT_HADDPD,
    IT_HSUBPS, IT_HSUBPD,
    IT_RCPSS, IT_RCPPS,
    IT_RSQRTSS, IT_RSQRTPS,
    // SSE Integer operations
    IT_PCMPEQB, IT_PCMPEQW, IT_PCMPEQD,
    IT_PMINUB, IT_PMOVMSKB, IT_PXOR, IT_PADDQ,

    // AVX
    IT_VMOVSS, IT_VMOVSD, IT_VMOVUPS, IT_VMOVUPD, IT_VMOVAPS, IT_VMOVAPD, IT_VMOVDQU,
    IT_VMOVDQA, IT_VMOVNTDQ,
    IT_VADDSS, IT_VADDSD, IT_VADDPS, IT_VADDPD,
    IT_VMULSS, IT_VMULSD, IT_VMULPS, IT_VMULPD,
    IT_VXORPS, IT_VXORPD,
    IT_VZEROUPPER, IT_VZEROALL,

    //
    IT_Max
} InstrType;

typedef enum _ValType {
    VT_None = 0,
    VT_1, VT_8, VT_16, VT_32, VT_64, VT_80, VT_128, VT_256, VT_512,

    // used in decoder and printer
    VT_Implicit, // type depends only on opcode, with Instr.vtype
    // only for decoder
    VT_Def,           // default op type (32, 64 with RexW, 16 with Pr66)
    VT_Implicit_REXW, // for pass-through: op-independent RexW marker

    VT_Max
} ValType;

typedef enum _OpType {
    OT_None = 0,
    OT_Imm8, OT_Imm16, OT_Imm32, OT_Imm64,
    OT_Reg8, OT_Reg16, OT_Reg32, OT_Reg64, OT_Reg128, OT_Reg256, OT_Reg512,
    // mem (64bit addr): register indirect + displacement
    OT_Ind8, OT_Ind16, OT_Ind32, OT_Ind64, OT_Ind128, OT_Ind256, OT_Ind512,
    //
    OT_MAX
} OpType;

typedef enum _OpSegOverride {
    OSO_None = 0, OSO_UseFS, OSO_UseGS
} OpSegOverride;

typedef enum _VexPrefix {
    VEX_No = 0,
    VEX_128, // Vex, length L=0: 128 bit
    VEX_256, // Vex, length L=1: 256 bit
    VEX_LIG, // Vex, ignore L setting (used in decoder)
} VexPrefix;

typedef struct _Operand {
    uint64_t val; // imm or displacement
    OpType type;
    Reg reg;
    Reg ireg; // with SIB
    int scale; // with SIB
    OpSegOverride seg; // with OP_Ind type
} Operand;

typedef enum _OperandEncoding {
    OE_Invalid = 0,
    OE_None,
    OE_MR,  // 2 operands, ModRM byte, dest is reg or memory
    OE_RM,  // 2 operands, ModRM byte, src  is reg or memory
    OE_RMI, // 3 operands, ModRM byte, src  is reg or memory, Immediate
    OE_RVM  // 3 operands, 2nd op is VEX vvvv reg
} OperandEncoding;

typedef enum _PrefixSet {
    PS_No = 0,
    PS_66 = 2,
    PS_F2 = 4,
    PS_F3 = 8,
    PS_2E = 16,
    PS_REXW = 32  // only used for pass-through
} PrefixSet;

typedef enum _OperandForm {
    OF_None = 0,
    OF_0, // no operand or implicit
    OF_1, // 1 operand: push/pop/... dst
    OF_2, // 2 operands: dst = dst op src
    OF_3, // 3 operands: dst = src op src2
    OF_Max
} OperandForm;

// information about capture state changes in Pass-Through instructions
typedef enum _StateChange {
    SC_None = 0,
    SC_dstDyn // operand dst is valid, should change to dynamic
} StateChange;

struct _Instr {
    InstrType type;

    OperandForm form;
    ValType vtype; // without explicit operands or all operands of same type
    Operand dst, src; //  with binary op: dst = dst op src
    Operand src2; // with ternary op: dst = src op src2

    // if instruction was decoded
    uint64_t addr;
    int len;

    // annotation for pass-through (not used when ptLen == 0)
    int ptLen;
    VexPrefix ptVexP;
    PrefixSet ptPSet;
    uint8_t ptOpc[3];
    OperandEncoding ptEnc;
    StateChange ptSChange;


    ExprNode* info_memAddr; // annotate memory reference of instr
};

RegType getGPRegType(ValType vt);
RegType getLegGPRegType(ValType vt);
RegType getVRegType(ValType vt);
ValType regValTypeT(RegType rt);
ValType regValType(Reg r);
bool regTypeIsGP(RegType rt);
bool regTypeIsV(RegType rt);
bool regIsGP(Reg r);
bool regIsV(Reg r);
RegIndex regGP64Index(Reg r);
RegIndex regVIndex(Reg r);
Reg getReg(RegType rt, RegIndex ri);

ValType opValType(Operand* o);
int opTypeWidth(Operand* o);
bool opIsImm(Operand* o);
bool opIsReg(Operand* o);
bool opIsGPReg(Operand* o);
bool opIsVReg(Operand* o);
bool opIsInd(Operand* o);

bool regIsEqual(Reg r1, Reg r2);
bool opIsEqual(Operand* o1, Operand* o2);

OpType getImmOpType(ValType t);
OpType getGPRegOpType(ValType t);

void setRegOp(Operand* o, Reg r);
Operand* getRegOp(Reg r);      // returns pointer to static object
Operand* getImmOp(ValType t, uint64_t v); // returns pointer to static object

void copyOperand(Operand* dst, Operand* src);
void opOverwriteType(Operand* o, ValType vt);
bool instrIsJcc(InstrType it);

void copyInstr(Instr* dst, Instr* src);
void initSimpleInstr(Instr* i, InstrType it);
void initUnaryInstr(Instr* i, InstrType it, Operand* o);
void initBinaryInstr(Instr* i, InstrType it, ValType vt,
                     Operand *o1, Operand *o2);
void initTernaryInstr(Instr* i, InstrType it,
                      Operand *o1, Operand *o2, Operand* o3);
void attachPassthrough(Instr* i, VexPrefix vp, PrefixSet set,
                       OperandEncoding enc, StateChange sc,
                       int b1, int b2, int b3);

#endif // INSTR_H
