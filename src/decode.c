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

/* For now, decoder only does x86-64 */

#include "decode.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "common.h"
#include "printer.h"
#include "engine.h"

Instr* nextInstr(Rewriter* c, uint64_t a, int len)
{
    Instr* i = c->decInstr + c->decInstrCount;
    assert(c->decInstrCount < c->decInstrCapacity);
    c->decInstrCount++;

    i->addr = a;
    i->len = len;

    i->ptLen = 0;
    i->vtype = VT_None;
    i->form = OF_None;
    i->dst.type = OT_None;
    i->src.type = OT_None;
    i->src2.type = OT_None;

    return i;
}

Instr* addSimple(Rewriter* c, uint64_t a, uint64_t a2, InstrType it)
{
    Instr* i = nextInstr(c, a, a2 - a);
    i->type = it;
    i->form = OF_0;

    return i;
}

Instr* addSimpleVType(Rewriter* c, uint64_t a, uint64_t a2, InstrType it, ValType vt)
{
    Instr* i = nextInstr(c, a, a2 - a);
    i->type = it;
    i->vtype = vt;
    i->form = OF_0;

    return i;
}

Instr* addUnaryOp(Rewriter* c, uint64_t a, uint64_t a2,
                  InstrType it, Operand* o)
{
    Instr* i = nextInstr(c, a, a2 - a);
    i->type = it;
    i->form = OF_1;
    copyOperand( &(i->dst), o);

    return i;
}

Instr* addBinaryOp(Rewriter* c, uint64_t a, uint64_t a2,
                   InstrType it, ValType vt, Operand* o1, Operand* o2)
{
    if ((vt != VT_None) && (vt != VT_Implicit)) {
        // if we specify an explicit value type, it must match destination
        // 2nd operand does not have to match (e.g. conversion/mask extraction)
        assert(vt == opValType(o1));
    }

    Instr* i = nextInstr(c, a, a2 - a);
    i->type = it;
    i->form = OF_2;
    i->vtype = vt;
    copyOperand( &(i->dst), o1);
    copyOperand( &(i->src), o2);

    return i;
}

Instr* addTernaryOp(Rewriter* c, uint64_t a, uint64_t a2,
                    InstrType it, Operand* o1, Operand* o2, Operand* o3)
{
    Instr* i = nextInstr(c, a, a2 - a);
    i->type = it;
    i->form = OF_3;
    copyOperand( &(i->dst), o1);
    copyOperand( &(i->src), o2);
    copyOperand( &(i->src2), o3);

    return i;
}


