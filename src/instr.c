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


// Helper functions for structs used for decoded instructions

// x86-64 register types used for a given value type
RegType getGPRegType(ValType vt)
{
    switch(vt) {
    case VT_8:  return RT_GP8;
    case VT_16: return RT_GP16;
    case VT_32: return RT_GP32;
    case VT_64: return RT_GP64;
    default: break;
    }
    return RT_None;
}

// legacy x86 register types used for a given value type
// same as getGPRegType apart from 8-bit values
RegType getLegGPRegType(ValType vt)
{
    switch(vt) {
    case VT_8:  return RT_GP8Leg;
    case VT_16: return RT_GP16;
    case VT_32: return RT_GP32;
    case VT_64: return RT_GP64;
    default: break;
    }
    return RT_None;
}

RegType getVRegType(ValType vt)
{
    switch(vt) {
    case VT_64:  return RT_MMX;
    case VT_128: return RT_XMM;
    case VT_256: return RT_YMM;
    case VT_512: return RT_ZMM;
    default: break;
    }
    return RT_None;
}

ValType regValTypeT(RegType rt)
{
    switch(rt) {
    case RT_Flag:
        return VT_1;
    case RT_GP8:
    case RT_GP8Leg:
        return VT_8;
    case RT_GP16:
        return VT_16;
    case RT_GP32:
        return VT_32;
    case RT_GP64:
    case RT_IP:
        return VT_64;
    case RT_X87:
        return VT_80;
    case RT_MMX:
        return VT_64;
    case RT_XMM:
        return VT_128;
    case RT_YMM:
        return VT_256;
    case RT_ZMM:
        return VT_512;
    default: break;
    }
    return VT_None;
}

ValType regValType(Reg r)
{
    return regValTypeT(r.rt);
}


bool regTypeIsGP(RegType rt)
{
    if ( (rt == RT_GP8Leg) ||
         (rt == RT_GP8)    ||
         (rt == RT_GP16)   ||
         (rt == RT_GP32)   ||
         (rt == RT_GP64)) {
        return true;
    }
    return false;
}

bool regTypeIsV(RegType rt)
{
    switch(rt) {
    case RT_MMX:
    case RT_XMM:
    case RT_YMM:
    case RT_ZMM:
        return true;
    default:
        break;
    }
    return false;
}

bool regIsGP(Reg r)
{
    if (regTypeIsGP(r.rt)) {
        assert(r.ri < RI_GPMax);
        return true;
    }
    return false;
}

bool regIsV(Reg r)
{
    switch(r.rt) {
    case RT_MMX:
        assert(r.ri < RI_MMMax);
        break;
    case RT_XMM:
    case RT_YMM:
        assert(r.ri < RI_XMMMax);
        break;
    case RT_ZMM:
        assert(r.ri < RI_ZMMMax);
        break;
    default:
        return false;
    }
    return true;
}

// if not a GP64, return RI_None
RegIndex regGP64Index(Reg r)
{
    if (r.rt == RT_GP64) {
        assert(r.ri < RI_GPMax);
        return r.ri;
    }
    return RI_None;
}

// if not a XMM/YMM/ZMM register, return RI_None
RegIndex regVIndex(Reg r)
{
    if ((r.rt == RT_XMM) || (r.rt == RT_YMM) || (r.rt == RT_ZMM)) {
        assert(r.ri < RI_XMMMax);
        return r.ri;
    }
    return RI_None;
}


Reg getReg(RegType rt, RegIndex ri)
{
    switch(rt) {
    case RT_None:
    case RT_IP:
        assert(ri == 0);
        break;
    case RT_GP8Leg:
    case RT_MMX:
        assert(ri < 8);
        break;
    case RT_GP8:
    case RT_GP16:
    case RT_GP32:
    case RT_GP64:
    case RT_XMM:
    case RT_YMM:
        assert(ri < 16);
        break;
    default:
        assert(0);
    }

    Reg r = {rt, ri};
    return r;
}


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
    return VT_None; // invalid;
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
    return regIsGP(o->reg);
}

