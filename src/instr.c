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

#include "instr.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


ValType opValType(Operand* o)
{
    switch(o->type) {
    case OT_Imm8:
    case OT_Reg8:
    case OT_Ind8:
        return VT_8;
    case OT_Imm16:
    case OT_Reg16:
    case OT_Ind16:
        return VT_16;
    case OT_Imm32:
    case OT_Reg32:
    case OT_Ind32:
        return VT_32;
    case OT_Imm64:
    case OT_Reg64:
    case OT_Ind64:
        return VT_64;
    case OT_Reg128:
    case OT_Ind128:
        return VT_128;
    case OT_Reg256:
    case OT_Ind256:
        return VT_256;

    default: assert(0);
    }
    return 0; // invalid;
}

int opTypeWidth(Operand* o)
{
    switch(opValType(o)) {
    case VT_8: return 8;
    case VT_16: return 16;
    case VT_32: return 32;
    case VT_64: return 64;
    case VT_128: return 128;
    case VT_256: return 256;
    default: assert(0);
    }
    return 0;
}

bool opIsImm(Operand* o)
{
    switch(o->type) {
    case OT_Imm8:
    case OT_Imm16:
    case OT_Imm32:
    case OT_Imm64:
        return true;
    default:
        break;
    }
    return false;
}

bool opIsReg(Operand* o)
{
    switch(o->type) {
    case OT_Reg8:
    case OT_Reg16:
    case OT_Reg32:
    case OT_Reg64:
    case OT_Reg128:
    case OT_Reg256:
        return true;
    default:
        break;
    }
    return false;
}

bool opIsGPReg(Operand* o)
{
    if (!opIsReg(o)) return false;
    if ((o->reg >= Reg_AX) && (o->reg <= Reg_15))
        return true;
    return false;
}

bool opIsVReg(Operand* o)
{
    if (!opIsReg(o)) return false;
    if ((o->reg >= Reg_X0) && (o->reg <= Reg_X15))
        return true;
    return false;
}


bool opIsInd(Operand* o)
{
    switch(o->type) {
    case OT_Ind8:
    case OT_Ind16:
    case OT_Ind32:
    case OT_Ind64:
    case OT_Ind128:
    case OT_Ind256:
        return true;
    default:
        break;
    }
    return false;
}

bool opIsEqual(Operand* o1, Operand* o2)
{
    if (o1->type != o2->type)
        return false;
    if (opIsReg(o1))
        return (o1->reg == o2->reg);
    if (opIsImm(o1))
        return (o1->val == o2->val);
    // memory
    assert(opIsInd(o1));
    if (o1->val != o2->val) return false;
    if (o1->reg != o2->reg) return false;
    if (o1->seg != o2->seg) return false;

    if (o1->scale == 0) return true;
    if ((o1->scale != o2->scale) || (o1->ireg != o2->ireg)) return false;
    return true;
}

Operand* getRegOp(ValType t, Reg r)
{
    static Operand o;

    if ((r >= Reg_AX) && (r <= Reg_15)) {
        switch(t) {
        case VT_8:  o.type = OT_Reg8; break;
        case VT_16: o.type = OT_Reg16; break;
        case VT_32: o.type = OT_Reg32; break;
        case VT_64: o.type = OT_Reg64; break;
        default: assert(0);
        }
        o.reg = r;
        return &o;
    }

    if ((r >= Reg_X0) && (r <= Reg_X15)) {
        switch(t) {
        case VT_64:  o.type = OT_Reg64; break;
        case VT_128: o.type = OT_Reg128; break;
        case VT_256: o.type = OT_Reg256; break;
        default: assert(0);
        }
        o.reg = r;
        return &o;
    }
    assert(0);
}

Operand* getImmOp(ValType t, uint64_t v)
{
    static Operand o;

    switch(t) {
    case VT_8:
        o.type = OT_Imm8;
        o.val = v;
        break;

    case VT_16:
        o.type = OT_Imm16;
        o.val = v;
        break;

    case VT_32:
        o.type = OT_Imm32;
        o.val = v;
        break;

    case VT_64:
        o.type = OT_Imm64;
        o.val = v;
        break;

    default: assert(0);
    }

    return &o;
}


void copyOperand(Operand* dst, Operand* src)
{
    dst->type = src->type;
    switch(src->type) {
    case OT_Imm8:
        assert(src->val < (1l<<8));
        // fall-trough
    case OT_Imm16:
        assert(src->val < (1l<<16));
        // fall-trough
    case OT_Imm32:
        assert(src->val < (1l<<32));
        // fall-trough
    case OT_Imm64:
        dst->val = src->val;
        break;
    case OT_Reg8:
    case OT_Reg16:
    case OT_Reg32:
    case OT_Reg64:
    case OT_Reg128:
    case OT_Reg256:
        dst->reg = src->reg;
        break;
    case OT_Ind8:
    case OT_Ind32:
    case OT_Ind64:
    case OT_Ind128:
    case OT_Ind256:
        assert( (src->reg == Reg_None) ||
                (src->reg == Reg_IP) ||
                ((src->reg >= Reg_AX) && (src->reg <= Reg_15)) );
        dst->reg = src->reg;
        dst->val = src->val;
        dst->seg = src->seg;
        dst->scale = src->scale;
        if (src->scale >0) {
            assert((src->scale == 1) || (src->scale == 2) ||
                   (src->scale == 4) || (src->scale == 8));
            assert((src->ireg >= Reg_AX) && (src->ireg <= Reg_15));
            dst->ireg = src->ireg;
        }
        break;
    default: assert(0);
    }
}