// Parse RM encoding (r/m,r: op1 is reg or memory operand, op2 is reg/digit)
// Encoding see SDM 2.1
// Input: REX prefix, SegOverride prefix, o1 or o2 may be vector registers
// Fills o1/o2/digit and returns number of bytes parsed
static int parseModRM(uint8_t* p,
               int rex, OpSegOverride o1Seg, Bool o1IsVec, Bool o2IsVec,
               Operand* o1, Operand* o2, int* digit)
{
    int modrm, mod, rm, reg; // modRM byte
    int sib, scale, idx, base; // SIB byte
    int64_t disp;
    Reg r;
    OpType ot;
    int o = 0;
    int hasRex = (rex>0);
    int hasDisp8 = 0, hasDisp32 = 0;

    modrm = p[o++];
    mod = (modrm & 192) >> 6;
    reg = (modrm & 56) >> 3;
    rm = modrm & 7;

    ot = (hasRex && (rex & REX_MASK_W)) ? OT_Reg64 : OT_Reg32;
    // r part: reg or digit, give both back to caller
    if (digit) *digit = reg;
    if (o2) {
        r = (o2IsVec ? Reg_X0 : Reg_AX) + reg;
        if (hasRex && (rex & REX_MASK_R)) r += 8;
        o2->type = ot;
        o2->reg = r;
    }

    if (mod == 3) {
        // r, r
        r = (o1IsVec ? Reg_X0 : Reg_AX) + rm;
        if (hasRex && (rex & REX_MASK_B)) r += 8;
        o1->type = ot;
        o1->reg = r;
        return o;
    }

    if (mod == 1) hasDisp8 = 1;
    if (mod == 2) hasDisp32 = 1;
    if ((mod == 0) && (rm == 5)) {
        // mod 0 + rm 5: RIP relative
        hasDisp32 = 1;
    }

    scale = 0;
    if (rm == 4) {
        // SIB
        sib = p[o++];
        scale = 1 << ((sib & 192) >> 6);
        idx   = (sib & 56) >> 3;
        base  = sib & 7;
        if ((base == 5) && (mod == 0))
            hasDisp32 = 1;
    }

    disp = 0;
    if (hasDisp8) {
        // 8bit disp: sign extend
        disp = *((signed char*) (p+o));
        o++;
    }
    if (hasDisp32) {
        disp = *((int32_t*) (p+o));
        o += 4;
    }

    ot = (hasRex && (rex & REX_MASK_W)) ? OT_Ind64 : OT_Ind32;
    o1->type = ot;
    o1->seg = o1Seg;
    o1->scale = scale;
    o1->val = (uint64_t) disp;
    if (scale == 0) {
        r = Reg_AX + rm;
        if (hasRex && (rex & REX_MASK_B)) r += 8;
        o1->reg = ((mod == 0) && (rm == 5)) ? Reg_IP : r;
        return o;
    }

    if (hasRex && (rex & REX_MASK_X)) idx += 8;
    r = Reg_AX + idx;
    o1->ireg = (idx == 4) ? Reg_None : r;


    if (hasRex && (rex & REX_MASK_B)) base += 8;
    r = Reg_AX + base;
    o1->reg = ((base == 5) && (mod == 0)) ? Reg_None : r;

    // no need to use SIB if index register not used
    if (o1->ireg == Reg_None) o1->scale = 0;

    return o;
}


