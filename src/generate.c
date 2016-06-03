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


/*------------------------------------------------------------*/
/* x86_64 code generation
 */

// helpers for operand encodings

// return 0 - 15 for RAX - R15
static
int GPRegEncoding(Reg r)
{
    assert((r >= Reg_AX) && (r <= Reg_15));
    return r - Reg_AX;
}

// return 0 - 15 for XMM0 - XMM15
static
int VRegEncoding(Reg r)
{
    assert((r >= Reg_X0) && (r <= Reg_X15));
    return r - Reg_X0;
}

// returns static buffer with requested operand encoding
static
uint8_t* calcModRMDigit(Operand* o1, int digit,
                        int* prex, OpSegOverride* pso, int* plen)
{
    static uint8_t buf[10];
    int modrm, r1;
    int o = 0;
    ValType vt;

    assert((digit>=0) && (digit<8));
    assert(opIsReg(o1) || opIsInd(o1));

    vt = opValType(o1);
    if (vt == VT_64) *prex |= REX_MASK_W;

    modrm = (digit & 7) << 3;

    if (opIsReg(o1)) {
        // r,r: mod 3
        modrm |= 192;
        if (opIsGPReg(o1))
            r1 = GPRegEncoding(o1->reg);
        else if (opIsVReg(o1))
            r1 = VRegEncoding(o1->reg);
        else
            assert(0);
        if (r1 & 8) *prex |= REX_MASK_B;
        modrm |= (r1 & 7);
        buf[o++] = modrm;
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
        *pso = o1->seg;

        if ((o1->scale == 0) && (o1->reg != Reg_SP)) {
            // no SIB needed (reg not sp which requires SIB)
            if (o1->reg == Reg_None) {
                useDisp32 = 1; // encoding needs disp32
                useDisp8 = 0;
                modrm &= 63; // mod needs to be 00
                useSIB = 1;
                sib = (4 << 3) + 5; // index 4 (= none) + base 5 (= none)
            }
            else {
                if (o1->reg == Reg_IP) {
                    // should not happen, we converted RIP-rel to absolute
                    assert(0);
                    // RIP relative
                    r1 = 5;
                    modrm &= 63;
                    useDisp32 = 1;
                }
                else {
                    r1 = GPRegEncoding(o1->reg);

                    if ((r1 == 5) && (v==0)) {
                        // encoding for rbp without displacement is reused
                        // for RIP-relative addressing!
                        // we need to enforce +disp8 with disp8 = 0
                        // (see SDM, table 2-5 in 2.2.1.2)
                        useDisp8 = 1;
                        assert(modrm < 64); // check that mod = 0
                        modrm |= 64;
                    }
                }
                if (r1 & 8) *prex |= REX_MASK_B;
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

            if ((o1->scale == 0) || (o1->ireg == Reg_None)) {
                // no index register: uses index 4 (usually SP, not allowed)
                sib |= (4 << 3);
            }
            else {
                ri = GPRegEncoding(o1->ireg);
                // offset 4 not allowed here, used for "no scaling"
                assert(ri != 4);
                if (ri & 8) *prex |= REX_MASK_X;
                sib |= (ri & 7) <<3;
            }

            if (o1->reg == Reg_None) {
                // encoding requires disp32 with mod = 00 / base 5 = none
                useDisp32 = 1;
                useDisp8 = 0;
                modrm &= 63;
                sib |= 5;
            }
            else {
                if (o1->reg == Reg_BP) {
                    // cannot use mod == 00
                    if ((modrm & 192) == 0) {
                        modrm |= 64;
                        useDisp8 = 1;
                    }
                }
                rb = GPRegEncoding(o1->reg);
                if (rb & 8) *prex |= REX_MASK_B;
                sib |= (rb & 7);
            }
        }

        if (useSIB)
            modrm |= 4; // signal SIB in modrm
        buf[o++] = modrm;
        if (useSIB)
            buf[o++] = sib;
        if (useDisp8)
            buf[o++] = (int8_t) v;
        if (useDisp32) {
            *(int32_t*)(buf+o) = (int32_t) v;
            o += 4;
        }
    }

    *plen = o;
    return buf;
}

static
uint8_t* calcModRM(Operand* o1, Operand* o2,
                   int* prex, OpSegOverride* pso, int* plen)
{
    int r2; // register offset encoding for operand 2

    assert(opValType(o1) == opValType(o2));

    if (opIsGPReg(o2)) {
        assert(opIsReg(o1) || opIsInd(o1));
        r2 = GPRegEncoding(o2->reg);
    }
    else if (opIsVReg(o2)) {
        assert(opIsReg(o1) || opIsInd(o1));
        r2 = VRegEncoding(o2->reg);
    }
    else assert(0);

    if (r2 & 8) *prex |= REX_MASK_R;
    return calcModRMDigit(o1, r2 & 7, prex, pso, plen);
}

static
int genPrefix(uint8_t* buf, int rex, OpSegOverride so)
{
    int o = 0;
    if (so == OSO_UseFS) buf[o++] = 0x64;
    if (so == OSO_UseGS) buf[o++] = 0x65;
    if (rex)
        buf[o++] = 0x40 | rex;
    return o;
}

// Generate instruction with operand encoding RM (o1: r/m, o2: r)
// into buf, up to 2 opcodes (2 if opc2 >=0).
// If result type (vt) is explicitly specified as "VT_Implicit", do not
// automatically generate REX prefix depending on operand types.
// Returns byte length of generated instruction.
static
int genModRM(uint8_t* buf, int opc, int opc2,
             Operand* o1, Operand* o2, ValType vt)
{
    OpSegOverride so = OSO_None;
    int rex = 0, len = 0;
    int o = 0;
    uint8_t* rmBuf;

    rmBuf = calcModRM(o1, o2, &rex, &so, &len);
    if (vt == VT_Implicit) rex &= ~REX_MASK_W;
    if (vt == VT_Implicit_REXW) rex |= REX_MASK_W;
    o += genPrefix(buf, rex, so);
    buf[o++] = (uint8_t) opc;
    if (opc2 >=0)
        buf[o++] = (uint8_t) opc2;
    while(len>0) {
        buf[o++] = *rmBuf++;
        len--;
    }
    return o;
}

// Operand o1: r/m
static
int genDigitRM(uint8_t* buf, int opc, int digit, Operand* o1)
{
    OpSegOverride so = OSO_None;
    int rex = 0, len = 0;
    int o = 0;
    uint8_t* rmBuf;

    rmBuf = calcModRMDigit(o1, digit, &rex, &so, &len);
    o += genPrefix(buf, rex, so);
    buf[o++] = (uint8_t) opc;
    while(len>0) {
        buf[o++] = *rmBuf++;
        len--;
    }
    return o;
}

// Operand o1: r/m, o2: r, o3: imm
static
int genModRMI(uint8_t* buf, int opc, int opc2,
              Operand* o1, Operand* o2, Operand* o3)
{
    OpSegOverride so = OSO_None;
    int rex = 0, len = 0;
    int o = 0;
    uint8_t* rmBuf;

    rmBuf = calcModRM(o1, o2, &rex, &so, &len);
    o += genPrefix(buf, rex, so);
    buf[o++] = (uint8_t) opc;
    if (opc2 >=0)
        buf[o++] = (uint8_t) opc2;
    while(len>0) {
        buf[o++] = *rmBuf++;
        len--;
    }
    assert(opIsImm(o3));
    switch(opValType(o3)) {
    case VT_8:
        buf[o++] = (uint8_t) o3->val;
        break;
    case VT_32:
        *(uint32_t*)(buf+o) = (uint32_t) o3->val;
        o += 4;
        break;
    default: assert(0);
    }

    return o;
}

// Operand o1: r/m, o2: imm
static
int genDigitMI(uint8_t* buf, int opc, int digit, Operand* o1, Operand* o2)
{
    OpSegOverride so = OSO_None;
    int rex = 0, len = 0;
    int o = 0;
    uint8_t* rmBuf;

    assert(opIsImm(o2));
    rmBuf = calcModRMDigit(o1, digit, &rex, &so, &len);
    o += genPrefix(buf, rex, so);
    buf[o++] = (uint8_t) opc;
    while(len>0) {
        buf[o++] = *rmBuf++;
        len--;
    }

    // immediate
    switch(o2->type) {
    case OT_Imm8:
        *(uint8_t*)(buf + o) = (uint8_t) o2->val;
        o += 1;
        break;

    case OT_Imm32:
        *(uint32_t*)(buf + o) = (uint32_t) o2->val;
        o += 4;
        break;

    default: assert(0);
    }

    return o;
}

// Operand o1: r (gets part of opcode), o2: imm
static
int genOI(uint8_t* buf, int opc, Operand* o1, Operand* o2)
{
    int rex = 0;
    int o = 0, r;

    assert(opIsReg(o1));
    assert(opIsImm(o2));

    r = GPRegEncoding(o1->reg);
    if (r & 8) rex |= REX_MASK_B;
    if (opValType(o1) == VT_64) rex |= REX_MASK_W;

    if (rex)
        buf[o++] = 0x40 | rex;
    buf[o++] = (uint8_t) (opc + (r & 7));

    // immediate
    switch(o2->type) {
    case OT_Imm8:
        *(uint8_t*)(buf + o) = (uint8_t) o2->val;
        o += 1;
        break;

    case OT_Imm32:
        *(uint32_t*)(buf + o) = (uint32_t) o2->val;
        o += 4;
        break;

    case OT_Imm64:
        *(uint64_t*)(buf + o) = o2->val;
        o += 8;
        break;

    default: assert(0);
    }

    return o;
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
int genRet(uint8_t* buf)
{
    buf[0] = 0xc3;
    return 1;
}

static
int genPush(uint8_t* buf, Operand* o)
{
    assert(o->type == OT_Reg64);
    if ((o->reg >= Reg_AX) && (o->reg <= Reg_DI)) {
        buf[0] = 0x50 + (o->reg - Reg_AX);
        return 1;
    }
    else if ((o->reg >= Reg_8) && (o->reg <= Reg_15)) {
        buf[0] = 0x41; // REX with MASK_B
        buf[1] = 0x50 + (o->reg - Reg_8);
        return 2;
    }
    assert(0);
}

static
int genPop(uint8_t* buf, Operand* o)
{
    assert(o->type == OT_Reg64);
    if ((o->reg >= Reg_AX) && (o->reg <= Reg_DI)) {
        buf[0] = 0x58 + (o->reg - Reg_AX);
        return 1;
    }
    else if ((o->reg >= Reg_8) && (o->reg <= Reg_15)) {
        buf[0] = 0x41; // REX with MASK_B
        buf[1] = 0x58 + (o->reg - Reg_8);
        return 2;
    }
    assert(0);
}

static
int genDec(uint8_t* buf, Operand* dst)
{
    switch(dst->type) {
    case OT_Ind32:
    case OT_Ind64:
    case OT_Reg32:
    case OT_Reg64:
      // use 'dec r/m 32/64' (0xFF/1)
      return genDigitRM(buf, 0xFF, 1, dst);

    default: assert(0);
    }
    return 0;
}

static
int genInc(uint8_t* buf, Operand* dst)
{
    switch(dst->type) {
    case OT_Ind32:
    case OT_Ind64:
    case OT_Reg32:
    case OT_Reg64:
      // use 'inc r/m 32/64' (0xFF/0)
      return genDigitRM(buf, 0xFF, 0, dst);

    default: assert(0);
    }
    return 0;
}

static
int genMov(uint8_t* buf, Operand* src, Operand* dst)
{
    src = reduceImm64to32(src);

    switch(dst->type) {
    case OT_Ind32:
    case OT_Ind64:
        // dst memory
        switch(src->type) {
        case OT_Reg32:
        case OT_Reg64:
            assert(opValType(src) == opValType(dst));
            // use 'mov r/m,r 32/64' (0x89 MR)
            return genModRM(buf, 0x89, -1, dst, src, VT_None);

        case OT_Imm32:
            // use 'mov r/m 32/64, imm32' (0xC7/0 MI)
            return genDigitMI(buf, 0xC7, 0, dst, src);

        default: assert(0);
        }
        break;

    case OT_Reg32:
    case OT_Reg64:
        // dst reg
        switch(src->type) {
        case OT_Ind32:
        case OT_Ind64:
        case OT_Reg32:
        case OT_Reg64:
            if (opValType(src) == opValType(dst)) {
                // use 'mov r,r/m 32/64' (0x8B RM)
                return genModRM(buf, 0x8B, -1, src, dst, VT_None);
            }
            else if ((opValType(src) == VT_32) &&
                     (opValType(dst) == VT_64)) {
                Operand eSrc; // extend to 64 bit for genModRM not to fail
                copyOperand(&eSrc, src);
                eSrc.type = (src->type == OT_Reg32) ? OT_Reg64 : OT_Ind64;
                // use 'movsx r64 ,r/m 32' (0x63)
                return genModRM(buf, 0x63, -1, &eSrc, dst, VT_None);
            }
            break;

        case OT_Imm32:
            if (src->val == 0) {
                // setting to 0: use 'xor r/m,r 32/64' (0x31 MR)
                return genModRM(buf, 0x31, -1, dst, dst, VT_None);
            }
            // use 'mov r/m 32/64, imm32' (0xC7/0)
            return genDigitMI(buf, 0xC7, 0, dst, src);

        case OT_Imm64: {
            if (src->val == 0) {
                // setting to 0: use 'xor r/m,r 32/64' (0x31 MR)
                return genModRM(buf, 0x31, -1, dst, dst, VT_None);
            }
            // use 'mov r64,imm64' (REX.W + 0xB8)
            return genOI(buf, 0xB8, dst, src);
        }

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genCMov(uint8_t* buf, InstrType it, Operand* src, Operand* dst)
{
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
            assert(opValType(src) == opValType(dst));
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
            return genModRM(buf, opc, -1, src, dst, VT_None);
            break;

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genAdd(uint8_t* buf, Operand* src, Operand* dst)
{
    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    case OT_Reg32:
    case OT_Reg64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'add r/m,r 32/64' (0x01 MR)
            return genModRM(buf, 0x01, -1, dst, src, VT_None);

        default: assert(0);
        }
        break;

    case OT_Ind32:
    case OT_Ind64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'add r,r/m 32/64' (0x03 RM)
            return genModRM(buf, 0x03, -1, src, dst, VT_None);

        default: assert(0);
        }
        break;

    case OT_Imm8:
        // src imm8
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'add r/m 32/64, imm8' (0x83/0 MI)
            return genDigitMI(buf, 0x83, 0, dst, src);

        default: assert(0);
        }
        break;

    case OT_Imm32:
        // src imm
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'add r/m 32/64, imm32' (0x81/0 MI)
            return genDigitMI(buf, 0x81, 0, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genSub(uint8_t* buf, Operand* src, Operand* dst)
{
    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    case OT_Reg32:
    case OT_Reg64:
        assert(opValType(src) == opValType(dst));
        // src reg
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'sub r/m,r 32/64' (0x29 MR)
            return genModRM(buf, 0x29, -1, dst, src, VT_None);

        default: assert(0);
        }
        break;

    case OT_Ind32:
    case OT_Ind64:
        assert(opValType(src) == opValType(dst));
        // src mem
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'sub r,r/m 32/64' (0x2B RM)
            return genModRM(buf, 0x2B, -1, src, dst, VT_None);

        default: assert(0);
        }
        break;

    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'sub r/m 32/64, imm8' (0x83/5 MI)
            return genDigitMI(buf, 0x83, 5, dst, src);

        default: assert(0);
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
            return genDigitMI(buf, 0x81, 5, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genTest(uint8_t* buf, Operand* src, Operand* dst)
{
    switch(src->type) {
    case OT_Reg8:
    case OT_Reg32:
    case OT_Reg64:
        assert(opValType(src) == opValType(dst));
        // src reg
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'test r/m,r 32/64' (0x85 MR)
            return genModRM(buf, 0x85, -1, dst, src, VT_None);

        case OT_Reg8:
        case OT_Ind8:
            // use 'test r/m,r 8' (0x84 MR)
            // FIXME: this does not generate VEX prefix for spl/bpl/sil/dil
            return genModRM(buf, 0x84, -1, dst, src, VT_None);


        default: assert(0);
        }
        break;

    case OT_Imm32:
        // src imm32
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'sub r/m 32/64,imm32' (0xF7/0 MI)
            return genDigitMI(buf, 0xF7, 0, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genIMul(uint8_t* buf, Operand* src, Operand* dst)
{
    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    case OT_Reg32:
    case OT_Ind32:
    case OT_Reg64:
    case OT_Ind64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'imul r,r/m 32/64' (0x0F 0xAF RM)
            return genModRM(buf, 0x0F, 0xAF, src, dst, VT_None);

        default: assert(0);
        }
        break;

    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'imul r,r/m 32/64,imm8' (0x6B/r RMI)
            return genModRMI(buf, 0x6B, -1, dst, dst, src);

        default: assert(0);
        }
        break;

    case OT_Imm32:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'imul r,r/m 32/64,imm32' (0x69/r RMI)
            return genModRMI(buf, 0x69, -1, dst, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genIDiv1(uint8_t* buf, Operand* src)
{
    switch(src->type) {
    case OT_Reg32:
    case OT_Ind32:
    case OT_Reg64:
    case OT_Ind64:
        // use 'idiv r/m 32/64' (0xF7/7 M)
        return genDigitRM(buf, 0xF7, 7, src);

    default: assert(0);
    }
    return 0;
}


static
int genXor(uint8_t* buf, Operand* src, Operand* dst)
{
    switch(src->type) {
    // src reg
    case OT_Reg32:
    case OT_Reg64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'xor r/m,r 32/64' (0x31 MR)
            return genModRM(buf, 0x31, -1, dst, src, VT_None);

        default: assert(0);
        }
        break;

    // src mem
    case OT_Ind32:
    case OT_Ind64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'xor r,r/m 32/64' (0x33 RM)
            return genModRM(buf, 0x33, -1, src, dst, VT_None);

        default: assert(0);
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
            return genDigitMI(buf, 0x83, 6, dst, src);

        default: assert(0);
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
            return genDigitMI(buf, 0x81, 6, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genOr(uint8_t* buf, Operand* src, Operand* dst)
{
    switch(src->type) {
    // src reg
    case OT_Reg32:
    case OT_Reg64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'or r/m,r 32/64' (0x09 MR)
            return genModRM(buf, 0x09, -1, dst, src, VT_None);

        default: assert(0);
        }
        break;

    // src mem
    case OT_Ind32:
    case OT_Ind64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'or r,r/m 32/64' (0x0B RM)
            return genModRM(buf, 0x0B, -1, src, dst, VT_None);

        default: assert(0);
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
            return genDigitMI(buf, 0x83, 1, dst, src);

        default: assert(0);
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
            return genDigitMI(buf, 0x81, 1, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genAnd(uint8_t* buf, Operand* src, Operand* dst)
{
    switch(src->type) {
    // src reg
    case OT_Reg32:
    case OT_Reg64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'and r/m,r 32/64' (0x21 MR)
            return genModRM(buf, 0x21, -1, dst, src, VT_None);

        default: assert(0);
        }
        break;

    // src mem
    case OT_Ind32:
    case OT_Ind64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'and r,r/m 32/64' (0x23 RM)
            return genModRM(buf, 0x23, -1, src, dst, VT_None);

        default: assert(0);
        }
        break;

    case OT_Imm8:
        // src imm8
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'and r/m 32/64, imm8' (0x83/4 MI)
            return genDigitMI(buf, 0x83, 4, dst, src);

        default: assert(0);
        }
        break;

    case OT_Imm32:
        // src imm32
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
        case OT_Ind32:
        case OT_Ind64:
            // use 'and r/m 32/64, imm32' (0x81/4 MI)
            return genDigitMI(buf, 0x81, 4, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}


static
int genShl(uint8_t* buf, Operand* src, Operand* dst)
{
    switch(src->type) {
    // src reg
    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'shl r/m 32/64, imm8' (0xC1/4 MI)
            return genDigitMI(buf, 0xC1, 4, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genShr(uint8_t* buf, Operand* src, Operand* dst)
{
    switch(src->type) {
    // src reg
    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'shr r/m 32/64, imm8' (0xC1/5 MI)
            return genDigitMI(buf, 0xC1, 5, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}

static
int genSar(uint8_t* buf, Operand* src, Operand* dst)
{
    switch(src->type) {
    // src reg
    case OT_Imm8:
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'sar r/m 32/64, imm8' (0xC1/7 MI)
            return genDigitMI(buf, 0xC1, 7, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}


static
int genLea(uint8_t* buf, Operand* src, Operand* dst)
{
    assert(opIsInd(src));
    assert(opIsGPReg(dst));
    switch(dst->type) {
    case OT_Reg32:
    case OT_Reg64:
        // use 'lea r/m,r 32/64' (0x8d)
        return genModRM(buf, 0x8d, -1, src, dst, VT_None);

    default: assert(0);
    }
    return 0;
}

static
int genCltq(uint8_t* buf, ValType vt)
{
    switch(vt) {
    case VT_32: buf[0] = 0x98; return 1;
    case VT_64: buf[0] = 0x48; buf[1] = 0x98; return 2;
    default: assert(0);
    }
    return 0;
}

static
int genCqto(uint8_t* buf, ValType vt)
{
    switch(vt) {
    case VT_64: buf[0] = 0x99; return 1;
    case VT_128: buf[0] = 0x48; buf[1] = 0x99; return 2;
    default: assert(0);
    }
    return 0;
}


static
int genCmp(uint8_t* buf, Operand* src, Operand* dst)
{
    // if src is imm, try to reduce width
    src = reduceImm64to32(src);
    src = reduceImm32to8(src);

    switch(src->type) {
    // src reg
    case OT_Reg32:
    case OT_Reg64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Ind32:
        case OT_Reg64:
        case OT_Ind64:
            // use 'cmp r/m,r 32/64' (0x39 MR)
            return genModRM(buf, 0x39, -1, dst, src, VT_None);

        default: assert(0);
        }
        break;

    // src mem
    case OT_Ind32:
    case OT_Ind64:
        assert(opValType(src) == opValType(dst));
        switch(dst->type) {
        case OT_Reg32:
        case OT_Reg64:
            // use 'cmp r,r/m 32/64' (0x3B RM)
            return genModRM(buf, 0x3B, -1, src, dst, VT_None);

        default: assert(0);
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
            return genDigitMI(buf, 0x83, 7, dst, src);

        default: assert(0);
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
            return genDigitMI(buf, 0x81, 7, dst, src);

        default: assert(0);
        }
        break;

    default: assert(0);
    }
    return 0;
}


// Pass-through: parser forwarding opcodes, provides encoding
static
int genPassThrough(uint8_t* buf, Instr* instr)
{
    int o = 0;
    ValType vt = instr->vtype;

    assert(instr->ptLen > 0);
    if (instr->ptPSet & PS_66) buf[o++] = 0x66;
    if (instr->ptPSet & PS_F2) buf[o++] = 0xF2;
    if (instr->ptPSet & PS_F3) buf[o++] = 0xF3;
    if (instr->ptPSet & PS_REXW) {
        assert(vt == VT_Implicit);
        vt = VT_Implicit_REXW;
    }

    // FIXME: REX prefix pass-through: combine with RM encoding changes

    if (instr->ptLen < 2) instr->ptOpc[1] = -1;
    assert(instr->ptLen < 3);

    switch(instr->ptEnc) {
    case OE_MR:
        o += genModRM(buf+o, instr->ptOpc[0], instr->ptOpc[1],
                &(instr->dst), &(instr->src), vt);
        break;

    case OE_RM:
        o += genModRM(buf+o, instr->ptOpc[0], instr->ptOpc[1],
                &(instr->src), &(instr->dst), vt);
        break;

    default: assert(0);
    }

    return o;
}




// generate code for a captured BB
void generate(Rewriter* r, CBB* cbb)
{
    uint8_t* buf;
    uint64_t buf0;
    int used, i, usedTotal;

    if (cbb == 0) return;
    if (r->cs == 0) return;

    if (r->showEmuSteps)
        printf("Generating code for BB %s (%d instructions)\n",
               cbb_prettyName(cbb), cbb->count);

    usedTotal = 0;
    buf0 = (uint64_t) reserveCodeStorage(r->cs, 0); // remember start address
    for(i = 0; i < cbb->count; i++) {
        Instr* instr = cbb->instr + i;

        buf = reserveCodeStorage(r->cs, 15);

        switch(instr->type) {
        case IT_ADD:
            used = genAdd(buf, &(instr->src), &(instr->dst));
            break;
        case IT_CLTQ:
            used = genCltq(buf, instr->vtype);
            break;
        case IT_CQTO:
            used = genCqto(buf, instr->vtype);
            break;
        case IT_CMP:
            used = genCmp(buf, &(instr->src), &(instr->dst));
            break;
        case IT_DEC:
            used = genDec(buf, &(instr->dst));
            break;
        case IT_IMUL:
            used = genIMul(buf, &(instr->src), &(instr->dst));
            break;
        case IT_IDIV1:
            used = genIDiv1(buf, &(instr->dst));
            break;
        case IT_INC:
            used = genInc(buf, &(instr->dst));
            break;
        case IT_XOR:
            used = genXor(buf, &(instr->src), &(instr->dst));
            break;
        case IT_OR:
            used = genOr(buf, &(instr->src), &(instr->dst));
            break;
        case IT_AND:
            used = genAnd(buf, &(instr->src), &(instr->dst));
            break;
        case IT_SHL:
            used = genShl(buf, &(instr->src), &(instr->dst));
            break;
        case IT_SHR:
            used = genShr(buf, &(instr->src), &(instr->dst));
            break;
        case IT_SAR:
            used = genSar(buf, &(instr->src), &(instr->dst));
            break;
        case IT_LEA:
            used = genLea(buf, &(instr->src), &(instr->dst));
            break;
        case IT_MOV:
        case IT_MOVSX: // converting move
            used = genMov(buf, &(instr->src), &(instr->dst));
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
            used = genCMov(buf, instr->type, &(instr->src), &(instr->dst));
            break;
        case IT_POP:
            used = genPop(buf, &(instr->dst));
            break;
        case IT_PUSH:
            used = genPush(buf, &(instr->dst));
            break;
        case IT_RET:
            used = genRet(buf);
            break;
        case IT_SUB:
            used = genSub(buf, &(instr->src), &(instr->dst));
            break;
        case IT_TEST:
            used = genTest(buf, &(instr->src), &(instr->dst));
            break;
        case IT_HINT_CALL:
        case IT_HINT_RET:
            used = 0;
            break;
        default:
            if (instr->ptLen > 0)
                used = genPassThrough(buf, instr);
            else
                assert(0);
        }
        assert(used < 15);

        instr->addr = (uint64_t) buf;
        instr->len = used;
        usedTotal += used;

        if (r->showEmuSteps) {
            printf("  I%2d : %-32s", i, instr2string(instr, 1, 0));
            printf(" (%s)+%lx %s\n",
                   cbb_prettyName(cbb), instr->addr - buf0,
                   bytes2string(instr, 0, used));
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

    // add padding space after generated code for jump instruction
    buf = useCodeStorage(r->cs, 10);

    cbb->size = usedTotal;
    // start address of generated code.
    // if CBB had no instruction, this points to the padding buffer
    cbb->addr1 = (cbb->count == 0) ? ((uint64_t)buf) : cbb->instr[0].addr;
}
