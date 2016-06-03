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

#ifndef INSTR_DESCRIPTOR_H
#define INSTR_DESCRIPTOR_H

#include "instr.h"


struct InstrDescriptor;

typedef struct InstrDescriptor InstrDescriptor;

typedef int(* DecodeHandler)(uint8_t* fp, Instr* instr, const InstrDescriptor* desc, int rex, OpSegOverride segment);
// typedef int(* GenerateHandler)(uint8_t* fp, Instr* instr, const InstrDescriptor* desc);

/**
 * An instruction descriptor for an X86-64 instruction. This includes most
 * general instructions and SSE instructions.
 *
 * Instructions with a VEX prefix require a different descriptor type, because
 * the prefix includes the REX prefix, can imply opcodes and supports three
 * register/memory operands as well as up to four operands in total.
 **/
struct InstrDescriptor {
    /**
     * \brief The encoding of the instruction
     **/
    OperandEncoding encoding;

    /**
     * \brief The number of opcodes
     **/
    uint8_t opcCount;

    /**
     * \brief The opcodes, -1 implies that this opcode is not used.
     **/
    int16_t opc[3];

    /**
     * \brief The prefix set, except the REX prefix
     **/
    PrefixSet prefixes;

    /**
     * \brief The types of the registers, one of RT_{G,V}{G,V}
     **/
    uint8_t regType;

    /**
     * \brief The value type of the first operand
     *
     * Further explanation:
     *  - #VT_None means no override
     *  - #VT_Implicit means #vti if the register is a general purpose register
     *  - All other values mean an explicit override
     **/
    ValType vto1;

    /**
     * \brief The value type of the second operand
     *
     * See #vto1 for further details.
     **/
    ValType vto2;

    /**
     * \brief The value type of the instruction
     *
     * Further explanation:
     *  - #VT_None means #VT_32 or #VT_64, depending on REX.W
     *  - All other values mean an explicit override
     *
     * Special cases are handled in the #dbrew_decode_instruction function.
     **/
    ValType vti;

    /**
     * \brief The ModR/M digit for M, MC and MI encodings
     **/
    int8_t digit;

    /**
     * \brief The size of the immediate
     **/
    uint8_t immsize;

    /**
     * \brief Whether the instruction is conditional
     **/
    bool conditional;

    /**
     * \brief The instruction type
     **/
    InstrType type;

    /**
     * \brief Custom decode handler, used for #OE_None
     **/
    DecodeHandler decodeHandler;

    // /**
    //  * \brief Custom generate handler, used for #OE_None
    //  **/
    // GenerateHandler generateHandler;
};

extern const InstrDescriptor instrDescriptors[];

#endif
