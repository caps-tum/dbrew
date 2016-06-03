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

/* For now, decoder only does x86-64
 *
 * Opcodes are defined using instruction descriptors.
 *
 * FIXME:
 * - for 8bit regs, we do not support/assert on use of AH/BH/CH/DH
 * - Eventually support MMX registers
 * - Support VEX/EVEX prefix
 */

#include "decode.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "common.h"
#include "printer.h"
#include "engine.h"
#include "error.h"
#include "instr.h"
#include "instr-descriptors.h"


static
const char*
dbrew_stringify_bytes(uint8_t* instr, int count)
{
    if (count == 0)
        return "";

    static char buf[46];
    for (int i = 0, off = 0; i < count; i++)
        off += snprintf(buf + off, sizeof(buf) - off, " %02x", instr[i]);

    return buf + 1;
}

// Parse MR encoding (r/m,r: op1 is reg or memory operand, op2 is reg/digit),
// or RM encoding (reverse op1/op2 when calling this function).
// Encoding see SDM 2.1
// Input: REX prefix, SegOverride prefix, o1 or o2 may be vector registers
// Fills o1/o2/digit
// Increments offset in context according to parsed number of bytes
static
int parseModRM(uint8_t* p,
               int rex, OpSegOverride o1Seg, bool o1IsVec, bool o2IsVec,
               Operand* o1, Operand* o2, int* digit)
{
    OpType ot;
    int off = 0;

    // The ModR/M byte
    int modrm = p[off++];
    int mod = (modrm & 192) >> 6;
    int reg = (modrm & 56) >> 3;
    int rm = modrm & 7;

    ot = rex & REX_MASK_W ? OT_Reg64 : OT_Reg32;

    // r part: reg or digit, give both back to caller
    if (digit)
        *digit = reg;

    if (o2)
    {
        o2->type = ot;
        o2->reg = (o2IsVec ? Reg_X0 : Reg_AX) + reg + (rex & REX_MASK_R ? 8 : 0);
    }

    if (mod == 3)
    {
        // r, r
        o1->type = ot;
        o1->reg = (o1IsVec ? Reg_X0 : Reg_AX) + rm + (rex & REX_MASK_B ? 8 : 0);
        return off;
    }

    // SIB byte
    int scale = 0;
    int idx = 0;
    int base = 0;
    if (rm == 4)
    {
        int sib = p[off++];
        scale = 1 << ((sib & 192) >> 6);
        idx = (sib & 56) >> 3;
        base = sib & 7;
    }

    // Displacement
    int64_t disp = 0;
    if (mod == 1)
    {
        // 8bit disp: sign extend
        disp = *((int8_t*) (p + off));
        off++;
    }
    else if (mod == 2 || (mod == 0 && (rm == 5 || base == 5)))
    {
        // mod 0 + rm 5: RIP relative
        // mod 0 + base 5: From SIB, implies rm == 4
        disp = *((int32_t*) (p + off));
        off += 4;
    }

    o1->type = rex & REX_MASK_W ? OT_Ind64 : OT_Ind32;
    o1->seg = o1Seg;
    o1->scale = scale;
    o1->val = (uint64_t) disp;

    if (scale == 0)
    {
        if (mod == 0 && rm == 5)
            o1->reg = Reg_IP;
        else
            o1->reg = Reg_AX + rm + (rex & REX_MASK_B ? 8 : 0);

        return off;
    }

    if (idx == 4)
    {
        // no need to use SIB if index register not used
        o1->scale = 0;
        o1->ireg = Reg_None;
    }
    else
        o1->ireg = Reg_AX + idx + (rex & REX_MASK_X ? 8 : 0);

    if (base == 5 && mod == 0)
        o1->reg = Reg_None;
    else
        o1->reg = Reg_AX + base + (rex & REX_MASK_B ? 8 : 0);

    return off;
}

/**
 * Decode one instruction at the given function pointer.
 *
 * \private
 *
 * \param fp The byte stream
 * \param r The rewriter
 * \param isTerminator Set to true when the instruction terminates a basic block
 * \returns The number of bytes consumed, or -1 if there was an error while
 * decoding the instruction.
 **/
