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

#include "generate.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "common.h"
#include "printer.h"
#include "error.h"

struct _GContext {
    Instr* instr;
    uint8_t* buf;
    GenerateError* e;

    // information passed among generator functions
    VexPrefix vp;
    int vvvv;
    PrefixSet ps;
    int rex;
    OpSegOverride so;
    uint8_t b[10];    // partly generated machine code
    int blen;         // valid bytes in b

    int32_t opc;
    OperandEncoding oe;
    ValType vt;
    int flags;
};

static
void markError(GContext* c, ErrorType et, const char* d)
{
    if (d == 0) {
        if (et == ET_UnsupportedOperands)
            d = "unsupported operands";
        else
            d = "unsupported instruction";
    }
    setError(&(c->e->e), et, EM_Generator, 0, d);

    // will be set later
    c->e->cbb = 0;
    c->e->offset = -1;
}


/*------------------------------------------------------------*/
/* x86_64 code generation
 */

// flags for genModRM/genDigitRM

#define GEN_66OnVT16  1
// default operand size is 64bit
#define GEN_DefOpVT64 2
// helpers for operand encodings

// return 0 - 15 for AL/AX/EAX/RAX - R15
static
int GPRegEncoding(Reg r)
{
    assert(regIsGP(r));
    return r.ri;
}

// return 0 - 15 for RAX - R15
static
int GP64RegEncoding(Reg r)
{
    assert(r.rt == RT_GP64);
    assert(r.ri < RI_GPMax);
    return r.ri;
}

// return 0 - 7 for mm0-mm7 (MMX), 0-15 for XMM0/YMM0-XMM15/YMM15 (SSE/AVX)
static
int VRegEncoding(Reg r)
{
    assert(regIsV(r));
    return r.ri;
}


// Generate bytes with ModRM encoding for operand <o> and 3-bit <digit>.
// Sets info in GContext <c>: b/len/rex/so
static
void calcModRMDigit(GContext* c, Operand* o1, int digit, int flags)
{
    int modrm, r1;
    int o = 0;
    ValType vt;

    assert((digit>=0) && (digit<8));
    assert(opIsReg(o1) || opIsInd(o1));

    vt = opValType(o1);
    if ((flags & GEN_DefOpVT64) == 0) {
        if (vt == VT_64) c->rex |= REX_MASK_W;
    }

    modrm = (digit & 7) << 3;

    if (opIsReg(o1)) {
        // r,r: mod 3
        modrm |= 192;
        if (opIsGPReg(o1)) {
            r1 = GPRegEncoding(o1->reg);
            if ( (o1->reg.rt == RT_GP8) &&
                 (o1->reg.ri >= 4) && (o1->reg.ri < 8) ) {
                // empty REX mask for non-legacy 8-bit registers
                c->rex |= 0x40;
            }
        }
        else if (opIsVReg(o1))
            r1 = VRegEncoding(o1->reg);
        else
            assert(0);
        if (r1 & 8) c->rex |= REX_MASK_B;
        modrm |= (r1 & 7);
        c->b[o++] = modrm;
    }
    else {
        int useDisp8 = 0, useDisp32 = 0, useSIB = 0;
        int sib = 0;
        int64_t v = (int64_t) o1->val;
        if (v != 0) {
            if ((v >= -128) && (v<128)) useDisp8 = 1;
            else if ((v >= -((int64_t)1<<31)) &&
                     (v < ((int64_t)1<<31))) useDisp32 = 1;
            else assert(0);

            if (useDisp8)
                modrm |= 64;
            if (useDisp32)
                modrm |= 128;
        }

        // may need to generated segment override prefix
        c->so = o1->seg;

        if ((o1->scale == 0) &&
                (regGP64Index(o1->reg) != RI_SP) &&
                (regGP64Index(o1->reg) != RI_12)) {
            // no SIB needed (and reg not rsp/r12 which requires SIB)
            if (o1->reg.rt == RT_None) {
                useDisp32 = 1; // encoding needs disp32
                useDisp8 = 0;
                modrm &= 63; // mod needs to be 00
                useSIB = 1;
                sib = (4 << 3) + 5; // index 4 (= none) + base 5 (= none)
            }
            else {
                if (o1->reg.rt == RT_IP) {
                    // should not happen, we converted RIP-rel to absolute
                    assert(0);
                    // RIP relative
                    r1 = 5;
                    modrm &= 63;
                    useDisp32 = 1;
                }
                else {
                    r1 = GP64RegEncoding(o1->reg);

                    if (((r1 == RI_BP) || (r1 == RI_13)) && (v==0)) {
                        // encoding for rbp/r13 without displacement is reused
                        // for RIP-relative addressing!
                        // we need to enforce +disp8 with disp8 = 0
                        // (see SDM, table 2-5 in 2.2.1.2)
                        useDisp8 = 1;
                        assert(modrm < 64); // check that mod = 0
                        modrm |= 64;
                    }
                }
                if (r1 & 8) c->rex |= REX_MASK_B;
                modrm |= (r1 & 7);
            }
        }
        else {
            // SIB
            int ri, rb;
            useSIB = 1;
            if      (o1->scale == 2) sib |= 64;
            else if (o1->scale == 4) sib |= 128;
            else if (o1->scale == 8) sib |= 192;
            else
                assert((o1->scale == 0) || (o1->scale == 1));

            if ((o1->scale == 0) || (o1->ireg.rt == RT_None)) {
                // no index register: uses index 4 (usually SP, not allowed)
                sib |= (4 << 3);
            }
            else {
                ri = GP64RegEncoding(o1->ireg);
                // offset 4 not allowed here, used for "no scaling"
                assert(ri != 4);
                if (ri & 8) c->rex |= REX_MASK_X;
                sib |= (ri & 7) <<3;
            }

            if (o1->reg.rt == RT_None) {
                // encoding requires disp32 with mod = 00 / base 5 = none
                useDisp32 = 1;
                useDisp8 = 0;
                modrm &= 63;
                sib |= 5;
            }
            else {
                if ((regGP64Index(o1->reg) == RI_BP)
                        || (regGP64Index(o1->reg) == RI_13) ) {
                    // cannot use mod == 00
                    if ((modrm & 192) == 0) {
                        modrm |= 64;
                        useDisp8 = 1;
                    }
                }
                rb = GP64RegEncoding(o1->reg);
                if (rb & 8) c->rex |= REX_MASK_B;
                sib |= (rb & 7);
            }
        }

        if (useSIB)
            modrm |= 4; // signal SIB in modrm
        c->b[o++] = modrm;
        if (useSIB)
            c->b[o++] = sib;
        if (useDisp8)
            c->b[o++] = (int8_t) v;
        if (useDisp32) {
            *(int32_t*)(c->b+o) = (int32_t) v;
            o += 4;
        }
    }

    c->blen = o;
}

