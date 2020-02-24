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

#include "printer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "instr.h"
#include "common.h"


static
bool opTypeVisible(Operand* o)
{
    switch(o->type) {
    case OT_Reg8:
    case OT_Reg16:
    case OT_Reg32:
    case OT_Reg64:
    case OT_Reg128:
    case OT_Reg256:
        return true;
    default: break;
    }
    return false;
}

// get register name by index
const char* regNameI(RegType rt, RegIndex ri)
{
    switch(rt) {

    case RT_IP:
        return "rip";

    case RT_GP8:
        switch(ri) {
        case RI_A: return "al";
        case RI_B: return "bl";
        case RI_C: return "cl";
        case RI_D: return "dl";
        case RI_DI: return "dil";
        case RI_SI: return "sil";
        case RI_BP: return "bpl";
        case RI_SP: return "spl";
        case RI_8:  return "r8b";
        case RI_9:  return "r9b";
        case RI_10: return "r10b";
        case RI_11: return "r11b";
        case RI_12: return "r12b";
        case RI_13: return "r13b";
        case RI_14: return "r14b";
        case RI_15: return "r15b";
        default: assert(0);
        }
        break;

    case RT_GP8Leg:
        switch(ri) {
        case RI_AL: return "al";
        case RI_BL: return "bl";
        case RI_CL: return "cl";
        case RI_DL: return "dl";
        case RI_AH: return "ah";
        case RI_BH: return "bh";
        case RI_CH: return "ch";
        case RI_DH: return "dh";
        case RI_8:  return "r8b";
        case RI_9:  return "r9b";
        case RI_10: return "r10b";
        case RI_11: return "r11b";
        case RI_12: return "r12b";
        case RI_13: return "r13b";
        case RI_14: return "r14b";
        case RI_15: return "r15b";
        default: assert(0);
        }
        break;

    case RT_GP16:
        switch(ri) {
        case RI_A: return "ax";
        case RI_B: return "bx";
        case RI_C: return "cx";
        case RI_D: return "dx";
        case RI_DI: return "di";
        case RI_SI: return "si";
        case RI_BP: return "bp";
        case RI_SP: return "sp";
        case RI_8:  return "r8w";
        case RI_9:  return "r9w";
        case RI_10: return "r10w";
        case RI_11: return "r11w";
        case RI_12: return "r12w";
        case RI_13: return "r13w";
        case RI_14: return "r14w";
        case RI_15: return "r15w";
        default: assert(0);
        }
        break;

    case RT_GP32:
        switch(ri) {
        case RI_A: return "eax";
        case RI_B: return "ebx";
        case RI_C: return "ecx";
        case RI_D: return "edx";
        case RI_DI: return "edi";
        case RI_SI: return "esi";
        case RI_BP: return "ebp";
        case RI_SP: return "esp";
        case RI_8:  return "r8d";
        case RI_9:  return "r9d";
        case RI_10: return "r10d";
        case RI_11: return "r11d";
        case RI_12: return "r12d";
        case RI_13: return "r13d";
        case RI_14: return "r14d";
        case RI_15: return "r15d";
        default: assert(0);
        }
        break;

    case RT_GP64:
        switch(ri) {
        case RI_A: return "rax";
        case RI_B: return "rbx";
        case RI_C: return "rcx";
        case RI_D: return "rdx";
        case RI_DI: return "rdi";
        case RI_SI: return "rsi";
        case RI_BP: return "rbp";
        case RI_SP: return "rsp";
        case RI_8:  return "r8";
        case RI_9:  return "r9";
        case RI_10: return "r10";
        case RI_11: return "r11";
        case RI_12: return "r12";
        case RI_13: return "r13";
        case RI_14: return "r14";
        case RI_15: return "r15";
        default: assert(0);
        }
        break;

    case RT_MMX:
        switch(ri) {
        case RI_MM0:  return "mm0";
        case RI_MM1:  return "mm1";
        case RI_MM2:  return "mm2";
        case RI_MM3:  return "mm3";
        case RI_MM4:  return "mm4";
        case RI_MM5:  return "mm5";
        case RI_MM6:  return "mm6";
        case RI_MM7:  return "mm7";
        default: assert(0);
        }
        break;

    case RT_XMM:
        switch(ri) {
        case RI_XMM0:  return "xmm0";
        case RI_XMM1:  return "xmm1";
        case RI_XMM2:  return "xmm2";
        case RI_XMM3:  return "xmm3";
        case RI_XMM4:  return "xmm4";
        case RI_XMM5:  return "xmm5";
        case RI_XMM6:  return "xmm6";
        case RI_XMM7:  return "xmm7";
        case RI_XMM8:  return "xmm8";
        case RI_XMM9:  return "xmm9";
        case RI_XMM10: return "xmm10";
        case RI_XMM11: return "xmm11";
        case RI_XMM12: return "xmm12";
        case RI_XMM13: return "xmm13";
        case RI_XMM14: return "xmm14";
        case RI_XMM15: return "xmm15";
        default: assert(0);
        }
        break;

    case RT_YMM:
        switch(ri) {
        case RI_YMM0:  return "ymm0";
        case RI_YMM1:  return "ymm1";
        case RI_YMM2:  return "ymm2";
        case RI_YMM3:  return "ymm3";
        case RI_YMM4:  return "ymm4";
        case RI_YMM5:  return "ymm5";
        case RI_YMM6:  return "ymm6";
        case RI_YMM7:  return "ymm7";
        case RI_YMM8:  return "ymm8";
        case RI_YMM9:  return "ymm9";
        case RI_YMM10: return "ymm10";
        case RI_YMM11: return "ymm11";
        case RI_YMM12: return "ymm12";
        case RI_YMM13: return "ymm13";
        case RI_YMM14: return "ymm14";
        case RI_YMM15: return "ymm15";
        default: assert(0);
        }
        break;

    case RT_ZMM:
        switch(ri) {
        case RI_ZMM0:  return "zmm0";
        case RI_ZMM1:  return "zmm1";
        case RI_ZMM2:  return "zmm2";
        case RI_ZMM3:  return "zmm3";
        case RI_ZMM4:  return "zmm4";
        case RI_ZMM5:  return "zmm5";
        case RI_ZMM6:  return "zmm6";
        case RI_ZMM7:  return "zmm7";
        case RI_ZMM8:  return "zmm8";
        case RI_ZMM9:  return "zmm9";
        case RI_ZMM10: return "zmm10";
        case RI_ZMM11: return "zmm11";
        case RI_ZMM12: return "zmm12";
        case RI_ZMM13: return "zmm13";
        case RI_ZMM14: return "zmm14";
        case RI_ZMM15: return "zmm15";
        case RI_ZMM16: return "zmm16";
        case RI_ZMM17: return "zmm17";
        case RI_ZMM18: return "zmm18";
        case RI_ZMM19: return "zmm19";
        case RI_ZMM20: return "zmm20";
        case RI_ZMM21: return "zmm21";
        case RI_ZMM22: return "zmm22";
        case RI_ZMM23: return "zmm23";
        case RI_ZMM24: return "zmm24";
        case RI_ZMM25: return "zmm25";
        case RI_ZMM26: return "zmm26";
        case RI_ZMM27: return "zmm27";
        case RI_ZMM28: return "zmm28";
        case RI_ZMM29: return "zmm29";
        case RI_ZMM30: return "zmm30";
        case RI_ZMM31: return "zmm31";

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return "(unknown)";
}

const char* regName(Reg r)
{
    return regNameI(r.rt, r.ri);
}


char* prettyAddress(uint64_t a, FunctionConfig* fc)
{
    static char buf[100];

    if (fc) {
        // use name from registered, labeled memory ranges
        MemRangeConfig* mrc;

        assert(fc->cc);
        mrc = fc->cc->range_configs;
        while(mrc) {
            if (mrc->name) {
                if (a == mrc->start) {
                    sprintf(buf, "%s", mrc->name);
                    return buf;
                }
                else if ((a > mrc->start) && (a < mrc->start + mrc->size)) {
                    sprintf(buf, "%s+%ld", mrc->name, a - mrc->start);
                    return buf;
                }
            }
            mrc = mrc->next;
        }
    }
    sprintf(buf, "0x%lx", a);
    return buf;
}

// if <fc> is not-null, use it to print immediates/displacement
char* op2string(Operand* o, Instr* instr, FunctionConfig* fc)
{
    static char buf[30];
    int off = 0;
    ValType t = instr->vtype;
    uint64_t val;

    switch(o->type) {
    case OT_Reg8:
    case OT_Reg16:
    case OT_Reg32:
    case OT_Reg64:
    case OT_Reg128:
    case OT_Reg256:
    case OT_Reg512:
        off += sprintf(buf, "%%%s", regName(o->reg));
        break;

    case OT_Imm8:
        val = o->val;
        assert(val < (1l<<8));
        switch(t) {
        case VT_None:
        case VT_8:
            break;
        case VT_16:
        case VT_32:
            if (val > 0x7F) val += 0xFFFFFF00;
            break;
        case VT_64:
            if (val > 0x7F) val += 0xFFFFFFFFFFFFFF00;
            break;
        default: assert(0);
        }
        off += sprintf(buf, "$0x%lx", val);
        break;

    case OT_Imm16:
        val = o->val;
        assert(val < (1l<<16));
        switch(t) {
        case VT_32:
            if (val > 0x7FFF) val += 0xFFFF0000;
            break;
        case VT_64:
            if (val > 0x7FFF) val += 0xFFFFFFFFFFFF0000;
            break;
        case VT_16:
        case VT_None:
            break;
        default: assert(0);
        }
        off += sprintf(buf, "$0x%lx", val);
        break;

    case OT_Imm32:
        val = o->val;
        assert(val < (1l<<32));
        switch(t) {
        case VT_None:
        case VT_32:
            break;
        case VT_64:
            if (val > 0x7FFFFFFF) val += 0xFFFFFFFF00000000;
            break;
        default: assert(0);
        }
        off += sprintf(buf, "$%s", prettyAddress(val, fc));
        break;

    case OT_Imm64:
        off += sprintf(buf, "$%s", prettyAddress(o->val, fc));
        break;

    case OT_Ind8:
    case OT_Ind16:
    case OT_Ind32:
    case OT_Ind64:
    case OT_Ind128:
    case OT_Ind256:
    case OT_Ind512:
        switch(o->seg) {
        case OSO_None: break;
        case OSO_UseFS: off += sprintf(buf+off, "%%fs:"); break;
        case OSO_UseGS: off += sprintf(buf+off, "%%gs:"); break;
        default: assert(0);
        }
        val = o->val;
        // for rip-relative addressing, shown displacement is adjusted
        if ((o->scale == 0) && (o->reg.rt == RT_IP)) {
            // addr+len is 0 if not decoded
            val += instr->addr + instr->len;
        }
        if (val != 0) {
            if (val & (1ul<<63))
                off += sprintf(buf+off, "-0x%lx", (~val)+1);
            else
                off += sprintf(buf+off, "%s", prettyAddress(val, fc));
        }
        if ((o->scale == 0) || (o->ireg.rt == RT_None)) {
            if (o->reg.rt != RT_None)
                off += sprintf(buf+off,"(%%%s)", regName(o->reg));
            else if (off == 0) {
                // nothing printed yet
                off += sprintf(buf, "0x0");
            }
        }
        else {
            const char* ri = regName(o->ireg);
            if (o->reg.rt == RT_None) {
                off += sprintf(buf+off,"(,%%%s,%d)", ri, o->scale);
            }
            else
                off += sprintf(buf+off,"(%%%s,%%%s,%d)",
                               regName(o->reg), ri, o->scale);
        }
        break;
    default: assert(0);
    }
    assert(off > 0);
    return buf;
}

const char* instrName(InstrType it, int* pOpCount)
{
    const char* n;
    int opCount = 0;

    switch(it) {
    case IT_HINT_CALL: n = "H-call"; break;
    case IT_HINT_RET:  n = "H-ret"; break;

    case IT_NOP:     n = "nop"; break;
    case IT_RET:     n = "ret"; break;
    case IT_LEAVE:   n = "leave"; break;
    case IT_CLTQ:    n = "cltq"; break;
    case IT_CWTL:    n = "cwtl"; break;
    case IT_CQTO:    n = "cqto"; break;
    case IT_PUSH:    n = "push";    opCount = 1; break;
    case IT_PUSHF:   n = "pushf";   opCount = 1; break;
    case IT_PUSHFQ:  n = "pushfq";  opCount = 1; break;
    case IT_POP:     n = "pop";     opCount = 1; break;
    case IT_POPF:    n = "popf";    opCount = 1; break;
    case IT_POPFQ:   n = "popfq";   opCount = 1; break;
    case IT_CALL:    n = "call";    opCount = 1; break;
    case IT_JMP:     n = "jmp";     opCount = 1; break;
    case IT_JMPI:    n = "jmp*";    opCount = 1; break;
    case IT_JO:      n = "jo";      opCount = 1; break;
    case IT_JNO:     n = "jno";     opCount = 1; break;
    case IT_JC:      n = "jb";      opCount = 1; break;
    case IT_JNC:     n = "jae";     opCount = 1; break;
    case IT_JZ:      n = "je";      opCount = 1; break;
    case IT_JNZ:     n = "jne";     opCount = 1; break;
    case IT_JBE:     n = "jbe";     opCount = 1; break;
    case IT_JA:      n = "ja";      opCount = 1; break;
    case IT_JS:      n = "js";      opCount = 1; break;
    case IT_JNS:     n = "jns";     opCount = 1; break;
    case IT_JP:      n = "jp";      opCount = 1; break;
    case IT_JNP:     n = "jnp";     opCount = 1; break;
    case IT_JL:      n = "jl";      opCount = 1; break;
    case IT_JGE:     n = "jge";     opCount = 1; break;
    case IT_JLE:     n = "jle";     opCount = 1; break;
    case IT_JG:      n = "jg";      opCount = 1; break;
    case IT_SETO:    n = "seto";    opCount = 1; break;
    case IT_SETNO:   n = "setno";   opCount = 1; break;
    case IT_SETC:    n = "setb";    opCount = 1; break;
    case IT_SETNC:   n = "setae";   opCount = 1; break;
    case IT_SETZ:    n = "sete";    opCount = 1; break;
    case IT_SETNZ:   n = "setne";   opCount = 1; break;
    case IT_SETBE:   n = "setbe";   opCount = 1; break;
    case IT_SETA:    n = "seta";    opCount = 1; break;
    case IT_SETS:    n = "sets";    opCount = 1; break;
    case IT_SETNS:   n = "setns";   opCount = 1; break;
    case IT_SETP:    n = "setp";    opCount = 1; break;
    case IT_SETNP:   n = "setnp";   opCount = 1; break;
    case IT_SETL:    n = "setl";    opCount = 1; break;
    case IT_SETGE:   n = "setge";   opCount = 1; break;
    case IT_SETLE:   n = "setle";   opCount = 1; break;
    case IT_SETG:    n = "setg";    opCount = 1; break;
    case IT_MOV:     n = "mov";     opCount = 2; break;
    case IT_MOVSX:   n = "movsx";   opCount = 2; break;
    case IT_MOVD:    n = "movd";    opCount = 2; break;
    case IT_MOVQ:    n = "movq";    opCount = 2; break;
    case IT_MOVZX:   n = "movzx";   opCount = 2; break;
    case IT_NEG:     n = "neg";     opCount = 1; break;
    case IT_NOT:     n = "not";     opCount = 1; break;
    case IT_INC:     n = "inc";     opCount = 1; break;
    case IT_DEC:     n = "dec";     opCount = 1; break;
    case IT_ADD:     n = "add";     opCount = 2; break;
    case IT_ADC:     n = "adc";     opCount = 2; break;
    case IT_SUB:     n = "sub";     opCount = 2; break;
    case IT_SBB:     n = "sbb";     opCount = 2; break;
    case IT_IMUL:    n = "imul";    opCount = 2; break;
    case IT_IDIV1:   n = "idiv";    opCount = 1; break;
    case IT_MUL:     n = "mul";     opCount = 1; break;
    case IT_DIV:     n = "div";     opCount = 1; break;
    case IT_AND:     n = "and";     opCount = 2; break;
    case IT_OR:      n = "or";      opCount = 2; break;
    case IT_XOR:     n = "xor";     opCount = 2; break;
    case IT_SHL:     n = "shl";     opCount = 2; break;
    case IT_SHR:     n = "shr";     opCount = 2; break;
    case IT_SAR:     n = "sar";     opCount = 2; break;
    case IT_LEA:     n = "lea";     opCount = 2; break;
    case IT_CMP:     n = "cmp";     opCount = 2; break;
    case IT_TEST:    n = "test";    opCount = 2; break;
    case IT_BSF:     n = "bsf";     opCount = 2; break;
    case IT_PXOR:    n = "pxor";    opCount = 2; break;
    case IT_PADDQ:   n = "paddq";   opCount = 2; break;
    case IT_MOVSS:   n = "movss";   opCount = 2; break;
    case IT_MOVSD:   n = "movsd";   opCount = 2; break;
    case IT_MOVUPS:  n = "movups";  opCount = 2; break;
    case IT_MOVUPD:  n = "movupd";  opCount = 2; break;
    case IT_MOVAPS:  n = "movaps";  opCount = 2; break;
    case IT_MOVAPD:  n = "movapd";  opCount = 2; break;
    case IT_MOVDQU:  n = "movdqu";  opCount = 2; break;
    case IT_MOVDQA:  n = "movdqa";  opCount = 2; break;
    case IT_MOVLPD:  n = "movlpd";  opCount = 2; break;
    case IT_MOVLPS:  n = "movlps";  opCount = 2; break;
    case IT_MOVHPD:  n = "movhpd";  opCount = 2; break;
    case IT_MOVHPS:  n = "movhps";  opCount = 2; break;

    case IT_ADDSS:   n = "addss";   opCount = 2; break;
    case IT_ADDSD:   n = "addsd";   opCount = 2; break;
    case IT_ADDPS:   n = "addps";   opCount = 2; break;
    case IT_ADDPD:   n = "addpd";   opCount = 2; break;
    case IT_ADDSUBPS:n = "addsubps";opCount = 2; break;
    case IT_ADDSUBPD:n = "addsubpd";opCount = 2; break;
    case IT_SUBSS:   n = "subss";   opCount = 2; break;
    case IT_SUBSD:   n = "subsd";   opCount = 2; break;
    case IT_SUBPS:   n = "subps";   opCount = 2; break;
    case IT_SUBPD:   n = "subpd";   opCount = 2; break;
    case IT_HADDPS:  n = "haddps";  opCount = 2; break;
    case IT_HADDPD:  n = "haddpd";  opCount = 2; break;
    case IT_HSUBPS:  n = "hsubps"  ;opCount = 2; break;
    case IT_HSUBPD:  n = "hsubpd"  ;opCount = 2; break;
    case IT_MULSS:   n = "mulss";   opCount = 2; break;
    case IT_MULSD:   n = "mulsd";   opCount = 2; break;
    case IT_MULPS:   n = "mulps";   opCount = 2; break;
    case IT_MULPD:   n = "mulpd";   opCount = 2; break;
    case IT_DIVSS:   n = "divss";   opCount = 2; break;
    case IT_DIVSD:   n = "divsd";   opCount = 2; break;
    case IT_DIVPD:   n = "divpd";   opCount = 2; break;
    case IT_DIVPS:   n = "divps";   opCount = 2; break;
    case IT_MAXSS:   n = "maxss";   opCount = 2; break;
    case IT_MAXSD:   n = "maxsd";   opCount = 2; break;
    case IT_MAXPD:   n = "maxpd";   opCount = 2; break;
    case IT_MAXPS:   n = "maxps";   opCount = 2; break;
    case IT_MINSS:   n = "minss";   opCount = 2; break;
    case IT_MINSD:   n = "minsd";   opCount = 2; break;
    case IT_MINPD:   n = "minpd";   opCount = 2; break;
    case IT_MINPS:   n = "minps";   opCount = 2; break;
    case IT_SQRTSS:  n = "sqrtss";  opCount = 2; break;
    case IT_SQRTSD:  n = "sqrtsd";  opCount = 2; break;
    case IT_SQRTPD:  n = "sqrtpd";  opCount = 2; break;
    case IT_SQRTPS:  n = "sqrtps";  opCount = 2; break;
    case IT_RCPSS:   n = "rcpss";   opCount = 2; break;
    case IT_RCPPS:   n = "rcpps";   opCount = 2; break;
    case IT_RSQRTSS: n = "rsqrtss"; opCount = 2; break;
    case IT_RSQRTPS: n = "rsqrtps"; opCount = 2; break;
    case IT_XORPS:   n = "xorps";   opCount = 2; break;
    case IT_XORPD:   n = "xorpd";   opCount = 2; break;
    case IT_ORPS:    n = "orps";    opCount = 2; break;
    case IT_ORPD:    n = "orpd";    opCount = 2; break;
    case IT_ANDPS:   n = "andps";   opCount = 2; break;
    case IT_ANDPD:   n = "andpd";   opCount = 2; break;
    case IT_ANDNPS:  n = "andnps";  opCount = 2; break;
    case IT_ANDNPD:  n = "andnpd";  opCount = 2; break;
    case IT_COMISS:  n = "comiss";  opCount = 2; break;
    case IT_COMISD:  n = "comisd";  opCount = 2; break;
    case IT_UCOMISS: n = "ucomiss"; opCount = 2; break;
    case IT_UCOMISD: n = "ucomisd"; opCount = 2; break;
    case IT_PCMPEQB: n = "pcmpeqb"; opCount = 2; break;
    case IT_PCMPEQW: n = "pcmpeqw"; opCount = 2; break;
    case IT_PCMPEQD: n = "pcmpeqd"; opCount = 2; break;
    case IT_PMINUB:  n = "pminub";  opCount = 2; break;
    case IT_PMOVMSKB:n = "pmovmskb";opCount = 2; break;
    case IT_UNPCKLPD: n = "unpcklpd"; opCount = 2; break;
    case IT_UNPCKLPS: n = "unpcklps"; opCount = 2; break;
    case IT_UNPCKHPD: n = "unpckhpd"; opCount = 2; break;
    case IT_UNPCKHPS: n = "unpckhps"; opCount = 2; break;
    case IT_CMOVO:   n = "cmovo";   opCount = 2; break;
    case IT_CMOVNO:  n = "cmovno";  opCount = 2; break;
    case IT_CMOVC:   n = "cmovc";   opCount = 2; break;
    case IT_CMOVNC:  n = "cmovnc";  opCount = 2; break;
    case IT_CMOVZ:   n = "cmovz";   opCount = 2; break;
    case IT_CMOVNZ:  n = "cmovnz";  opCount = 2; break;
    case IT_CMOVBE:  n = "cmovbe";  opCount = 2; break;
    case IT_CMOVA:   n = "cmova";   opCount = 2; break;
    case IT_CMOVS:   n = "cmovs";   opCount = 2; break;
    case IT_CMOVNS:  n = "cmovns";  opCount = 2; break;
    case IT_CMOVP:   n = "cmovp";   opCount = 2; break;
    case IT_CMOVNP:  n = "cmovnp";  opCount = 2; break;
    case IT_CMOVL:   n = "cmovl";   opCount = 2; break;
    case IT_CMOVGE:  n = "cmovge";  opCount = 2; break;
    case IT_CMOVLE:  n = "cmovle";  opCount = 2; break;
    case IT_CMOVG:   n = "cmovg";   opCount = 2; break;

    case IT_VMOVSS:  n = "vmovss";  opCount = 2; break;
    case IT_VMOVSD:  n = "vmovsd";  opCount = 2; break;
    case IT_VMOVUPS: n = "vmovups"; opCount = 2; break;
    case IT_VMOVUPD: n = "vmovupd"; opCount = 2; break;
    case IT_VMOVAPS: n = "vmovaps"; opCount = 2; break;
    case IT_VMOVAPD: n = "vmovapd"; opCount = 2; break;
    case IT_VMOVDQU: n = "vmovdqu"; opCount = 2; break;
    case IT_VMOVDQA: n = "vmovdqa"; opCount = 2; break;
    case IT_VMOVNTDQ:n = "vmovntdq";opCount = 2; break;
    case IT_VADDSS:  n = "vaddss";  opCount = 3; break;
    case IT_VADDSD:  n = "vaddsd";  opCount = 3; break;
    case IT_VADDPS:  n = "vaddps";  opCount = 3; break;
    case IT_VADDPD:  n = "vaddpd";  opCount = 3; break;
    case IT_VMULSS:  n = "vmulss";  opCount = 3; break;
    case IT_VMULSD:  n = "vmulsd";  opCount = 3; break;
    case IT_VMULPS:  n = "vmulps";  opCount = 3; break;
    case IT_VMULPD:  n = "vmulpd";  opCount = 3; break;
    case IT_VXORPS:  n = "vxorps";  opCount = 3; break;
    case IT_VXORPD:  n = "vxorpd";  opCount = 3; break;
    case IT_VZEROALL:n = "vzeroall";opCount = 0; break;
    case IT_VZEROUPPER: n = "vzeroupper"; opCount = 0; break;

    default: n = "<Invalid>"; break;
    }

    if (pOpCount) *pOpCount = opCount;
    return n;
}

char* instr2string(Instr* instr, int align, FunctionConfig* fc)
{
    static char buf[100];
    const char* n;
    int oc = 0, off = 0;

    n = instrName(instr->type, &oc);

    if (align)
        off += sprintf(buf, "%-7s", n);
    else
        off += sprintf(buf, "%s", n);

    // print value type if needed
    bool typeVisible = false;
    ValType vt = instr->vtype;
    // do operands show type?
    if (instr->form == OF_1) {
        if (opTypeVisible(&(instr->dst)))
            typeVisible = true;
    }
    if (instr->form == OF_2) {
        if (opTypeVisible(&(instr->dst)))
            typeVisible = true;
        if (opTypeVisible(&(instr->src)))
            typeVisible = true;
        // special case: conversions (MOVSX/MOVZX)
        // if source type not visible, make it so
        if (((instr->type == IT_MOVSX) ||
             (instr->type == IT_MOVZX)) && !opTypeVisible(&(instr->src))) {
            typeVisible = false;
            vt = opValType(&(instr->src));
        }
    }
    if (instr->form == OF_3) {
        if (opTypeVisible(&(instr->dst)))
            typeVisible = true;
        if (opTypeVisible(&(instr->src)))
            typeVisible = true;
        if (opTypeVisible(&(instr->src2)))
            typeVisible = true;
    }
    // is type implicitly known via instruction name?
    if (instr->vtype == VT_Implicit)
        typeVisible = true;

    if (vt == VT_None) {
        if ((instr->form >= OF_1) && (instr->form <= OF_3))
            vt = opValType(&(instr->dst));
    }
    if (typeVisible) vt = VT_None; // suppress type as already shown

    if (vt != VT_None) {
        char vtc = ' ';
        switch(vt) {
        case VT_8:  vtc = 'b'; break;
        case VT_16: vtc = 'w'; break;
        case VT_32: vtc = 'l'; break;
        case VT_64: vtc = 'q'; break;
        case VT_Implicit: break;
        default: assert(0);
        }
        if (vtc != ' ') {
            int nlen = strlen(n);
            if (buf[nlen] == ' ') buf[nlen] = vtc;
            else {
                buf[nlen] = vtc;
                buf[nlen+1] = 0;
                off++;
            }
        }
    }

    switch(instr->form) {
    case OF_0:
        assert(instr->dst.type == OT_None);
        assert(instr->src.type == OT_None);
        assert(instr->src2.type == OT_None);
        break;

    case OF_1:
        assert(instr->dst.type != OT_None);
        assert(instr->src.type == OT_None);
        assert(instr->src2.type == OT_None);
        off += sprintf(buf+off, " %s",
                       op2string(&(instr->dst), instr, fc));
        break;

    case OF_2:
        assert(instr->dst.type != OT_None);
        assert(instr->src.type != OT_None);
        assert(instr->src2.type == OT_None);
        off += sprintf(buf+off, " %s",
                       op2string(&(instr->src), instr, fc));
        off += sprintf(buf+off, ",%s",
                       op2string(&(instr->dst), instr, fc));
        break;

    case OF_3:
        assert(instr->dst.type != OT_None);
        assert(instr->src.type != OT_None);
        assert(instr->src2.type != OT_None);
        off += sprintf(buf+off, " %s",
                       op2string(&(instr->src2), instr, fc));
        off += sprintf(buf+off, ",%s",
                       op2string(&(instr->src), instr, fc));
        off += sprintf(buf+off, ",%s",
                       op2string(&(instr->dst), instr, fc));
        break;

    default: assert(0);
    }

    return buf;
}

char* bytes2string(Instr* instr, int start, int count)
{
    static char buf[100];
    int off = 0, i, j;
    for(i = start, j=0; (i < instr->len) && (j<count); i++, j++) {
        uint8_t b = ((uint8_t*) instr->addr)[i];
        off += sprintf(buf+off, " %02x", b);
    }
    for(;j<count;j++)
        off += sprintf(buf+off, "   ");
    if (off == 0) buf[0] = 0;
    return buf;
}

void dbrew_print_decoded(DBB* bb, bool printBytes)
{
    int i;
    for(i = 0; i < bb->count; i++) {
        Instr* instr = bb->instr + i;
        printf("  %18s: ", prettyAddress(instr->addr, bb->fc));
        if (printBytes)
            printf("%s ", bytes2string(instr, 0, 7));
        printf(" %s\n", instr2string(instr, 1, bb->fc));
        if (printBytes && (instr->len > 7))
            printf("  %18s: %s\n", prettyAddress(instr->addr + 7, bb->fc),
                   bytes2string(instr, 7, 7));
        if (printBytes && (instr->len > 14))
            printf("  %18s: %s\n", prettyAddress(instr->addr + 14, bb->fc),
                   bytes2string(instr, 14, 7));
    }
}

void printDecodedBBs(Rewriter* r)
{
    int i;
    for(i=0; i< r->decBBCount; i++) {
        printf("BB %s (%d instructions):\n",
               prettyAddress(r->decBB[i].addr, r->decBB[i].fc),
               r->decBB[i].count);
        dbrew_print_decoded(r->decBB + i, r->printBytes);
    }
}