static int
dbrew_decode_instruction(uint8_t* fp, Rewriter* r, bool* isTerminator)
{
    PrefixSet ps = PS_None;
    OpSegOverride segmentOverride = OSO_None;
    Operand o1;
    Operand o2;
    Operand o3;
    Instr* instr;
    uint64_t off = 0;
    int rex = 0;
    int digit;
    int immsize;

    do {
        int prefix = fp[off];
        if (prefix == 0x2E)
            ps |= PS_2E;
        else if (prefix >= 0x40 && prefix <= 0x4F)
            rex = prefix & 0xF;
        else if (prefix == 0x64)
            segmentOverride = OSO_UseFS;
        else if (prefix == 0x65)
            segmentOverride = OSO_UseGS;
        else if (prefix == 0x66)
            ps |= PS_66;
        else if (prefix == 0xF2)
            ps |= PS_F2;
        else if (prefix == 0xF3)
            ps |= PS_F3;
        else
            break;
        off++;
    } while (true);

    int opc1 = fp[off++];
    int opc2 = -1;
    int opc3 = -1;
    if (opc1 == 0x0F)
    {
        opc2 = fp[off++];
        if (opc2 == 0x38 || opc2 == 0x3A)
            opc3 = fp[off++];
    }
    int opc[3] = {opc1, opc2, opc3};

    bool success = false;

    // Note: instrDescriptors is defined in instr-descriptors.c
    for (int i = 0; instrDescriptors[i].encoding != OE_Invalid && !success; i++)
    {
        const InstrDescriptor* desc = &instrDescriptors[i];
        int reg = opc[desc->opcCount - 1] & 0x7;
        int condition = opc[desc->opcCount - 1] & 0xF;

        if (desc->prefixes != ps)
            continue;

        if (desc->digit != -1 && desc->digit != (fp[off] & 56) >> 3)
            continue;

        if (desc->opcCount >= 2 && desc->opc[0] != opc[0])
            continue;

        if (desc->opcCount >= 3 && desc->opc[1] != opc[1])
            continue;

        // This leaves to check whether the last opcode is matching. We now
        // modify the opcode in the array as it is not used after this.

        // Temporary (!) variables
        int lastOpc = opc[desc->opcCount - 1];
        int expectedLastOpc = desc->opc[desc->opcCount - 1];

        // Cases where operand is encoded in the opcode
        if ((desc->encoding == OE_O || desc->encoding == OE_OI) && expectedLastOpc == (lastOpc & ~0x7))
            lastOpc = expectedLastOpc;

        // Case where we have a condition encoded in the opcode
        if (desc->conditional && expectedLastOpc == (lastOpc & ~0xF))
            lastOpc = expectedLastOpc;

        if (lastOpc != expectedLastOpc)
            continue;

        success = true;

        bool o1IsVec = (desc->regType & 2) != 0;
        bool o2IsVec = (desc->regType & 1) != 0;
        ValType vti = rex & REX_MASK_W ? VT_64 : VT_32;

        if (instrIsJcc(desc->type) || desc->type == IT_JMP || desc->type == IT_JMPI ||
            desc->type == IT_RET || desc->type == IT_CALL)
            *isTerminator = true;

        // Allocate next instruction.
        instr = r->decInstr + r->decInstrCount;
        assert(r->decInstrCount < r->decInstrCapacity);
        r->decInstrCount++;

        switch (desc->encoding)
        {
            case OE_M:
                off += parseModRM(fp + off, rex, segmentOverride, 0, 0, &o1, 0, &digit);
                if (desc->vto1 != VT_None && desc->vto1 != VT_Implicit)
                    opOverwriteType(&o1, desc->vto1);
                if (desc->vti != VT_None)
                    vti = desc->vti;
                initUnaryInstr(instr, desc->type + (desc->conditional ? condition : 0), &o1);
                break;
            case OE_M1:
                off += parseModRM(fp + off, rex, segmentOverride, 0, 0, &o1, 0, &digit);
                if (desc->vto1 != VT_None && desc->vto1 != VT_Implicit)
                    opOverwriteType(&o1, desc->vto1);
                if (desc->vti != VT_None)
                    vti = desc->vti;
                o2.type = OT_Imm8;
                o2.val = 1;
                initBinaryInstr(instr, desc->type, vti, &o1, &o2);
                break;
            case OE_MI:
                off += parseModRM(fp + off, rex, segmentOverride, 0, 0, &o1, 0, &digit);
                if (desc->vto1 != VT_None && desc->vto1 != VT_Implicit)
                    opOverwriteType(&o1, desc->vto1);
                if (desc->vti != VT_None)
                    vti = desc->vti;
                switch (desc->immsize)
                {
                    case 8: o2.type = OT_Imm8; o2.val = *(uint8_t*)(fp + off); off += 1; break;
                    case 16: o2.type = OT_Imm16; o2.val = *(uint16_t*)(fp + off); off += 2; break;
                    case 32: o2.type = OT_Imm32; o2.val = *(uint32_t*)(fp + off); off += 4; break;
                    case 64: o2.type = OT_Imm64; o2.val = *(uint64_t*)(fp + off); off += 8; break;
                    default: assert(0);
                }
                initBinaryInstr(instr, desc->type, vti, &o1, &o2);
                break;
            case OE_MC:
                off += parseModRM(fp + off, rex, segmentOverride, 0, 0, &o1, 0, &digit);
                if (desc->vto1 != VT_None && desc->vto1 != VT_Implicit)
                    opOverwriteType(&o1, desc->vto1);
                if (desc->vti != VT_None)
                    vti = desc->vti;
                o2.type = OT_Reg8;
                o2.reg = Reg_CX;
                initBinaryInstr(instr, desc->type, vti, &o1, &o2);
                break;
            case OE_RM:
            case OE_MR:
                if (desc->encoding == OE_MR)
                    off += parseModRM(fp + off, rex, segmentOverride, o1IsVec, o2IsVec, &o1, &o2, &digit);
                else
                    off += parseModRM(fp + off, rex, segmentOverride, o2IsVec, o1IsVec, &o2, &o1, &digit);
                if (desc->vto1 != VT_None && desc->vto1 != VT_Implicit)
                    opOverwriteType(&o1, desc->vto1);
                if (desc->vto2 != VT_None && desc->vto2 != VT_Implicit)
                    opOverwriteType(&o2, desc->vto2);
                if (desc->vto1 == VT_Implicit && !o1IsVec)
                    opOverwriteType(&o1, vti);
                if (desc->vto2 == VT_Implicit && !o2IsVec)
                    opOverwriteType(&o2, vti);
                if (desc->vti != VT_None)
                    vti = desc->vti;
                initBinaryInstr(instr, desc->type + (desc->conditional ? condition : 0), vti, &o1, &o2);
                break;
            case OE_RMI:
                off += parseModRM(fp + off, rex, segmentOverride, o2IsVec, o1IsVec, &o2, &o1, &digit);
                if (desc->vto1 != VT_None && desc->vto1 != VT_Implicit)
                    opOverwriteType(&o1, desc->vto1);
                if (desc->vto2 != VT_None && desc->vto2 != VT_Implicit)
                    opOverwriteType(&o2, desc->vto2);
                if (desc->vto1 == VT_Implicit && !o1IsVec)
                    opOverwriteType(&o1, vti);
                if (desc->vto2 == VT_Implicit && !o2IsVec)
                    opOverwriteType(&o2, vti);
                if (desc->vti != VT_None)
                    vti = desc->vti;
                switch (desc->immsize)
                {
                    case 8: o3.type = OT_Imm8; o3.val = *(uint8_t*)(fp + off); off += 1; break;
                    case 16: o3.type = OT_Imm16; o3.val = *(uint16_t*)(fp + off); off += 2; break;
                    case 32: o3.type = OT_Imm32; o3.val = *(uint32_t*)(fp + off); off += 4; break;
                    case 64: o3.type = OT_Imm64; o3.val = *(uint64_t*)(fp + off); off += 8; break;
                    default: assert(0);
                }
                initTernaryInstr(instr, desc->type, &o1, &o2, &o3);
                break;
            case OE_O:
                o1.reg = (o1IsVec ? Reg_X0 : Reg_AX) + reg;
                o1.type = OT_Reg8;
                if (rex & REX_MASK_B)
                    o1.reg += 8;
                if (desc->vti != VT_None)
                    vti = desc->vti;
                opOverwriteType(&o1, vti);
                initUnaryInstr(instr, desc->type, &o1);
                break;
            case OE_OI:
                o1.reg = (o1IsVec ? Reg_X0 : Reg_AX) + reg;
                o1.type = OT_Reg8;
                immsize = desc->immsize;
                if (desc->immsize == 0)
                    immsize = rex & REX_MASK_W ? 64 : 32;
                if (rex & REX_MASK_B)
                    o1.reg += 8;
                if (desc->vti != VT_None)
                    vti = desc->vti;
                opOverwriteType(&o1, vti);
                switch (immsize)
                {
                    case 8: o2.type = OT_Imm8; o2.val = *(uint8_t*)(fp + off); off += 1; break;
                    case 16: o2.type = OT_Imm16; o2.val = *(uint16_t*)(fp + off); off += 2; break;
                    case 32: o2.type = OT_Imm32; o2.val = *(uint32_t*)(fp + off); off += 4; break;
                    case 64: o2.type = OT_Imm64; o2.val = *(uint64_t*)(fp + off); off += 8; break;
                    default: assert(0);
                }
                initBinaryInstr(instr, desc->type, vti, &o1, &o2);
                break;
            case OE_I:
                switch (desc->immsize)
                {
                    case 8: o1.type = OT_Imm8; o1.val = *(uint8_t*)(fp + off); off += 1; break;
                    case 16: o1.type = OT_Imm16; o1.val = *(uint16_t*)(fp + off); off += 2; break;
                    case 32: o1.type = OT_Imm32; o1.val = *(uint32_t*)(fp + off); off += 4; break;
                    case 64: o1.type = OT_Imm64; o1.val = *(uint64_t*)(fp + off); off += 8; break;
                    default: assert(0);
                }
                if (desc->vti != VT_None)
                    vti = desc->vti;
                initUnaryInstr(instr, desc->type, &o1);
                instr->vtype = vti;
                break;
            case OE_IA:
                switch (desc->immsize)
                {
                    case 8: o2.type = OT_Imm8; o2.val = *(uint8_t*)(fp + off); off += 1; break;
                    case 16: o2.type = OT_Imm16; o2.val = *(uint16_t*)(fp + off); off += 2; break;
                    case 32: o2.type = OT_Imm32; o2.val = *(uint32_t*)(fp + off); off += 4; break;
                    case 64: o2.type = OT_Imm64; o2.val = *(uint64_t*)(fp + off); off += 8; break;
                    default: assert(0);
                }
                if (desc->vti != VT_None)
                    vti = desc->vti;
                initBinaryInstr(instr, desc->type, vti, getRegOp(vti, Reg_AX), &o2);
                break;
            case OE_D:
                o1.type = OT_Imm64;
                switch (desc->immsize)
                {
                    case 8: o1.val = *(int8_t*)(fp + off); off += 1; break;
                    case 16: o1.val = *(int16_t*)(fp + off); off += 2; break;
                    case 32: o1.val = *(int32_t*)(fp + off); off += 4; break;
                    case 64: o1.val = *(int64_t*)(fp + off); off += 8; break;
                    default: assert(0);
                }
                o1.val += (uintptr_t) fp + off;
                initUnaryInstr(instr, desc->type + (desc->conditional ? condition : 0), &o1);
                if (instrIsJcc(desc->type))
                    instr->vtype = VT_Implicit;
                break;
            case OE_NP:
                if (desc->vti != VT_Implicit)
                    vti = desc->vti;
                if (desc->opc[0] == 0x99) // CQTO, CQO
                    vti = rex & REX_MASK_W ? VT_128 : VT_64;
                initSimpleInstr(instr, desc->type);
                if (desc->vti != VT_None)
                    instr->vtype = vti;
                break;
            case OE_None:
                off += desc->decodeHandler(fp + off, instr, desc, rex, segmentOverride);
                break;
            default:
                assert(0);
        }

        instr->addr = (uintptr_t) fp;
        instr->len = off;

        attachPassthrough(instr, ps, desc->encoding, opc1, opc2, opc3);
    }

    if (!success)
    {
        printf("Error while decoding. Bytes: %s\n", dbrew_stringify_bytes(fp, 15));
        return -1;
    }

    return off;
}

