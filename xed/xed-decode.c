
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <common.h>
#include <instr.h>
#include <printer.h>

#include <xed/xed-interface.h>

#include "dbrew-xed.h"


__attribute__((constructor))
static void
dbrew_xed_init(void)
{
    xed_tables_init();
}

static const InstrType xedTypeMapping[] = {
    [XED_ICLASS_ADC] = IT_ADC,
    [XED_ICLASS_ADD] = IT_ADD,
    [XED_ICLASS_ADDPD] = IT_ADDPD,
    [XED_ICLASS_ADDPS] = IT_ADDPS,
    [XED_ICLASS_ADDSD] = IT_ADDSD,
    [XED_ICLASS_ADDSS] = IT_ADDSS,
    [XED_ICLASS_ADDSUBPD] = IT_ADDSUBPD,
    [XED_ICLASS_ADDSUBPS] = IT_ADDSUBPS,
    [XED_ICLASS_AND] = IT_AND,
    [XED_ICLASS_ANDNPD] = IT_ANDNPD,
    [XED_ICLASS_ANDNPS] = IT_ANDNPS,
    [XED_ICLASS_ANDPD] = IT_ANDPD,
    [XED_ICLASS_ANDPS] = IT_ANDPS,
    [XED_ICLASS_BSF] = IT_BSF,
    [XED_ICLASS_CALL_NEAR] = IT_CALL,
    [XED_ICLASS_CMOVB] = IT_CMOVC,
    [XED_ICLASS_CMOVBE] = IT_CMOVBE,
    [XED_ICLASS_CMOVL] = IT_CMOVL,
    [XED_ICLASS_CMOVLE] = IT_CMOVLE,
    [XED_ICLASS_CMOVNB] = IT_CMOVNC,
    [XED_ICLASS_CMOVNBE] = IT_CMOVA,
    [XED_ICLASS_CMOVNL] = IT_CMOVGE,
    [XED_ICLASS_CMOVNLE] = IT_CMOVG,
    [XED_ICLASS_CMOVNO] = IT_CMOVNO,
    [XED_ICLASS_CMOVNP] = IT_CMOVNP,
    [XED_ICLASS_CMOVNS] = IT_CMOVNS,
    [XED_ICLASS_CMOVO] = IT_CMOVO,
    [XED_ICLASS_CMOVP] = IT_CMOVP,
    [XED_ICLASS_CMOVS] = IT_CMOVS,
    [XED_ICLASS_CMOVZ] = IT_CMOVZ,
    [XED_ICLASS_CMP] = IT_CMP,
    [XED_ICLASS_DEC] = IT_DEC,
    [XED_ICLASS_DIV] = IT_DIV,
    [XED_ICLASS_DIVPD] = IT_DIVPD,
    [XED_ICLASS_DIVPS] = IT_DIVPS,
    [XED_ICLASS_DIVSD] = IT_DIVSD,
    [XED_ICLASS_DIVSS] = IT_DIVSS,
    [XED_ICLASS_HADDPD] = IT_HADDPD,
    [XED_ICLASS_HADDPS] = IT_HADDPS,
    [XED_ICLASS_HSUBPD] = IT_HSUBPD,
    [XED_ICLASS_HSUBPS] = IT_HSUBPS,
    [XED_ICLASS_IDIV] = IT_IDIV1,
    [XED_ICLASS_IMUL] = IT_IMUL,
    [XED_ICLASS_INC] = IT_INC,
    [XED_ICLASS_JB] = IT_JC,
    [XED_ICLASS_JBE] = IT_JBE,
    [XED_ICLASS_JL] = IT_JL,
    [XED_ICLASS_JLE] = IT_JLE,
    [XED_ICLASS_JMP] = IT_JMP,
    [XED_ICLASS_JNB] = IT_JNC,
    [XED_ICLASS_JNBE] = IT_JA,
    [XED_ICLASS_JNL] = IT_JGE,
    [XED_ICLASS_JNLE] = IT_JG,
    [XED_ICLASS_JNO] = IT_JNO,
    [XED_ICLASS_JNP] = IT_JNP,
    [XED_ICLASS_JNS] = IT_JNS,
    [XED_ICLASS_JNZ] = IT_JNZ,
    [XED_ICLASS_JO] = IT_JO,
    [XED_ICLASS_JP] = IT_JP,
    [XED_ICLASS_JS] = IT_JS,
    [XED_ICLASS_JZ] = IT_JZ,
    [XED_ICLASS_LEA] = IT_LEA,
    [XED_ICLASS_LEAVE] = IT_LEAVE,
    [XED_ICLASS_MAXPD] = IT_MAXPD,
    [XED_ICLASS_MAXPS] = IT_MAXPS,
    [XED_ICLASS_MAXSD] = IT_MAXSD,
    [XED_ICLASS_MAXSS] = IT_MAXSS,
    [XED_ICLASS_MINPD] = IT_MINPD,
    [XED_ICLASS_MINPS] = IT_MINPS,
    [XED_ICLASS_MINSD] = IT_MINSD,
    [XED_ICLASS_MINSS] = IT_MINSS,
    [XED_ICLASS_MOV] = IT_MOV,
    [XED_ICLASS_MOVAPD] = IT_MOVAPD,
    [XED_ICLASS_MOVAPS] = IT_MOVAPS,
    [XED_ICLASS_MOVD] = IT_MOVD,
    [XED_ICLASS_MOVDQA] = IT_MOVDQA,
    [XED_ICLASS_MOVDQU] = IT_MOVDQU,
    [XED_ICLASS_MOVHPD] = IT_MOVHPD,
    [XED_ICLASS_MOVHPS] = IT_MOVHPS,
    [XED_ICLASS_MOVLPD] = IT_MOVLPD,
    [XED_ICLASS_MOVLPS] = IT_MOVLPS,
    [XED_ICLASS_MOVQ] = IT_MOVQ,
    [XED_ICLASS_MOVSD_XMM] = IT_MOVSD,
    [XED_ICLASS_MOVSS] = IT_MOVSS,
    [XED_ICLASS_MOVSX] = IT_MOVSX,
    [XED_ICLASS_MOVUPD] = IT_MOVUPD,
    [XED_ICLASS_MOVUPS] = IT_MOVUPS,
    [XED_ICLASS_MOVZX] = IT_MOVZX,
    [XED_ICLASS_MUL] = IT_MUL,
    [XED_ICLASS_MULPD] = IT_MULPD,
    [XED_ICLASS_MULPS] = IT_MULPS,
    [XED_ICLASS_MULSD] = IT_MULSD,
    [XED_ICLASS_MULSS] = IT_MULSS,
    [XED_ICLASS_NEG] = IT_NEG,
    [XED_ICLASS_NOP2] = IT_NOP,
    [XED_ICLASS_NOP3] = IT_NOP,
    [XED_ICLASS_NOP4] = IT_NOP,
    [XED_ICLASS_NOP5] = IT_NOP,
    [XED_ICLASS_NOP6] = IT_NOP,
    [XED_ICLASS_NOP7] = IT_NOP,
    [XED_ICLASS_NOP8] = IT_NOP,
    [XED_ICLASS_NOP9] = IT_NOP,
    [XED_ICLASS_NOP] = IT_NOP,
    [XED_ICLASS_NOT] = IT_NOT,
    [XED_ICLASS_OR] = IT_OR,
    [XED_ICLASS_ORPD] = IT_ORPD,
    [XED_ICLASS_ORPS] = IT_ORPS,
    [XED_ICLASS_POP] = IT_POP,
    [XED_ICLASS_PUSH] = IT_PUSH,
    [XED_ICLASS_PXOR] = IT_PXOR,
    [XED_ICLASS_RET_NEAR] = IT_RET,
    // [XED_ICLASS_ROL] = IT_ROL,
    // [XED_ICLASS_ROR] = IT_ROR,
    [XED_ICLASS_SAR] = IT_SAR,
    [XED_ICLASS_SBB] = IT_SBB,
    [XED_ICLASS_SETB] = IT_SETC,
    [XED_ICLASS_SETBE] = IT_SETBE,
    [XED_ICLASS_SETL] = IT_SETL,
    [XED_ICLASS_SETLE] = IT_SETLE,
    [XED_ICLASS_SETNB] = IT_SETNC,
    [XED_ICLASS_SETNBE] = IT_SETA,
    [XED_ICLASS_SETNL] = IT_SETGE,
    [XED_ICLASS_SETNLE] = IT_SETG,
    [XED_ICLASS_SETNO] = IT_SETNO,
    [XED_ICLASS_SETNP] = IT_SETNP,
    [XED_ICLASS_SETNS] = IT_SETNS,
    [XED_ICLASS_SETO] = IT_SETO,
    [XED_ICLASS_SETP] = IT_SETP,
    [XED_ICLASS_SETS] = IT_SETS,
    [XED_ICLASS_SETZ] = IT_SETZ,
    [XED_ICLASS_SHL] = IT_SHL,
    [XED_ICLASS_SHR] = IT_SHR,
    [XED_ICLASS_SQRTPD] = IT_SQRTPD,
    [XED_ICLASS_SQRTPS] = IT_SQRTPS,
    [XED_ICLASS_SQRTSD] = IT_SQRTSD,
    [XED_ICLASS_SQRTSS] = IT_SQRTSS,
    [XED_ICLASS_SUB] = IT_SUB,
    [XED_ICLASS_SUBPD] = IT_SUBPD,
    [XED_ICLASS_SUBPS] = IT_SUBPS,
    [XED_ICLASS_SUBSD] = IT_SUBSD,
    [XED_ICLASS_SUBSS] = IT_SUBSS,
    [XED_ICLASS_TEST] = IT_TEST,
    [XED_ICLASS_UNPCKHPD] = IT_UNPCKHPD,
    [XED_ICLASS_UNPCKHPS] = IT_UNPCKHPS,
    [XED_ICLASS_UNPCKLPD] = IT_UNPCKLPD,
    [XED_ICLASS_UNPCKLPS] = IT_UNPCKLPS,
    [XED_ICLASS_XOR] = IT_XOR,
    [XED_ICLASS_XORPD] = IT_XORPD,
    [XED_ICLASS_XORPS] = IT_XORPS,
    [XED_ICLASS_LAST] = IT_None
};