// Generate bytes with ModRM encoding for operands <o1> and <o2>
// Sets info in GContext <c>: b/len/rex/so
static
void calcModRM(GContext* c, Operand* o1, Operand* o2)
{
    int r2; // register offset encoding for operand 2

    assert(opValType(o1) == opValType(o2));

    if (opIsGPReg(o2)) {
        assert(opIsReg(o1) || opIsInd(o1));
        r2 = GPRegEncoding(o2->reg);
        if ( (o2->reg.rt == RT_GP8) &&
             (o2->reg.ri >= 4) && (o2->reg.ri < 8) ) {
            // empty REX mask for non-legacy 8-bit registers
            c->rex |= 0x40;
        }
    }
    else if (opIsVReg(o2)) {
        assert(opIsReg(o1) || opIsInd(o1));
        r2 = VRegEncoding(o2->reg);
    }
    else assert(0);

    if (r2 & 8) c->rex |= REX_MASK_R;
    calcModRMDigit(c, o1, r2 & 7, 0);
}


static
int genPrefix(GContext* c)
{
    uint8_t* buf = c->buf;
    int o = 0;

    if (c->so == OSO_UseFS) buf[o++] = 0x64;
    if (c->so == OSO_UseGS) buf[o++] = 0x65;
    if (c->ps & PS_REXW) {
        c->rex |= REX_MASK_W;
        c->ps &= ~PS_REXW;
    }

    if (c->vp == VEX_No) {
        // legacy
        if (c->ps & PS_66) buf[o++] = 0x66;
        if (c->ps & PS_F2) buf[o++] = 0xF2;
        if (c->ps & PS_F3) buf[o++] = 0xF3;
        if (c->rex) buf[o++] = 0x40 | c->rex;
        return o;
    }

    // Vex
    assert((c->vp == VEX_128) || (c->vp == VEX_256));
    assert((c->opc & 0xFF00) == 0x0F00); // opcode 2 byte starting with 0x0F
    c->opc = c->opc & 0xFF; // do not generate the leading 0x0F
    uint8_t b = ((15 - c->vvvv) << 3) | ((c->vp == VEX_128) ? 0:4);
    switch(c->ps) {
    case PS_66: b |= 1; break;
    case PS_F3: b |= 2; break;
    case PS_F2: b |= 3; break;
    case PS_No: break;
    default: assert(0);
    }
    if ((c->rex & (REX_MASK_X | REX_MASK_B | REX_MASK_W)) == 0) {
        // 2-byte vex prefix enough
        b |= (c->rex & REX_MASK_R) ? 0:128; // inverted;
        buf[o++] = 0xC5;
        buf[o++] = b;
    }
    else {
        // 3-byte vex prefix
        int b0 = 1; // 0x0F leading opcode byte
        b0 |= (c->rex & REX_MASK_R) ? 0:128; // inverted;
        b0 |= (c->rex & REX_MASK_X) ? 0:64; // inverted;
        b0 |= (c->rex & REX_MASK_B) ? 0:32; // inverted;
        b |= (c->rex & REX_MASK_W) ? 0:128; // inverted;
        buf[o++] = 0xC4;
        buf[o++] = b0;
        buf[o++] = b;
    }
    return o;
}

// append bytes for opcode and operands
static
int appendOO(GContext* c, int o)
{
    uint8_t* buf = c->buf;
    int opc = c->opc;
    if (opc > 255) {
        assert(opc < 65536);
        buf[o++] = (uint8_t) (opc >> 8);
        buf[o++] = (uint8_t) (opc & 255);
    }
    else
        buf[o++] = (uint8_t) opc;

    // append bytes for encoded operands
    for(int i=0; i < c->blen; i++)
        buf[o++] = c->b[i];

    return o;
}



// Generate instruction with operand encoding RM (o1: r/m, o2: r)
// into c->buf, up to 2 opcodes (2 if opc2 >=0).
// If result type (vt) is explicitly specified as "VT_Implicit", do not
// automatically generate REX prefix depending on operand types.
// Returns byte length of generated instruction.
static
int genModRM(GContext* c, int opc,
             Operand* o1, Operand* o2, ValType vt, int flags)
{
    int o = 0;

    calcModRM(c, o1, o2);
    // with value type implicit given by instruction type (e.g. MMX/SSE),
    //   clear REX_MASK_W if not explicitly requested
    if ((vt == VT_Implicit) && ((c->ps & PS_REXW) == 0)) c->rex &= ~REX_MASK_W;
    if ((flags & GEN_66OnVT16) && (opValType(o1) == VT_16)) c->ps |= PS_66;
    c->opc = opc;
    o = genPrefix(c);
    o = appendOO(c, o);

    return o;
}

// Operand o1: r/m
static
int genDigitRM(GContext* c, int opc,
               int digit, Operand* o1, int flags)
{
    int o;

    calcModRMDigit(c, o1, digit, flags);
    if ((flags & GEN_66OnVT16) && (opValType(o1) == VT_16)) c->ps |= PS_66;
    c->opc = opc;
    o = genPrefix(c);
    o = appendOO(c, o);

    return o;
}