DBB*
dbrew_decode(Rewriter* r, uint64_t f)
{
    if (f == 0)
        return NULL;

    if (r->decBB == 0)
        initRewriter(r);

    for(int i = 0; i < r->decBBCount; i++)
        if (r->decBB[i].addr == f)
            return &r->decBB[i];

    int old_icount = r->decInstrCount;

    // start decoding of new BB beginning at f
    assert(r->decBBCount < r->decBBCapacity);
    DBB* dbb = &r->decBB[r->decBBCount++];
    dbb->addr = f;
    dbb->fc = config_find_function(r, f);
    dbb->count = 0;
    dbb->size = 0;
    dbb->instr = r->decInstr + r->decInstrCount;

    if (r->showDecoding)
        printf("Decoding BB %s ...\n", prettyAddress(f, dbb->fc));

    uint64_t offset = 0;
    bool terminate = false;
    do
    {
        int newOffset = dbrew_decode_instruction((uint8_t*) f + offset, r, &terminate);

        if (newOffset < 0)
            break;

        offset += newOffset;
    }
    while (!terminate);

    assert(dbb->addr == dbb->instr->addr);
    dbb->count = r->decInstrCount - old_icount;
    dbb->size = offset;

    if (r->showDecoding)
        dbrew_print_decoded(dbb);

    return dbb;
}