struct InstrChain {
    struct InstrChain* next;
    uintptr_t address;
    xed_decoded_inst_t xedd;
};

typedef struct InstrChain InstrChain;

static InstrChain*
dbrew_xed_decode_instr(uintptr_t address, xed_state_t* dstate)
{
    InstrChain* instr = malloc(sizeof(InstrChain));
    instr->next = NULL;
    instr->address = address;

    xed_decoded_inst_zero_set_mode(&instr->xedd, dstate);
    xed_error_enum_t xed_error = xed_decode(&instr->xedd, (uint8_t*) address, 15);

    if (xed_error != XED_ERROR_NONE)
    {
        printf("Error while decoding at address %p: %s", (uint8_t*) address, xed_error_enum_t2str(xed_error));
        free(instr);
        return NULL;
    }

    return instr;
}

static void
dbrew_xed_transform_operand(Operand* op, uintptr_t address, const xed_decoded_inst_t* xedd, int i, const xed_operand_t* xop)
{
    xed_operand_enum_t name = xed_operand_name(xop);

    switch (name)
    {
        case XED_OPERAND_AGEN:
        case XED_OPERAND_MEM0:
        {
            size_t bytes = xed_decoded_inst_get_memory_operand_length(xedd, 0);
            if (bytes == 0) op->type = OT_Ind64; // LEA
            if (bytes == 1) op->type = OT_Ind8;
            if (bytes == 2) op->type = OT_Ind16;
            if (bytes == 4) op->type = OT_Ind32;
            if (bytes == 8) op->type = OT_Ind64;
            if (bytes == 16) op->type = OT_Ind128;
            xed_reg_enum_t base = xed_decoded_inst_get_base_reg(xedd, 0);
            xed_reg_enum_t index = xed_decoded_inst_get_index_reg(xedd, 0);
            if (base != XED_REG_INVALID && base != XED_REG_RIP)
                op->reg = getReg(RT_GP64, base - XED_REG_RAX);
            else if (base == XED_REG_RIP)
                op->reg = getReg(RT_IP, 0);
            else
                op->reg = getReg(RT_None, 0);
            if (index != XED_REG_INVALID)
            {
                op->ireg = getReg(RT_GP64, index - XED_REG_RAX);
                op->scale = xed_decoded_inst_get_scale(xedd, 0);
            }
            else
            {
                op->ireg = getReg(RT_None, 0);
                op->scale = 0;
            }
            if (xed_operand_values_has_memory_displacement(xedd))
                op->val = xed_decoded_inst_get_memory_displacement(xedd, 0);
            else
                op->val = 0;
            op->seg = OSO_None; // TODO: xed_decoded_inst_get_seg_reg(xedd, i)
            break;
        }
        case XED_OPERAND_REG0:
        case XED_OPERAND_REG1:
        case XED_OPERAND_REG2:
        case XED_OPERAND_REG3:
        case XED_OPERAND_REG4:
        case XED_OPERAND_REG5:
        case XED_OPERAND_REG6:
        case XED_OPERAND_REG7:
        case XED_OPERAND_REG8:
        {
            xed_reg_enum_t reg = xed_decoded_inst_get_reg(xedd, name);
            size_t bits = xed_decoded_inst_operand_length_bits(xedd, i);

            if (bits == 8) op->type = OT_Reg8;
            if (bits == 16) op->type = OT_Reg16;
            if (bits == 32) op->type = OT_Reg32;
            if (bits == 64) op->type = OT_Reg64;
            if (bits == 128) op->type = OT_Reg128;
            if (bits == 256) op->type = OT_Reg256;

            // xed_reg_class_enum_t clazz = xed_reg_class(reg);
            // printf("%s=%s\n",
            //            xed_reg_class_enum_t2str(clazz),
            //            xed_reg_enum_t2str(reg));
            if (reg >= XED_REG_RAX && reg <= XED_REG_R15)
                op->reg = getReg(RT_GP64, reg - XED_REG_RAX);
            else if (reg >= XED_REG_EAX && reg <= XED_REG_R15D)
                op->reg = getReg(RT_GP32, reg - XED_REG_EAX);
            else if (reg >= XED_REG_AX && reg <= XED_REG_R15W)
                op->reg = getReg(RT_GP16, reg - XED_REG_AX);
            else if (reg >= XED_REG_AL && reg <= XED_REG_R15B)
                op->reg = getReg(RT_GP8, reg - XED_REG_AL);
            else if (reg >= XED_REG_AH && reg <= XED_REG_BH)
                op->reg = getReg(RT_GP8Leg, RI_AH + reg - XED_REG_AH);
            else if (reg >= XED_REG_XMM0 && reg <= XED_REG_XMM31)
                op->reg = getReg(RT_XMM, reg - XED_REG_XMM0);
            else if (reg >= XED_REG_YMM0 && reg <= XED_REG_YMM31)
                op->reg = getReg(RT_YMM, reg - XED_REG_YMM0);
            else abort();
            break;
        }
        case XED_OPERAND_IMM0:
        {
            size_t ibits = xed_decoded_inst_get_immediate_width_bits(xedd);
            if (ibits == 8) op->type = OT_Imm8;
            if (ibits == 16) op->type = OT_Imm16;
            if (ibits == 32) op->type = OT_Imm32;
            if (ibits == 64) op->type = OT_Imm64;
            op->val = xed_decoded_inst_get_unsigned_immediate(xedd);
            break;
        }
        case XED_OPERAND_RELBR:
        {
            int32_t disp = xed_decoded_inst_get_branch_displacement(xedd);
            op->val = address + xed_decoded_inst_get_length(xedd) + disp;
            op->type = OT_Imm64;
            break;
        }
        default:
            printf("Unsupported operand: %s", xed_operand_enum_t2str(name));
    }
}