// Operand o: imm
static
int appendI(uint8_t* buf, int o, Operand* op)
{
    assert(opIsImm(op));
    switch(op->type) {
    case OT_Imm8:
        *(uint8_t*)(buf + o) = (uint8_t) op->val;
        o += 1;
        break;

    case OT_Imm16:
        *(uint16_t*)(buf + o) = (uint16_t) op->val;
        o += 2;
        break;

    case OT_Imm32:
        *(uint32_t*)(buf + o) = (uint32_t) op->val;
        o += 4;
        break;

    case OT_Imm64:
        *(uint64_t*)(buf + o) = op->val;
        o += 8;
        break;

    default: assert(0);
    }
    return o;
}

// Operand o1: r/m, o2: r, o3: imm
static
int genModRMI(GContext* c, int opc,
              Operand* o1, Operand* o2, Operand* o3, int flags)
{
    uint8_t* buf = c->buf;
    int o;

    calcModRM(c, o1, o2);
    if ((flags & GEN_66OnVT16) && (opValType(o1) == VT_16)) c->ps |= PS_66;
    c->opc = opc;
    o = genPrefix(c);
    o = appendOO(c, o);

    return appendI(buf, o, o3);
}

// Operand o1: r/m, o2: imm
static
int genDigitMI(GContext* c, int opc,
               int digit, Operand* o1, Operand* o2, int flags)
{
    uint8_t* buf = c->buf;
    int o;

    assert(opIsImm(o2));
    calcModRMDigit(c, o1, digit, flags);
    if ((flags & GEN_66OnVT16) && (opValType(o1) == VT_16)) c->ps |= PS_66;
    c->opc = opc;
    o = genPrefix(c);
    o = appendOO(c, o);

    return appendI(buf, o, o2);
}

// Operand o1: r (gets part of opcode), o2: imm
static
int genOI(GContext* c, int opc, Operand* o1, Operand* o2, int flags)
{
    uint8_t* buf = c->buf;
    int o = 0, r;

    assert(opIsReg(o1));
    assert(opIsImm(o2));

    r = GPRegEncoding(o1->reg);
    if (r & 8) c->rex |= REX_MASK_B;
    if (opValType(o1) == VT_64) c->rex |= REX_MASK_W;
    if ((flags & GEN_66OnVT16) && (opValType(o1) == VT_16)) c->ps |= PS_66;
    assert((opc < 256) && ((opc&7)==0));
    c->opc = opc + (r & 7);
    o = genPrefix(c);
    o = appendOO(c, o);
    return appendI(buf, o, o2);
}


// almost generic generator, using data from GContext
static
int genInstr(GContext* c)
{
    Instr* i = c->instr;
    ValType vt = c->vt;
    int flags = c->flags;
    int opc = c->opc;

    switch(c->oe) {
    case OE_MR: return genModRM(c, opc, &(i->dst), &(i->src), vt, flags);
    case OE_RM: return genModRM(c, opc, &(i->src), &(i->dst), vt, flags);
    default: break;
    }
    return 0;
}

static
void setGenInfo(GContext* c,
                PrefixSet ps, int opc, OperandEncoding oe, ValType vt)
{
    c->ps = ps;
    c->oe = oe;
    c->vt = vt;
    c->opc = opc;
}


// if imm64 and value fitting into imm32, return imm32 version
// otherwise, or if operand is not imm, just return the original
static
Operand* reduceImm64to32(Operand* o)
{
    static Operand newOp;

    if (o->type == OT_Imm64) {
        // reduction possible if signed 64bit fits into signed 32bit
        int64_t v = (int64_t) o->val;
        if ((v > -(1l << 31)) && (v < (1l << 31))) {
            newOp.type = OT_Imm32;
            newOp.val = (uint32_t) (int32_t) v;
            return &newOp;
        }
    }
    return o;
}

static
Operand* reduceImm16to8(Operand* o)
{
    static Operand newOp;

    if (o->type == OT_Imm16) {
        // reduction possible if signed 16bit fits into signed 8bit
        int16_t v = (int16_t) o->val;
        if ((v > -(1<<7)) && (v < (1<<7))) {
            newOp.type = OT_Imm8;
            newOp.val = (uint8_t) (int8_t) v;
            return &newOp;
        }
    }
    return o;
}

static
Operand* reduceImm32to8(Operand* o)
{
    static Operand newOp;

    if (o->type == OT_Imm32) {
        // reduction possible if signed 32bit fits into signed 8bit
        int32_t v = (int32_t) o->val;
        if ((v > -(1<<7)) && (v < (1<<7))) {
            newOp.type = OT_Imm8;
            newOp.val = (uint8_t) (int8_t) v;
            return &newOp;
        }
    }
    return o;
}


// machine code generators for instruction types
//
// 1st par is buffer to write to, with at least 15 bytes space.
// Return number of bytes written

static
int genRet(GContext* cxt)
{
    uint8_t* buf = cxt->buf;
    buf[0] = 0xc3;
    return 1;
}

static
int genPush(GContext* cxt)
{
    int r;
    uint8_t* buf = cxt->buf;
    Operand* o =  &(cxt->instr->dst);

    switch(o->type) {
    case OT_Reg64:
        r = GP64RegEncoding(o->reg);
        if (r > 7) {
            assert(r < 16);
            buf[0] = 0x41; // REX with MASK_B
            buf[1] = 0x50 + r - 8;
            return 2;
        }
        buf[0] = 0x50 + r;
        return 1;

    case OT_Ind64:
        // use 'push r/m 64' (0xFF/6)
        return genDigitRM(cxt, 0xFF, 6, o, GEN_DefOpVT64);

    default:
        break;
    }
    return -1;
}