bool opIsVReg(Operand* o)
{
    if (!opIsReg(o)) return false;
    return regIsV(o->reg);
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

bool regIsEqual(Reg r1, Reg r2)
{
    if (r1.rt != r2.rt) return false;

    // if one or both are unspecified, they cannot be equal
    if (r1.rt == RT_None) return false;
    if (r2.rt == RT_None) return false;

    return (r1.ri == r2.ri);
}

bool opIsEqual(Operand* o1, Operand* o2)
{
    if (o1->type != o2->type)
        return false;
    if (opIsReg(o1))
        return regIsEqual(o1->reg, o2->reg);
    if (opIsImm(o1))
        return (o1->val == o2->val);
    // memory
    assert(opIsInd(o1));
    if (o1->val != o2->val) return false;
    if (!regIsEqual(o1->reg, o2->reg)) return false;
    if (o1->seg != o2->seg) return false;

    if (o1->scale == 0) return true;
    if (o1->scale != o2->scale) return false;
    if (!regIsEqual(o1->ireg, o2->ireg)) return false;
    return true;
}

OpType getImmOpType(ValType t)
{
    switch(t) {
    case VT_8:  return OT_Imm8;
    case VT_16: return OT_Imm16;
    case VT_32: return OT_Imm32;
    case VT_64: return OT_Imm64;
    default: break;
    }
    assert(0);
}

OpType getGPRegOpType(ValType t)
{
    switch(t) {
    case VT_8:  return OT_Reg8;
    case VT_16: return OT_Reg16;
    case VT_32: return OT_Reg32;
    case VT_64: return OT_Reg64;
    default: break;
    }
    assert(0);
}

void setRegOp(Operand* o, Reg r)
{
    ValType t = regValType(r);
    if (regIsGP(r)) {
        o->type = getGPRegOpType(t);
        o->reg = r;
        return;
    }

    if (regIsV(r)) {
        switch(t) {
        case VT_64:  o->type = OT_Reg64; break;
        case VT_128: o->type = OT_Reg128; break;
        case VT_256: o->type = OT_Reg256; break;
        default: assert(0);
        }
        o->reg = r;
        return;
    }
    assert(0);

}

Operand* getRegOp(Reg r)
{
    static Operand o;

    setRegOp(&o, r);
    return &o;
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
    case OT_Ind16:
    case OT_Ind32:
    case OT_Ind64:
    case OT_Ind128:
    case OT_Ind256:
        assert( (src->reg.rt == RT_None) ||
                (src->reg.rt == RT_IP)   ||
                (src->reg.rt == RT_GP64) );
        dst->reg = src->reg;
        dst->val = src->val;
        dst->seg = src->seg;
        dst->scale = src->scale;
        if (src->scale >0) {
            assert((src->scale == 1) || (src->scale == 2) ||
                   (src->scale == 4) || (src->scale == 8));
            assert(src->ireg.rt == RT_GP64);
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
        case VT_16:  o->type = OT_Imm16; break;
        case VT_32:  o->type = OT_Imm32; break;
        case VT_64:  o->type = OT_Imm64; break;
        default: assert(0);
        }
    }
    else if (opIsReg(o)) {
        switch(vt) {
        case VT_8:
            o->type   = OT_Reg8;
            o->reg.rt = RT_GP8;
            break;
        case VT_16:
            o->type   = OT_Reg16;
            o->reg.rt = RT_GP16;
            break;
        case VT_32:
            o->type   = OT_Reg32;
            o->reg.rt = RT_GP32;
            break;
        case VT_64:
            o->type   = OT_Reg64;
            if (opIsVReg(o))
                o->reg.rt = RT_MMX;
            else
                o->reg.rt = RT_GP64;
            break;
        case VT_128:
            o->type   = OT_Reg128;
            o->reg.rt = RT_XMM;
            assert(opIsVReg(o));
            break;
        case VT_256:
            o->type = OT_Reg256;
            o->reg.rt = RT_YMM;
            assert(opIsVReg(o));
            break;
        case VT_512:
            o->type = OT_Reg512;
            o->reg.rt = RT_ZMM;
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
        dst->ptVexP = src->ptVexP;
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


void attachPassthrough(Instr* i, VexPrefix vp,
                       PrefixSet set, OperandEncoding enc, StateChange sc,
                       int b1, int b2, int b3)
{
    // catch previous decode error
    if (!i) return;

    assert(i->ptLen == 0); // never should happen
    i->ptEnc = enc;
    i->ptSChange = sc;
    i->ptPSet = set;
    i->ptVexP = vp;
    assert(b1 >= 0); // never should happen
    i->ptLen++;
    i->ptOpc[0] = (uint8_t) b1;
    if (b2 < 0) return;
    i->ptLen++;
    i->ptOpc[1] = (uint8_t) b2;
    if (b3 < 0) return;
    i->ptLen++;
    i->ptOpc[2] = (uint8_t) b3;
}