DBB*
dbrew_xed_decode(int debug, uintptr_t address)
{
    xed_state_t dstate;
    xed_state_zero(&dstate);
    dstate.mmode = XED_MACHINE_MODE_LONG_64;
    dstate.stack_addr_width = XED_ADDRESS_WIDTH_64b;

    InstrChain* firstInstr = dbrew_xed_decode_instr(address, &dstate);
    InstrChain* currentInstr = firstInstr;
    uintptr_t currentAddr = address;
    size_t count = 0;

    while (currentInstr != NULL)
    {
        count++;

        xed_category_enum_t cat = xed_decoded_inst_get_category(&currentInstr->xedd);
        if (cat == XED_CATEGORY_CALL || cat == XED_CATEGORY_RET ||
            cat == XED_CATEGORY_COND_BR || cat == XED_CATEGORY_UNCOND_BR)
            break;

        currentAddr += xed_decoded_inst_get_length(&currentInstr->xedd);

        InstrChain* newInstr = malloc(sizeof(InstrChain));
        newInstr->next = NULL;
        newInstr->address = currentAddr;

        xed_decoded_inst_zero_set_mode(&newInstr->xedd, &dstate);
        xed_error_enum_t xed_error = xed_decode(&newInstr->xedd, (uint8_t*) currentAddr, 15);

        if (xed_error != XED_ERROR_NONE)
        {
            printf("Error while decoding at address %p: %s", (uint8_t*) currentAddr, xed_error_enum_t2str(xed_error));
            free(newInstr);
            newInstr = NULL;
        }

        currentInstr = currentInstr->next = newInstr;
    }

    DBB* dbb = malloc(sizeof(Instr) * count + sizeof(DBB));
    Instr* dbrewInstrs = (Instr*) (dbb + 1);

    currentInstr = firstInstr;
    for (size_t i = 0; i < count; i++)
    {
        xed_decoded_inst_t* xedd = &currentInstr->xedd;
        const xed_inst_t* xi = xed_decoded_inst_inst(xedd);
        Instr* inst = &dbrewInstrs[i];

        uint32_t opCount = xed_inst_noperands(xi);

        inst->type = xedTypeMapping[xed_decoded_inst_get_iclass(xedd)];
        inst->addr = currentInstr->address;
        inst->len = xed_decoded_inst_get_length(xedd);
        inst->vtype = VT_Implicit;

        size_t opWidth = xed_decoded_inst_get_operand_width(xedd);
        if (opWidth == 8) inst->vtype = VT_8;
        if (opWidth == 16) inst->vtype = VT_16;
        if (opWidth == 32) inst->vtype = VT_32;
        if (opWidth == 64) inst->vtype = VT_64;

        Operand* operands[3] = { &inst->dst, &inst->src, &inst->src2 };
        size_t explicitOperandCount = 0;
        for (size_t j = 0; j < opCount; j++)
        {
            const xed_operand_t* op = xed_inst_operand(xi, j);
            if (xed_operand_operand_visibility(op) == XED_OPVIS_SUPPRESSED)
                continue;

            dbrew_xed_transform_operand(operands[explicitOperandCount], currentInstr->address, xedd, j, op);

            explicitOperandCount++;
            if (explicitOperandCount > 3)
                abort();
        }

        for (size_t j = explicitOperandCount; j < 3; j++)
            operands[j]->type = OT_None;

        inst->form = OF_0 + explicitOperandCount;

        if (debug >= 1)
        {
            char disasBuf[100] = {0};
            xed_format_context(XED_SYNTAX_INTEL, xedd, disasBuf, sizeof(disasBuf), currentInstr->address, NULL, NULL);
            printf("  %16lx: %s (%s)\n", currentInstr->address, disasBuf, instr2string(inst, 0, NULL));
        }

        InstrChain* tmp = currentInstr;
        currentInstr = currentInstr->next;
        free(tmp);
    }

    dbb->addr = address;
    dbb->count = count;
    dbb->instr = dbrewInstrs;
    dbb->fc = NULL;
    dbb->size = -1;

    return dbb;
}