static
int genPop(GContext* cxt)
{
    uint8_t* buf = cxt->buf;
    Operand* o =  &(cxt->instr->dst);

    if (o->type != OT_Reg64) return -1;
    int r = GP64RegEncoding(o->reg);
    if (r > 7) {
        assert(r < 16);
        buf[0] = 0x41; // REX with MASK_B
        buf[1] = 0x58 + r - 8;
        return 2;
    }
    buf[0] = 0x58 + r;
    return 1;
}

static
int genDec(GContext* cxt)
{
    Operand* dst =  &(cxt->instr->dst);

    switch(dst->type) {
    case OT_Ind32:
    case OT_Ind64:
    case OT_Reg32:
    case OT_Reg64:
      // use 'dec r/m 32/64' (0xFF/1)
      return genDigitRM(cxt, 0xFF, 1, dst, 0);

    default: return -1;
    }
    return 0;
}

static
int genInc(GContext* cxt)
{
    Operand* dst =  &(cxt->instr->dst);

    switch(dst->type) {
    case OT_Ind32:
    case OT_Ind64:
    case OT_Reg32:
    case OT_Reg64:
      // use 'inc r/m 32/64' (0xFF/0)
      return genDigitRM(cxt, 0xFF, 0, dst, 0);

    default: return -1;
    }
    return 0;
}

static
int genNeg(GContext* cxt)
{
    Operand* dst =  &(cxt->instr->dst);

    switch(dst->type) {
    case OT_Ind8:
    case OT_Reg8:
        // use 'neg r/m8' (0xF6/3)
        return genDigitRM(cxt, 0xF6, 3, dst, 0);

    case OT_Ind16:
    case OT_Ind32:
    case OT_Ind64:
    case OT_Reg16:
    case OT_Reg32:
    case OT_Reg64:
      // use 'neg r/m 16/32/64' (0xF7/3)
      return genDigitRM(cxt, 0xF7, 3, dst, GEN_66OnVT16);

    default: return -1;
    }
    return 0;
}