void opOverwriteType(Operand* o, ValType vt)
{
    if (opIsImm(o)) {
        switch(vt) {
        case VT_8:   o->type = OT_Imm8; break;
        case VT_16:  o->type = OT_Imm8; break;
        case VT_32:  o->type = OT_Imm32; break;
        case VT_64:  o->type = OT_Imm64; break;
        default: assert(0);
        }
    }
    else if (opIsReg(o)) {
        switch(vt) {
        case VT_8:   o->type = OT_Reg8; break;
        case VT_16:  o->type = OT_Reg16; break;
        case VT_32:  o->type = OT_Reg32; break;
        case VT_64:  o->type = OT_Reg64; break;
        case VT_128:
            o->type = OT_Reg128;
            assert(opIsVReg(o));
            break;
        case VT_256:
            o->type = OT_Reg256;
            assert(opIsVReg(o));
            break;
        default: assert(0);
        }
    }
    else if (opIsInd(o)) {
        switch(vt) {
        case VT_8:   o->type = OT_Ind8; break;
        case VT_16:  o->type = OT_Ind16; break;
        case VT_32:  o->type = OT_Ind32; break;
        case VT_64:  o->type = OT_Ind64; break;
        case VT_128: o->type = OT_Ind128; break;
        case VT_256: o->type = OT_Ind256; break;
        default: assert(0);
        }
    }
    else
        assert(0);
}

bool instrIsJcc(InstrType it)
{
    switch(it) {
    case IT_JO:
    case IT_JNO:
    case IT_JC:
    case IT_JNC:
    case IT_JZ:
    case IT_JNZ:
    case IT_JBE:
    case IT_JA:
    case IT_JS:
    case IT_JNS:
    case IT_JP:
    case IT_JNP:
    case IT_JL:
    case IT_JGE:
    case IT_JLE:
    case IT_JG:
        return true;
    default:
        break;
    }
    return false;
}

void copyInstr(Instr* dst, Instr* src)
{
    dst->addr  = src->addr;
    dst->len   = src->len;
    dst->type  = src->type;
    dst->vtype = src->vtype;
    dst->form  = src->form;

    dst->dst.type = OT_None;
    dst->src.type = OT_None;
    dst->src2.type = OT_None;
    switch(src->form) {
    case OF_3:
        copyOperand(&(dst->src2), &(src->src2));
        // fall through
    case OF_2:
        copyOperand(&(dst->src), &(src->src));
        // fall through
    case OF_1:
        copyOperand(&(dst->dst), &(src->dst));
        // fall through
    case OF_0:
        break;
    default: assert(0);
    }

    dst->ptLen = src->ptLen;
    if (src->ptLen > 0) {
        dst->ptPSet = src->ptPSet;
        dst->ptEnc  = src->ptEnc;
        dst->ptSChange = src->ptSChange;
        for(int j=0; j < src->ptLen; j++)
            dst->ptOpc[j] = src->ptOpc[j];
    }
}

void initSimpleInstr(Instr* i, InstrType it)
{
    i->addr = 0; // unknown: created, not parsed
    i->len = 0;

    i->type = it;
    i->ptLen = 0; // no pass-through info
    i->vtype = VT_None;
    i->form = OF_0;
    i->dst.type = OT_None;
    i->src.type = OT_None;
    i->src2.type = OT_None;

    i->info_memAddr = 0;
}

void initUnaryInstr(Instr* i, InstrType it, Operand* o)
{
    initSimpleInstr(i, it);
    i->form = OF_1;
    copyOperand( &(i->dst), o);
}

void initBinaryInstr(Instr* i, InstrType it, ValType vt,
                     Operand *o1, Operand *o2)
{
    if (vt != VT_None) {
        // if we specify a value type, it must match destination
        assert(vt == opValType(o1));
        // if 2nd operand is other than immediate, types also must match
        if (!opIsImm(o2))
            assert(vt == opValType(o2));
    }

    initSimpleInstr(i, it);
    i->form = OF_2;
    i->vtype = vt;
    copyOperand( &(i->dst), o1);
    copyOperand( &(i->src), o2);
}

void initTernaryInstr(Instr* i, InstrType it,
                      Operand *o1, Operand *o2, Operand* o3)
{
    initSimpleInstr(i, it);
    i->form = OF_3;
    copyOperand( &(i->dst), o1);
    copyOperand( &(i->src), o2);
    copyOperand( &(i->src2), o3);
}


void attachPassthrough(Instr* i, PrefixSet set,
                       OperandEncoding enc, StateChange sc,
                       int b1, int b2, int b3)
{
    assert(i->ptLen == 0);
    i->ptEnc = enc;
    i->ptSChange = sc;
    i->ptPSet = set;
    assert(b1>=0);
    i->ptLen++;
    i->ptOpc[0] = (unsigned char) b1;
    if (b2 < 0) return;
    i->ptLen++;
    i->ptOpc[1] = (unsigned char) b2;
    if (b3 < 0) return;
    i->ptLen++;
    i->ptOpc[2] = (unsigned char) b3;
}