// decode the basic block starting at f (automatically triggered by emulator)
DBB* dbrew_decode(Rewriter* c, uint64_t f)
{
    Bool hasRex, hasF2, hasF3, has66;
    Bool has2E; // cs-segment override or branch not taken hint (Jcc)
    OpSegOverride segOv;
    int rex;
    uint64_t a;
    int i, off, opc, opc2, digit, old_icount;
    Bool exitLoop;
    uint8_t* fp;
    Operand o1, o2, o3;
    Reg r;
    ValType vt;
    InstrType it;
    Instr* ii;
    DBB* dbb;

    if (f == 0) return 0; // nothing to decode
    if (c->decBB == 0) initRewriter(c);

    // already decoded?
    for(i = 0; i < c->decBBCount; i++)
        if (c->decBB[i].addr == f) return &(c->decBB[i]);


    // start decoding of new BB beginning at f
    assert(c->decBBCount < c->decBBCapacity);
    dbb = &(c->decBB[c->decBBCount]);
    c->decBBCount++;
    dbb->addr = f;
    dbb->fc = config_find_function(c, f);
    dbb->count = 0;
    dbb->size = 0;
    dbb->instr = c->decInstr + c->decInstrCount;
    old_icount = c->decInstrCount;

    if (c->showDecoding)
        printf("Decoding BB %s ...\n", prettyAddress(f, dbb->fc));

    fp = (uint8_t*) f;
    off = 0;
    hasRex = False;
    rex = 0;
    segOv = OSO_None;
    hasF2 = False;
    hasF3 = False;
    has66 = False;
    has2E = False;
    exitLoop = False;
    while(!exitLoop) {
        a = (uint64_t)(fp + off);

        // prefixes
        while(1) {
            if ((fp[off] >= 0x40) && (fp[off] <= 0x4F)) {
                rex = fp[off] & 15;
                hasRex = True;
                off++;
                continue;
            }
            if (fp[off] == 0xF2) {
                hasF2 = True;
                off++;
                continue;
            }
            if (fp[off] == 0xF3) {
                hasF3 = True;
                off++;
                continue;
            }
            if (fp[off] == 0x66) {
                has66 = True;
                off++;
                continue;
            }
            if (fp[off] == 0x64) {
                segOv = OSO_UseFS;
                off++;
                continue;
            }
            if (fp[off] == 0x65) {
                segOv = OSO_UseGS;
                off++;
                continue;
            }
            if (fp[off] == 0x2E) {
                has2E = True;
                off++;
                continue;
            }
            // no further prefixes
            break;
        }

        opc = fp[off++];
        switch(opc) {

        case 0x01:
            // add r/m,r 32/64 (MR, dst: r/m, src: r)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_ADD, vt, &o1, &o2);
            break;

        case 0x03:
            // add r,r/m 32/64 (RM, dst: r, src: r/m)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_ADD, vt, &o1, &o2);
            break;

        case 0x09:
            // or r/m,r 32/64 (MR, dst: r/m, src: r)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_OR, vt, &o1, &o2);
            break;

        case 0x0B:
            // or r,r/m 32/64 (RM, dst: r, src: r/m)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_OR, vt, &o1, &o2);
            break;

        case 0x0F:
            opc2 = fp[off++];
            switch(opc2) {
            case 0xAF:
                // imul r 32/64, r/m 32/64 (RM, dst: r)
                vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
                off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
                addBinaryOp(c, a, (uint64_t)(fp + off), IT_IMUL, vt, &o1, &o2);
                break;

            case 0x10:
                assert(hasF2);
                // movsd xmm2,xmm1/m64 (RM)
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, VT_64);
                opOverwriteType(&o2, VT_64);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_MOVSD, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, PS_F2, OE_RM, SC_None, 0x0F, 0x10, -1);
                break;

            case 0x11:
                assert(hasF2);
                // movsd xmm2/m64,xmm1 (MR)
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o1, &o2, 0);
                opOverwriteType(&o1, VT_64);
                opOverwriteType(&o2, VT_64);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_MOVSD, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, PS_F2, OE_MR, SC_None, 0x0F, 0x11, -1);
                break;

            case 0x1F:
                off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, 0, &digit);
                switch(digit) {
                case 0:
                    // 0F 1F /0: nop r/m 32
                    addUnaryOp(c, a, (uint64_t)(fp + off), IT_NOP, &o1);
                    break;

                default:
                    addSimple(c, a, (uint64_t)(fp + off), IT_Invalid);
                    break;
                }
                break;

            case 0x2E:
                assert(has66);
                // ucomisd xmm1,xmm2/m64 (RM)
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, VT_64);
                opOverwriteType(&o2, VT_64);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_UCOMISD, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, PS_66, OE_RM, SC_None, 0x0F, 0x2E, -1);
                break;

            case 0x40: // cmovo   r,r/m 32/64
            case 0x41: // cmovno  r,r/m 32/64
            case 0x42: // cmovc   r,r/m 32/64
            case 0x43: // cmovnc  r,r/m 32/64
            case 0x44: // cmovz   r,r/m 32/64
            case 0x45: // cmovnz  r,r/m 32/64
            case 0x48: // cmovs   r,r/m 32/64
            case 0x49: // cmovns  r,r/m 32/64
                switch(opc2) {
                case 0x40: it = IT_CMOVO; break;
                case 0x41: it = IT_CMOVNO; break;
                case 0x42: it = IT_CMOVC; break;
                case 0x43: it = IT_CMOVNC; break;
                case 0x44: it = IT_CMOVZ; break;
                case 0x45: it = IT_CMOVNZ; break;
                case 0x48: it = IT_CMOVS; break;
                case 0x49: it = IT_CMOVNS; break;
                default: assert(0);
                }
                vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
                off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
                addBinaryOp(c, a, (uint64_t)(fp + off), it, vt, &o1, &o2);
                break;

            case 0x58:
                assert(hasF2);
                // addsd xmm1,xmm2/m64 (RM)
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, VT_64);
                opOverwriteType(&o2, VT_64);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_ADDSD, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, PS_F2, OE_RM, SC_None, 0x0F, 0x58, -1);
                break;

            case 0x59:
                assert(hasF2);
                // mulsd xmm1,xmm2/m64 (RM)
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, VT_64);
                opOverwriteType(&o2, VT_64);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_MULSD, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, PS_F2, OE_RM, SC_None, 0x0F, 0x59, -1);
                break;

            case 0x5C:
                assert(hasF2);
                // subsd xmm1,xmm2/m64 (RM)
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, VT_64);
                opOverwriteType(&o2, VT_64);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_SUBSD, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, PS_F2, OE_RM, SC_None, 0x0F, 0x5C, -1);
                break;

            case 0x6F:
                assert(hasF3);
                // movdqu xmm1,xmm2/m128 (RM): move unaligned dqw xmm2 -> xmm1
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, VT_128);
                opOverwriteType(&o2, VT_128);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_MOVDQU, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, PS_F3, OE_RM, SC_None, 0x0F, 0x6F, -1);
                break;

            case 0x74:
                // pcmpeqb mm,mm/m 64/128 (RM): compare packed bytes
                vt = has66 ? VT_128 : VT_64;
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, vt);
                opOverwriteType(&o2, vt);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_PCMPEQB, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, has66 ? PS_66:0, OE_RM, SC_None,
                                  0x0F, 0x74, -1);
                break;

            case 0x7E:
                assert(has66);
                // movd/q xmm,r/m 32/64 (RM)
                vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
                off += parseModRM(fp+off, rex, segOv, 1, 0, &o2, &o1, 0);
                opOverwriteType(&o1, vt);
                opOverwriteType(&o2, vt);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_MOV, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, PS_66, OE_RM, SC_dstDyn, 0x0F, 0x7E, -1);
                break;

            case 0x84: // JE/JZ rel32
            case 0x85: // JNE/JNZ rel32
            case 0x8A: // JP rel32
            case 0x8C: // JL/JNGE rel32
            case 0x8D: // JGE/JNL rel32
            case 0x8E: // JLE/JNG rel32
            case 0x8F: // JG/JNLE rel32
                o1.type = OT_Imm64;
                o1.val = (uint64_t) (fp + off + 4 + *(int32_t*)(fp + off));
                off += 4;
                if      (opc2 == 0x84) it = IT_JE;
                else if (opc2 == 0x85) it = IT_JNE;
                else if (opc2 == 0x8A) it = IT_JP;
                else if (opc2 == 0x8C) it = IT_JL;
                else if (opc2 == 0x8D) it = IT_JGE;
                else if (opc2 == 0x8E) it = IT_JLE;
                else if (opc2 == 0x8F) it = IT_JG;
                else assert(0);
                addUnaryOp(c, a, (uint64_t)(fp + off), it, &o1);
                exitLoop = True;
                break;

            case 0xB6:
                // movzbl r32/64,r/m8 (RM): move byte to (d)word, zero-extend
                vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
                off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
                opOverwriteType(&o1, vt);
                opOverwriteType(&o2, VT_8); // src, r/m8
                addBinaryOp(c, a, (uint64_t)(fp + off), IT_MOVZBL, vt, &o1, &o2);
                break;

            case 0xBC:
                // bsf r,r/m 32/64 (RM): bit scan forward
                vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
                off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
                addBinaryOp(c, a, (uint64_t)(fp + off), IT_BSF, vt, &o1, &o2);
                break;

            case 0xD7:
                // pmovmskb r,mm 64/128 (RM): minimum of packed bytes
                vt = has66 ? VT_128 : VT_64;
                off += parseModRM(fp+off, rex, segOv, 1, 0, &o2, &o1, 0);
                opOverwriteType(&o1, VT_32);
                opOverwriteType(&o2, vt);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_PMOVMSKB, VT_32, &o1, &o2);
                attachPassthrough(ii, has66 ? PS_66:0, OE_RM, SC_dstDyn,
                                  0x0F, 0xD7, -1);
                break;

            case 0xDA:
                // pminub mm,mm/m 64/128 (RM): minimum of packed bytes
                vt = has66 ? VT_128 : VT_64;
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, vt);
                opOverwriteType(&o2, vt);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_PMINUB, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, has66 ? PS_66:0, OE_RM, SC_None,
                                  0x0F, 0xDA, -1);
                break;


            case 0xEF:
                // pxor xmm1, xmm2/m 64/128 (RM)
                vt = has66 ? VT_128 : VT_64;
                off += parseModRM(fp+off, rex, segOv, 1, 1, &o2, &o1, 0);
                opOverwriteType(&o1, vt);
                opOverwriteType(&o2, vt);
                ii = addBinaryOp(c, a, (uint64_t)(fp + off),
                                 IT_PXOR, VT_Implicit, &o1, &o2);
                attachPassthrough(ii, has66 ? PS_66 : 0, OE_RM, SC_None,
                                  0x0F, 0xEF, -1);
                break;

            default:
                addSimple(c, a, (uint64_t)(fp + off), IT_Invalid);
                break;
            }
            break;

        case 0x11:
            // adc r/m,r 32/64 (MR, dst: r/m, src: r)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_ADC, vt, &o1, &o2);
            break;

        case 0x13:
            // adc r,r/m 32/64 (RM, dst: r, src: r/m)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_ADC, vt, &o1, &o2);
            break;

        case 0x19:
            // sbb r/m,r 32/64 (MR, dst: r/m, src: r)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_SBB, vt, &o1, &o2);
            break;

        case 0x1B:
            // sbb r,r/m 32/64 (RM, dst: r, src: r/m)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_SBB, vt, &o1, &o2);
            break;

        case 0x21:
            // and r/m,r 32/64 (MR, dst: r/m, src: r)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_AND, vt, &o1, &o2);
            break;

        case 0x23:
            // and r,r/m 32/64 (RM, dst: r, src: r/m)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_AND, vt, &o1, &o2);
            break;

        case 0x25:
            // and eax,imm32
            o1.type = OT_Imm32;
            o1.val = *(uint32_t*)(fp + off);
            off += 4;
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_AND, VT_32,
                        getRegOp(VT_32, Reg_AX), &o1);
            break;

        case 0x29:
            // sub r/m,r 32/64 (MR)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_SUB, vt, &o1, &o2);
            break;

        case 0x2B:
            // sub r,r/m 32/64 (RM)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_SUB, vt, &o1, &o2);
            break;

        case 0x31:
            // xor r/m,r 32/64 (MR, dst: r/m, src: r)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_XOR, vt, &o1, &o2);
            break;

        case 0x33:
            // xor r,r/m 32/64 (RM, dst: r, src: r/m)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_XOR, vt, &o1, &o2);
            break;

        case 0x39:
            // cmp r/m,r 32/64 (MR)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_CMP, vt, &o1, &o2);
            break;

        case 0x3B:
            // cmp r,r/m 32/64 (RM)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_CMP, vt, &o1, &o2);
            break;

        case 0x3D:
            // cmp eax,imm32
            o1.type = OT_Imm32;
            o1.val = *(uint32_t*)(fp + off);
            off += 4;
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_CMP, VT_32,
                        getRegOp(VT_32, Reg_AX), &o1);
            break;

        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57:
            // push
            r = Reg_AX + (opc - 0x50);
            if (hasRex && (rex & REX_MASK_B)) r += 8;
            addUnaryOp(c, a, (uint64_t)(fp + off),
                       IT_PUSH, getRegOp(VT_64, r));
            break;

        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            // pop
            r = Reg_AX + (opc - 0x58);
            if (hasRex && (rex & REX_MASK_B)) r += 8;
            addUnaryOp(c, a, (uint64_t)(fp + off),
                       IT_POP, getRegOp(VT_64, r));
            break;

        case 0x63:
            // movsx r64,r/m32 (RM) mov with sign extension
            assert(rex & REX_MASK_W);
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            // src is 32 bit
            switch(o2.type) {
            case OT_Reg64: o2.type = OT_Reg32; break;
            case OT_Ind64: o2.type = OT_Ind32; break;
            default: assert(0);
            }
            addBinaryOp(c, a, (uint64_t)(fp + off),
                        IT_MOVSX, VT_None, &o1, &o2);
            break;

        case 0x68:
            // push imm32
            o1.type = OT_Imm32;
            o1.val = *(uint32_t*)(fp + off);
            off += 4;
            addUnaryOp(c, a, (uint64_t)(fp + off), IT_PUSH, &o1);
            break;

        case 0x69:
            // imul r,r/m32/64,imm32 (RMI)
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            o3.type = OT_Imm32;
            o3.val = *(uint32_t*)(fp + off);
            off += 4;
            addTernaryOp(c, a, (uint64_t)(fp + off), IT_IMUL, &o1, &o2, &o3);
            break;

        case 0x6A:
            // push imm8
            o1.type = OT_Imm8;
            o1.val = *(uint8_t*)(fp + off);
            off++;
            addUnaryOp(c, a, (uint64_t)(fp + off), IT_PUSH, &o1);
            break;

        case 0x6B:
            // imul r,r/m32/64,imm8 (RMI)
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            o3.type = OT_Imm8;
            o3.val = *(uint8_t*)(fp + off);
            off += 1;
            addTernaryOp(c, a, (uint64_t)(fp + off), IT_IMUL, &o1, &o2, &o3);
            break;


        case 0x74: // JE/JZ rel8
        case 0x75: // JNE/JNZ rel8
        case 0x7A: // JP rel8
        case 0x7C: // JL/JNGE rel8
        case 0x7D: // JGE/JNL rel8
        case 0x7E: // JLE/JNG rel8
        case 0x7F: // JG/JNLE rel8
            o1.type = OT_Imm64;
            o1.val = (uint64_t) (fp + off + 1 + *(int8_t*)(fp + off));
            off += 1;
            if      (opc == 0x74) it = IT_JE;
            else if (opc == 0x75) it = IT_JNE;
            else if (opc == 0x7A) it = IT_JP;
            else if (opc == 0x7C) it = IT_JL;
            else if (opc == 0x7D) it = IT_JGE;
            else if (opc == 0x7E) it = IT_JLE;
            else if (opc == 0x7F) it = IT_JG;
            else assert(0);
            addUnaryOp(c, a, (uint64_t)(fp + off), it, &o1);
            exitLoop = True;
            break;

        case 0x80:
            // add/or/... r/m and imm8
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, 0, &digit);
            switch(digit) {
            case 0: it = IT_ADD; break; // 80/0: add r/m8,imm8
            case 1: it = IT_OR;  break; // 80/1: or  r/m8,imm8
            case 2: it = IT_ADC; break; // 80/2: adc r/m8,imm8
            case 3: it = IT_SBB; break; // 80/3: sbb r/m8,imm8
            case 4: it = IT_AND; break; // 80/4: and r/m8,imm8
            case 5: it = IT_SUB; break; // 80/5: sub r/m8,imm8
            case 6: it = IT_XOR; break; // 80/6: xor r/m8,imm8
            case 7: it = IT_CMP; break; // 80/7: cmp r/m8,imm8
            default: assert(0);
            }
            vt = VT_8;
            opOverwriteType(&o1, vt);
            o2.type = OT_Imm8;
            o2.val = (uint8_t) (*(int8_t*)(fp + off));
            off += 1;
            addBinaryOp(c, a, (uint64_t)(fp + off), it, vt, &o1, &o2);
            break;

        case 0x81:
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, 0, &digit);
            switch(digit) {
            case 0: it = IT_ADD; break; // 81/0: add r/m 32/64, imm32
            case 1: it = IT_OR;  break; // 81/1: or  r/m 32/64, imm32
            case 2: it = IT_ADC; break; // 81/2: adc r/m 32/64, imm32
            case 3: it = IT_SBB; break; // 81/3: sbb r/m 32/64, imm32
            case 4: it = IT_AND; break; // 81/4: and r/m 32/64, imm32
            case 5: it = IT_SUB; break; // 81/5: sub r/m 32/64, imm32
            case 6: it = IT_XOR; break; // 81/6: xor r/m 32/64, imm32
            case 7: it = IT_CMP; break; // 81/7: cmp r/m 32/64, imm32
            default: assert(0);
            }
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            o2.type = OT_Imm32;
            o2.val = *(uint32_t*)(fp + off);
            off += 4;
            addBinaryOp(c, a, (uint64_t)(fp + off), it, vt, &o1, &o2);
            break;

        case 0x83:
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, 0, &digit);
            // add/or/... r/m and sign-extended imm8
            switch(digit) {
            case 0: it = IT_ADD; break; // 83/0: add r/m 32/64, imm8
            case 1: it = IT_OR;  break; // 83/1: or  r/m 32/64, imm8
            case 2: it = IT_ADC; break; // 83/2: adc r/m 32/64, imm8
            case 3: it = IT_SBB; break; // 83/3: sbb r/m 32/64, imm8
            case 4: it = IT_AND; break; // 83/4: and r/m 32/64, imm8
            case 5: it = IT_SUB; break; // 83/5: sub r/m 32/64, imm8
            case 6: it = IT_XOR; break; // 83/6: xor r/m 32/64, imm8
            case 7: it = IT_CMP; break; // 83/7: cmp r/m 32/64, imm8
            default: assert(0);
            }
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            o2.type = OT_Imm8;
            o2.val = (uint8_t) (*(int8_t*)(fp + off));
            off += 1;
            addBinaryOp(c, a, (uint64_t)(fp + off), it, vt, &o1, &o2);
            break;

        case 0x85:
            // test r/m,r 32/64 (dst: r/m, src: r)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_TEST, vt, &o1, &o2);
            break;

        case 0x89:
            // mov r/m,r 32/64 (dst: r/m, src: r)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, &o2, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_MOV, vt, &o1, &o2);
            break;

        case 0x8B:
            // mov r,r/m 32/64 (dst: r, src: r/m)
            vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_MOV, vt, &o1, &o2);
            break;

        case 0x8D:
            // lea r32/64,m
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o2, &o1, 0);
            assert(opIsInd(&o2)); // TODO: bad code error
            addBinaryOp(c, a, (uint64_t)(fp + off),
                        IT_LEA, VT_None, &o1, &o2);
            break;

        case 0x90:
            // nop
            addSimple(c, a, (uint64_t)(fp + off), IT_NOP);
            break;

        case 0x98:
            // cltq (Intel: cdqe - sign-extend eax to rax)
            addSimpleVType(c, a, (uint64_t)(fp + off), IT_CLTQ,
                           hasRex && (rex & REX_MASK_W) ? VT_64 : VT_32);
            break;

        case 0x99:
            // cqto (Intel: cqo - sign-extend rax to rdx/rax, eax to edx/eax)
            addSimpleVType(c, a, (uint64_t)(fp + off), IT_CQTO,
                           hasRex && (rex & REX_MASK_W) ? VT_128 : VT_64);
            break;

        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            // MOV r32/64,imm32/64
            o1.reg = Reg_AX + (opc - 0xB8);
            if (rex & REX_MASK_R) o1.reg += 8;
            if (rex & REX_MASK_W) {
                vt = VT_64;
                o1.type = OT_Reg64;
                o2.type = OT_Imm64;
                o2.val = *(uint64_t*)(fp + off);
                off += 8;
            }
            else {
                vt = VT_32;
                o1.type = OT_Reg32;
                o2.type = OT_Imm32;
                o2.val = *(uint32_t*)(fp + off);
                off += 4;
            }
            addBinaryOp(c, a, (uint64_t)(fp + off), IT_MOV, vt, &o1, &o2);
            break;

        case 0xC1:
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, 0, &digit);
            switch(digit) {
            case 4:
                // shl r/m 32/64,imm8 (MI) (= sal)
                o2.type = OT_Imm8;
                o2.val = *(uint8_t*)(fp + off);
                off += 1;
                addBinaryOp(c, a, (uint64_t)(fp + off),
                            IT_SHL, VT_None, &o1, &o2);
                break;

            case 5:
                // shr r/m 32/64,imm8 (MI)
                o2.type = OT_Imm8;
                o2.val = *(uint8_t*)(fp + off);
                off += 1;
                addBinaryOp(c, a, (uint64_t)(fp + off),
                            IT_SHR, VT_None, &o1, &o2);
                break;

            case 7:
                // sar r/m 32/64,imm8 (MI)
                o2.type = OT_Imm8;
                o2.val = *(uint8_t*)(fp + off);
                off += 1;
                addBinaryOp(c, a, (uint64_t)(fp + off),
                            IT_SAR, VT_None, &o1, &o2);
                break;


            default:
                addSimple(c, a, (uint64_t)(fp + off), IT_Invalid);
                break;
            }
            break;

        case 0xC3:
            // ret
            addSimple(c, a, (uint64_t)(fp + off), IT_RET);
            exitLoop = True;
            break;

        case 0xC7:
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, 0, &digit);
            switch(digit) {
            case 0:
                // mov r/m 32/64, imm32
                vt = (rex & REX_MASK_W) ? VT_64 : VT_32;
                o2.type = OT_Imm32;
                o2.val = *(uint32_t*)(fp + off);
                off += 4;
                addBinaryOp(c, a, (uint64_t)(fp + off), IT_MOV, vt, &o1, &o2);
                break;

            default:
                addSimple(c, a, (uint64_t)(fp + off), IT_Invalid);
                break;
            }
            break;

        case 0xC9:
            // leave ( = mov rbp,rsp + pop rbp)
            addSimple(c, a, (uint64_t)(fp + off), IT_LEAVE);
            break;

        case 0xE8:
            // call rel32
            o1.type = OT_Imm64;
            o1.val = (uint64_t) (fp + off + 4 + *(int32_t*)(fp + off));
            off += 4;
            addUnaryOp(c, a, (uint64_t)(fp + off), IT_CALL, &o1);
            exitLoop = True;
            break;

        case 0xE9:
            // jmp rel32: relative, displacement relative to next instruction
            o1.type = OT_Imm64;
            o1.val = (uint64_t) (fp + off + 4 + *(int32_t*)(fp + off));
            off += 4;
            addUnaryOp(c, a, (uint64_t)(fp + off), IT_JMP, &o1);
            exitLoop = True;
            break;

        case 0xEB:
            // jmp rel8: relative, displacement relative to next instruction
            o1.type = OT_Imm64;
            o1.val = (uint64_t) (fp + off + 1 + *(int8_t*)(fp + off));
            off += 1;
            addUnaryOp(c, a, (uint64_t)(fp + off), IT_JMP, &o1);
            exitLoop = True;
            break;

        case 0xF7:
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, 0, &digit);
            switch(digit) {
            case 3:
                // neg r/m 32/64
                addUnaryOp(c, a, (uint64_t)(fp + off), IT_NEG, &o1);
                break;

            case 7:
                // idiv r/m 32/64 (signed divide eax/rax by rm
                addUnaryOp(c, a, (uint64_t)(fp + off), IT_IDIV1, &o1);
                break;

            default:
                addSimple(c, a, (uint64_t)(fp + off), IT_Invalid);
                break;
            }
            break;

        case 0xFF:
            off += parseModRM(fp+off, rex, segOv, 0, 0, &o1, 0, &digit);
            switch(digit) {
            case 0:
                // inc r/m 32/64
                addUnaryOp(c, a, (uint64_t)(fp + off), IT_INC, &o1);
                break;

            case 1:
                // dec r/m 32/64
                addUnaryOp(c, a, (uint64_t)(fp + off), IT_DEC, &o1);
                break;

            case 2:
                // call r/m64
                addUnaryOp(c, a, (uint64_t)(fp + off), IT_CALL, &o1);
                exitLoop = True;
                break;

            case 4:
                // jmp* r/m64: absolute indirect
                assert(rex == 0);
                opOverwriteType(&o1, VT_64);
                addUnaryOp(c, a, (uint64_t)(fp + off), IT_JMPI, &o1);
                exitLoop = True;
                break;

            default:
                addSimple(c, a, (uint64_t)(fp + off), IT_Invalid);
                break;
            }
            break;

        default:
            addSimple(c, a, (uint64_t)(fp + off), IT_Invalid);
            break;
        }
        hasRex = False;
        rex = 0;
        segOv = OSO_None;
        hasF2 = False;
        hasF3 = False;
        has66 = False;
        has2E = False;
    }

    assert(dbb->addr == dbb->instr->addr);
    dbb->count = c->decInstrCount - old_icount;
    dbb->size = off;

    if (c->showDecoding)
        dbrew_print_decoded(dbb);

    return dbb;
}