static
int genMov(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    src = reduceImm64to32(src);

    switch(src->type) {
    case OT_Reg32:
    case OT_Reg64:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // movsx
            if ((opValType(src) == VT_32) && (opValType(dst) == VT_64)) {
                Operand eSrc; // extend to 64 bit for genModRM not to fail
                copyOperand(&eSrc, src);
                eSrc.type = (src->type == OT_Reg32) ? OT_Reg64 : OT_Ind64;
                // use 'movsx r64,r/m 32' (0x63)
                return genModRM(cxt, 0x63, &eSrc, dst, VT_None, 0);
            }
            // for regular 'mov r,r' with same operand sizes
            // fall through
        case OT_Ind32:
        case OT_Ind64:
            if (opValType(src) != opValType(dst)) return -1;
            // use 'mov r/m,r 32/64' (0x89 MR)
            return genModRM(cxt, 0x89, dst, src, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Ind32:
    case OT_Ind64:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            if (opValType(src) == opValType(dst)) {
                // use 'mov r,r/m 32/64' (0x8B RM)
                return genModRM(cxt, 0x8B, src, dst, VT_None, 0);
            }
            else if ((opValType(src) == VT_32) &&
                     (opValType(dst) == VT_64)) {
                Operand eSrc; // extend to 64 bit for genModRM not to fail
                copyOperand(&eSrc, src);
                eSrc.type = (src->type == OT_Reg32) ? OT_Reg64 : OT_Ind64;
                // use 'movsx r64,r/m 32' (0x63)
                return genModRM(cxt, 0x63, &eSrc, dst, VT_None, 0);
            }
            break;

        default: return -1;
        }
        break;

    case OT_Imm32:
        switch(dst->type) {
        case OT_Ind32:
        case OT_Ind64:
            // use 'mov r/m 32/64, imm32' (0xC7/0 MI)
            return genDigitMI(cxt, 0xC7, 0, dst, src, 0);

        case OT_Reg32:
        case OT_Reg64:
            if (src->val == 0) {
                // setting to 0: use 'xor r/m,r 32/64' (0x31 MR)
                return genModRM(cxt, 0x31, dst, dst, VT_None, 0);
            }
            // use 'mov r/m 32/64, imm32' (0xC7/0)
            return genDigitMI(cxt, 0xC7, 0, dst, src, 0);

        default: return -1;
        }
        break;

    case OT_Imm64:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            if (src->val == 0) {
                // setting to 0: use 'xor r/m,r 32/64' (0x31 MR)
                return genModRM(cxt, 0x31, dst, dst, VT_None, 0);
            }
            // use 'mov r64,imm64' (REX.W + 0xB8)
            return genOI(cxt, 0xB8, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}


static
int genCMov(GContext* cxt)
{
    InstrType it = cxt->instr->type;
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);
    int opc;

    switch(dst->type) {
    case OT_Reg32:
    case OT_Reg64:
        // dst reg
        switch(src->type) {
        case OT_Ind32:
        case OT_Ind64:
        case OT_Reg32:
        case OT_Reg64:
            if (opValType(src) != opValType(dst)) return -1;
            switch(it) {
            case IT_CMOVO:  opc = 0x40; break; // cmovo  r,r/m 32/64
            case IT_CMOVNO: opc = 0x41; break; // cmovno r,r/m 32/64
            case IT_CMOVC:  opc = 0x42; break; // cmovc  r,r/m 32/64
            case IT_CMOVNC: opc = 0x43; break; // cmovnc r,r/m 32/64
            case IT_CMOVZ:  opc = 0x44; break; // cmovz  r,r/m 32/64
            case IT_CMOVNZ: opc = 0x45; break; // cmovnz r,r/m 32/64
            case IT_CMOVBE: opc = 0x46; break; // cmovbe r,r/m 32/64
            case IT_CMOVA:  opc = 0x47; break; // cmova  r,r/m 32/64
            case IT_CMOVS:  opc = 0x48; break; // cmovs  r,r/m 32/64
            case IT_CMOVNS: opc = 0x49; break; // cmovns r,r/m 32/64
            case IT_CMOVP:  opc = 0x4A; break; // cmovp  r,r/m 32/64
            case IT_CMOVNP: opc = 0x4B; break; // cmovnp r,r/m 32/64
            case IT_CMOVL:  opc = 0x4C; break; // cmovl  r,r/m 32/64
            case IT_CMOVGE: opc = 0x4D; break; // cmovge r,r/m 32/64
            case IT_CMOVLE: opc = 0x4E; break; // cmovle r,r/m 32/64
            case IT_CMOVG:  opc = 0x4F; break; // cmovg  r,r/m 32/64
            default: assert(0);
            }
            // use 'cmov r,r/m 32/64' (opc RM)
            return genModRM(cxt, opc, src, dst, VT_None, 0);
            break;

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genAdd(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);
    src = reduceImm16to8(src);

    switch(src->type) {
    case OT_Reg8:
    case OT_Reg16:
    case OT_Reg32:
    case OT_Reg64:
        // src reg
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg16:
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind16:
        case OT_Ind32:
        case OT_Ind64:
            // use 'add r/m,r 32/64' (0x01 MR)
            return genModRM(cxt, 0x01, dst, src, VT_None, GEN_66OnVT16);

        case OT_Reg8:
        case OT_Ind8:
            // use 'add r/m,r 8' (0x00 MR)
            return genModRM(cxt, 0x00, dst, src, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Ind8:
    case OT_Ind16:
    case OT_Ind32:
    case OT_Ind64:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg8:
            // use 'add r,r/m 8' (0x02 RM)
            return genModRM(cxt, 0x02, src, dst, VT_None, 0);

        case OT_Reg16:
        case OT_Reg32:
        case OT_Reg64:
            // use 'add r,r/m 16/32/64' (0x03 RM)
            return genModRM(cxt, 0x03, src, dst, VT_None, GEN_66OnVT16);

        default: return -1;
        }
        break;

    case OT_Imm8:
        // src imm8
        switch(dst->type) {
        case OT_Reg8:
            if (dst->reg.ri == RI_A) {
                // use 'add al,imm8' (0x04 I)
                uint8_t* buf = cxt->buf;
                buf[0] = 0x04;
                return appendI(buf, 1, src);
            }
            // fall-through
        case OT_Ind8:
            // use 'add r/m 8,imm8' (0x80/0 MI)
            return genDigitMI(cxt, 0x80, 0, dst, src, 0);

        case OT_Reg16:
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind16:
        case OT_Ind32:
        case OT_Ind64:
            // use 'add r/m 16/32/64, imm8' (0x83/0 MI)
            return genDigitMI(cxt, 0x83, 0, dst, src, GEN_66OnVT16);

        default: return -1;
        }
        break;

    case OT_Imm16:
        switch(dst->type) {
        case OT_Reg16:
            if (dst->reg.ri == RI_A) {
                // use 'add ax,imm16' (0x66 0x05 I)
                uint8_t* buf = cxt->buf;
                buf[0] = 0x66; buf[1] = 0x05;
                return appendI(buf, 2, src);
            }
            // fall-through
        case OT_Ind16:
            // use 'add r/m 16,imm16' (0x81/0 MI)
            return genDigitMI(cxt, 0x81, 0, dst, src, GEN_66OnVT16);

        default: return -1;
        }
        break;

    case OT_Imm32:
        // src imm
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            if (dst->reg.ri == RI_A) {
                // use 'add eax/rax,imm32' (0x05 I)
                uint8_t* buf = cxt->buf;
                int o=0;
                if (dst->type == OT_Reg64) buf[o++] = 0x40 | REX_MASK_W;
                buf[o++] = 0x05;
                return appendI(buf, o, src);
            }
            // fall-through
        case OT_Ind32:
        case OT_Ind64:
            // use 'add r/m 32/64, imm32' (0x81/0 MI)
            return genDigitMI(cxt, 0x81, 0, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genSub(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    case OT_Reg32:
    case OT_Reg64:
        if (opValType(src) != opValType(dst)) return -1;
        // src reg
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'sub r/m,r 32/64' (0x29 MR)
            return genModRM(cxt, 0x29, dst, src, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Ind32:
    case OT_Ind64:
        if (opValType(src) != opValType(dst)) return -1;
        // src mem
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'sub r,r/m 32/64' (0x2B RM)
            return genModRM(cxt, 0x2B, src, dst, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'sub r/m 32/64, imm8' (0x83/5 MI)
            return genDigitMI(cxt, 0x83, 5, dst, src, 0);

        default: return -1;
        }
        break;

    case OT_Imm32:
        // src imm32
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'sub r/m 32/64, imm32' (0x81/5 MI)
            return genDigitMI(cxt, 0x81, 5, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genTest(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    // if src is imm, try to reduce width
    src = reduceImm64to32(src);

    switch(src->type) {
    case OT_Reg8:
    case OT_Reg16:
    case OT_Reg32:
    case OT_Reg64:
        // src reg
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg16:
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind16:
        case OT_Ind32:
        case OT_Ind64:
            // use 'test r/m,r 16/32/64' (0x85 MR)
            return genModRM(cxt, 0x85, dst, src, VT_None, GEN_66OnVT16);

        case OT_Reg8:
        case OT_Ind8:
            // use 'test r/m,r 8' (0x84 MR)
            return genModRM(cxt, 0x84, dst, src, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg8:
            if (dst->reg.ri == RI_A) {
                // use 'test al,imm8' (0xA8 I)
                uint8_t* buf = cxt->buf;
                buf[0] = 0xA8;
                return appendI(buf, 1, src);
            }
            // fall-through
        case OT_Ind8:
            // use 'test r/m 8,imm8' (0xF6/0 MI)
            return genDigitMI(cxt, 0xF6, 0, dst, src, 0);

        default: return -1;
        }
        break;

    case OT_Imm16:
        switch(dst->type) {
        case OT_Reg16:
            if (dst->reg.ri == RI_A) {
                // use 'test ax,imm8' (0x66 0xA9 I)
                uint8_t* buf = cxt->buf;
                buf[0] = 0x66; buf[1] = 0xA9;
                return appendI(buf, 2, src);
            }
            // fall-through
        case OT_Ind16:
            // use 'test r/m 16,imm16' (0xF7/0 MI)
            return genDigitMI(cxt, 0xF7, 0, dst, src, GEN_66OnVT16);

        default: return -1;
        }
        break;

    case OT_Imm32:
        // src imm32
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            if (dst->reg.ri == RI_A) {
                // use 'test eax/rax,imm32' (0xA9 I)
                int o=0;
                uint8_t* buf = cxt->buf;
                if (dst->type == OT_Reg64) buf[o++] = 0x40 | REX_MASK_W;
                buf[o++] = 0xA9;
                return appendI(buf, o, src);
            }
            // fall-through
        case OT_Ind32:
        case OT_Ind64:
            // use 'test r/m 16/32/64,imm16/32' (0xF7/0 MI)
            return genDigitMI(cxt, 0xF7, 0, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genIMul(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    case OT_Reg32:
    case OT_Ind32:
    case OT_Reg64:
    case OT_Ind64:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'imul r,r/m 32/64' (0x0F 0xAF RM)
            return genModRM(cxt, 0x0FAF, src, dst, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'imul r,r/m 32/64,imm8' (0x6B/r RMI)
            return genModRMI(cxt, 0x6B, dst, dst, src, 0);

        default: return -1;
        }
        break;

    case OT_Imm32:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'imul r,r/m 32/64,imm32' (0x69/r RMI)
            return genModRMI(cxt, 0x69, dst, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genIDiv1(GContext* cxt)
{
    Operand* src =  &(cxt->instr->dst); // idiv src decoded into instr->dst

    switch(src->type) {
    case OT_Reg32:
    case OT_Ind32:
    case OT_Reg64:
    case OT_Ind64:
        // use 'idiv r/m 32/64' (0xF7/7 M)
        return genDigitRM(cxt, 0xF7, 7, src, 0);

    default: return -1;
    }
    return -1;
}


static
int genXor(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    // src reg
    case OT_Reg32:
    case OT_Reg64:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'xor r/m,r 32/64' (0x31 MR)
            return genModRM(cxt, 0x31, dst, src, VT_None, 0);

        default: return -1;
        }
        break;

    // src mem
    case OT_Ind32:
    case OT_Ind64:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'xor r,r/m 32/64' (0x33 RM)
            return genModRM(cxt, 0x33, src, dst, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Imm8:
        // src imm8
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'xor r/m 32/64, imm8' (0x83/6 MI)
            return genDigitMI(cxt, 0x83, 6, dst, src, 0);

        default: return -1;
        }
        break;

    case OT_Imm32:
        // src imm32
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'xor r/m 32/64, imm32' (0x81/6 MI)
            return genDigitMI(cxt, 0x81, 6, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genOr(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    // src reg
    case OT_Reg32:
    case OT_Reg64:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'or r/m,r 32/64' (0x09 MR)
            return genModRM(cxt, 0x09, dst, src, VT_None, 0);

        default: return -1;
        }
        break;

    // src mem
    case OT_Ind32:
    case OT_Ind64:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'or r,r/m 32/64' (0x0B RM)
            return genModRM(cxt, 0x0B, src, dst, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Imm8:
        // src imm8
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'or r/m 32/64, imm8' (0x83/1 MI)
            return genDigitMI(cxt, 0x83, 1, dst, src, 0);

        default: return -1;
        }
        break;

    case OT_Imm32:
        // src imm32
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'or r/m 32/64, imm32' (0x81/1 MI)
            return genDigitMI(cxt, 0x81, 1, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return -1;
}

static
int genAnd(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);
    src = reduceImm16to8(src);

    switch(src->type) {
    // src reg
    case OT_Reg8:
    case OT_Reg16:
    case OT_Reg32:
    case OT_Reg64:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg16:
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind16:
        case OT_Ind32:
        case OT_Ind64:
            // use 'and r/m,r 16/32/64' (0x21 MR)
            return genModRM(cxt, 0x21, dst, src, VT_None, GEN_66OnVT16);

        case OT_Reg8:
        case OT_Ind8:
            // use 'and r/m,r 8' (0x20 MR)
            return genModRM(cxt, 0x20, dst, src, VT_None, 0);

        default: return -1;
        }
        break;

    // src mem
    case OT_Ind8:
    case OT_Ind16:
    case OT_Ind32:
    case OT_Ind64:
        if (opValType(src) != opValType(dst)) return -1;
        switch(dst->type) {
        case OT_Reg8:
            // use 'and r,r/m 8' (0x22 RM)
            return genModRM(cxt, 0x22, src, dst, VT_None, 0);

        case OT_Reg16:
        case OT_Reg32:
        case OT_Reg64:
            // use 'and r,r/m 32/64' (0x23 RM)
            return genModRM(cxt, 0x23, src, dst, VT_None, GEN_66OnVT16);

        default: return -1;
        }
        break;

    case OT_Imm8:
        // src imm8
        switch(dst->type) {
        case OT_Reg8:
            if (dst->reg.ri == RI_A) {
                // use 'and al,imm8' (0x24 I)
                uint8_t* buf = cxt->buf;
                buf[0] = 0x24;
                return appendI(buf, 1, src);
            }
            // fall-through
        case OT_Ind8:
            // use 'and r/m 8,imm8' (0x80/4 MI)
            return genDigitMI(cxt, 0x80, 4, dst, src, 0);

        case OT_Reg16:
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind16:
        case OT_Ind32:
        case OT_Ind64:
            // use 'and r/m 32/64, imm8' (0x83/4 MI)
            return genDigitMI(cxt, 0x83, 4, dst, src, GEN_66OnVT16);

        default: return -1;
        }
        break;

    case OT_Imm16:
        switch(dst->type) {
        case OT_Reg16:
            if (dst->reg.ri == RI_A) {
                // use 'and ax,imm16' (0x66 0x25 I)
                uint8_t* buf = cxt->buf;
                buf[0] = 0x66; buf[1] = 0x25;
                return appendI(buf, 2, src);
            }
            // fall-through
        case OT_Ind16:
            // use 'and r/m 16,imm16' (0x81/4 MI)
            return genDigitMI(cxt, 0x81, 4, dst, src, GEN_66OnVT16);

        default: return -1;
        }
        break;


    case OT_Imm32:
        // src imm32
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            if (dst->reg.ri == RI_A) {
                // use 'and eax/rax,imm32' (0x25 I)
                int o=0;
                uint8_t* buf = cxt->buf;
                if (dst->type == OT_Reg64) buf[o++] = 0x40 | REX_MASK_W;
                buf[o++] = 0x25;
                return appendI(buf, o, src);
            }
            // fall-through
        case OT_Ind32:
        case OT_Ind64:
            // use 'and r/m 32/64, imm32' (0x81/4 MI)
            return genDigitMI(cxt, 0x81, 4, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}


static
int genShl(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    switch(src->type) {
    // src reg
    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'shl r/m 32/64, imm8' (0xC1/4 MI)
            return genDigitMI(cxt, 0xC1, 4, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genShr(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    switch(src->type) {
    // src reg
    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'shr r/m 32/64, imm8' (0xC1/5 MI)
            return genDigitMI(cxt, 0xC1, 5, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genSar(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    switch(src->type) {
    // src reg
    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'sar r/m 32/64, imm8' (0xC1/7 MI)
            return genDigitMI(cxt, 0xC1, 7, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}


static
int genLea(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    if (!opIsInd(src) || !opIsGPReg(dst)) return -1;
    switch(dst->type) {
    case OT_Reg32:
    case OT_Reg64:
        // use 'lea r/m,r 32/64' (0x8d)
        return genModRM(cxt, 0x8d, src, dst, VT_None, 0);

    default: return -1;
    }
    return 0;
}

static
int genCltq(GContext* cxt)
{
    uint8_t* buf = cxt->buf;
    buf[0] = 0x48;
    buf[1] = 0x98;
    return 2;
}

static
int genCwtl(GContext* cxt)
{
    uint8_t* buf = cxt->buf;
    buf[0] = 0x98;
    return 1;
}


static
int genCqto(GContext* cxt)
{
    uint8_t* buf = cxt->buf;
    ValType vt =  cxt->instr->vtype;

    switch(vt) {
    case VT_64: buf[0] = 0x99; return 1;
    case VT_128: buf[0] = 0x48; buf[1] = 0x99; return 2;
    default: return -1;
    }
    return 0;
}


static
int genCmp(GContext* cxt)
{
    Operand* src =  &(cxt->instr->src);
    Operand* dst =  &(cxt->instr->dst);

    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    // src reg
    case OT_Reg32:
    case OT_Reg64:
        if (opValType(src) != opValType(dst)) return -1;

        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'cmp r/m,r 32/64' (0x39 MR)
            return genModRM(cxt, 0x39, dst, src, VT_None, 0);

        default: return -1;
        }
        break;

    // src mem
    case OT_Ind32:
    case OT_Ind64:
        if (opValType(src) != opValType(dst)) return -1;

        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'cmp r,r/m 32/64' (0x3B RM)
            return genModRM(cxt, 0x3B, src, dst, VT_None, 0);

        default: return -1;
        }
        break;

    case OT_Imm8:
        // src imm8
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'cmp r/m 32/64, imm8' (0x83/7 MI)
            return genDigitMI(cxt, 0x83, 7, dst, src, 0);

        default: return -1;
        }
        break;

    case OT_Imm32:
        // src imm32
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'cmp r/m 32/64, imm32' (0x81/7 MI)
            return genDigitMI(cxt, 0x81, 7, dst, src, 0);

        default: return -1;
        }
        break;

    default: return -1;
    }
    return 0;
}

static
int genVec(GContext* c)
{
    Instr* instr = c->instr;

    switch(instr->type) {
    case IT_ADDSS: setGenInfo(c, PS_F3, 0x0F58, OE_RM, VT_Implicit); break;
    case IT_ADDSD: setGenInfo(c, PS_F2, 0x0F58, OE_RM, VT_Implicit); break;
    case IT_ADDPS: setGenInfo(c, PS_No, 0x0F58, OE_RM, VT_Implicit); break;
    case IT_ADDPD: setGenInfo(c, PS_66, 0x0F58, OE_RM, VT_Implicit); break;
    default: assert(0);
    }

    return genInstr(c);
}

// Pass-through: parser forwarding opcodes, provides encoding
static
int genPassThrough(GContext* cxt)
{
    Instr* instr = cxt->instr;
    ValType vt = instr->vtype;
    int o = 0, opc;

    assert(instr->ptLen > 0);
    cxt->ps = instr->ptPSet;
    cxt->vp = instr->ptVexP;

    assert(instr->ptLen < 3);
    if (instr->ptLen < 2)
        opc = instr->ptOpc[0];
    else
        opc = (instr->ptOpc[0] << 8) | instr->ptOpc[1];

    switch(instr->ptEnc) {
    case OE_None:
        cxt->opc = opc;
        o = genPrefix(cxt);
        o = appendOO(cxt, o);
        break;

    case OE_MR:
        o += genModRM(cxt, opc, &(instr->dst), &(instr->src), vt, 0);
        break;

    case OE_RVM:
        assert(opIsVReg(&(instr->src)));
        cxt->vvvv = instr->src.reg.ri;
        o += genModRM(cxt, opc, &(instr->src2), &(instr->dst), vt, 0);
        break;

    case OE_RM:
        o += genModRM(cxt, opc, &(instr->src), &(instr->dst), vt, 0);
        break;

    default: assert(0);
    }
    return o;
}

// clear generation context for new instruction
static
void initGContext(GContext* c, uint8_t* buf, Instr* i)
{
    c->buf = buf;
    c->instr = i;

    c->vp = VEX_No;
    c->vvvv = 0; // must be 0 to not produce bad code in Vex prefix
    c->rex = 0;
    c->so = OSO_None;
    c->ps = PS_No;
    c->blen = 0;

    c->opc = -1;
    c->oe = OE_Invalid;
    c->flags = 0;
    c->vt = VT_None;
}

// generate code for a captured BB
// this sets cbb->addr1/cbb->size
GenerateError* generate(Rewriter* r, CBB* cbb)
{
    static GenerateError error;

    uint64_t buf0;
    int used, i, usedTotal;
    GContext cxt;

    assert(cbb != 0);

    cxt.e = &error;
    setErrorNone((Error*) cxt.e);

    if (r->cs == 0) {
        markError(&cxt, ET_BufferOverflow, "no code buffer available");
        error.e.r = r;
        error.cbb = cbb;
        return &error;
    }

    if (r->showEmuSteps)
        printf("Generating code for BB %s (%d instructions)\n",
               cbb_prettyName(cbb), cbb->count);

    usedTotal = 0;
    buf0 = (uint64_t) reserveCodeStorage(r->cs, 0); // remember start address
    for(i = 0; i < cbb->count; i++) {
        Instr* instr = cbb->instr + i;

        // pass generator requests via GContext to helpers
        initGContext(&cxt, reserveCodeStorage(r->cs, 15), instr);
        used = 0;

        if (instr->ptLen > 0) {
            used = genPassThrough(&cxt);
        }
        else {
            switch(instr->type) {
            case IT_ADD:
                used = genAdd(&cxt);
                break;
            case IT_CLTQ:
                used = genCltq(&cxt);
                break;
            case IT_CWTL:
                used = genCwtl(&cxt);
                break;
            case IT_CQTO:
                used = genCqto(&cxt);
                break;
            case IT_CMP:
                used = genCmp(&cxt);
                break;
            case IT_DEC:
                used = genDec(&cxt);
                break;
            case IT_IMUL:
                used = genIMul(&cxt);
                break;
            case IT_IDIV1:
                used = genIDiv1(&cxt);
                break;
            case IT_INC:
                used = genInc(&cxt);
                break;
            case IT_NEG:
                used = genNeg(&cxt);
                break;
            case IT_XOR:
                used = genXor(&cxt);
                break;
            case IT_OR:
                used = genOr(&cxt);
                break;
            case IT_AND:
                used = genAnd(&cxt);
                break;
            case IT_SHL:
                used = genShl(&cxt);
                break;
            case IT_SHR:
                used = genShr(&cxt);
                break;
            case IT_SAR:
                used = genSar(&cxt);
                break;
            case IT_LEA:
                used = genLea(&cxt);
                break;
            case IT_MOV:
            case IT_MOVSX: // converting move
                used = genMov(&cxt);
                break;
            case IT_CMOVO:
            case IT_CMOVNO:
            case IT_CMOVC:
            case IT_CMOVNC:
            case IT_CMOVZ:
            case IT_CMOVNZ:
            case IT_CMOVBE:
            case IT_CMOVA:
            case IT_CMOVS:
            case IT_CMOVNS:
            case IT_CMOVP:
            case IT_CMOVNP:
            case IT_CMOVL:
            case IT_CMOVGE:
            case IT_CMOVLE:
            case IT_CMOVG:
                used = genCMov(&cxt);
                break;
            case IT_POP:
                used = genPop(&cxt);
                break;
            case IT_PUSH:
                used = genPush(&cxt);
                break;
            case IT_RET:
                used = genRet(&cxt);
                break;
            case IT_SUB:
                used = genSub(&cxt);
                break;
            case IT_TEST:
                used = genTest(&cxt);
                break;

            case IT_ADDSS:
            case IT_ADDSD:
            case IT_ADDPS:
            case IT_ADDPD:
                used = genVec(&cxt);
                break;

            case IT_HINT_CALL:
            case IT_HINT_RET:
                break;
            default:
                markError(&cxt, ET_UnsupportedInstr, 0);
                break;
            }
        }
        if (used == -1) {
            // if genXXX returns -1, this is error "operands not supported"
            markError(&cxt, ET_UnsupportedOperands, 0);
        }

        assert(used < 15);

        if (isErrorSet((Error*)cxt.e)) {
            // fill-in error info
            error.e.r = r;
            error.cbb = cbb;
            error.offset = i;

            // error: no code generated, reset used buffer
            cbb->size = -1;
            r->cs->used = buf0 - (uint64_t) r->cs->buf;
            return &error;
        }

        instr->addr = (uint64_t) cxt.buf;
        instr->len = used;
        usedTotal += used;

        if (r->showEmuSteps) {
            printf("  I%2d : %-32s", i, instr2string(instr, 1, cbb->fc));
            printf(" (%s)+%-3d",
                   cbb_prettyName(cbb), (int)(instr->addr - buf0));
            if (r->printBytes)
                printf(" %s", bytes2string(instr, 0, used));
            printf("\n");
        }

        useCodeStorage(r->cs, used);
    }

    if (r->showEmuSteps) {
        if (instrIsJcc(cbb->endType)) {
            assert(cbb->nextBranch != 0);
            assert(cbb->nextFallThrough != 0);

        printf("  I%2d : %s (%s),",
               i, instrName(cbb->endType, 0),
               cbb_prettyName(cbb->nextBranch));
        printf(" fall-through to (%s)\n",
               cbb_prettyName(cbb->nextFallThrough));
        }
    }

    cbb->size = usedTotal;
    // start address of generated code.
    // if CBB had no instruction, this points to the padding buffer
    cbb->addr1 = (cbb->count == 0) ? buf0 : cbb->instr[0].addr;

    // no error
    return 0;
}
