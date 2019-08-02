
#include <assert.h>
#include <stdint.h>

#include <llvm-c/Core.h>

#include <common.h>
#include <engine.h>
#include <printer.h>

#include <rellume/rellume.h>
#include <rellume/instr.h>

#include <dbrew-backend.h>

#include <llengine.h>
#include <llengine-internal.h>
#include <llfunction.h>
#include <llfunction-internal.h>


static LLInstrType _llinst_lut[IT_Max] = {
    [IT_HINT_CALL] = LL_INS_NOP,
    [IT_HINT_RET] = LL_INS_NOP,
    [IT_NOP] = LL_INS_NOP,
    [IT_CALL] = LL_INS_CALL,
    [IT_RET] = LL_INS_RET,
    [IT_PUSH] = LL_INS_PUSH,
    [IT_PUSHFQ] = LL_INS_PUSHFQ,
    [IT_POP] = LL_INS_POP,
    [IT_LEAVE] = LL_INS_LEAVE,
    [IT_MOV] = LL_INS_MOV,
    [IT_MOVZX] = LL_INS_MOVZX,
    [IT_MOVSX] = LL_INS_MOVSX,
    [IT_ADD] = LL_INS_ADD,
    [IT_SUB] = LL_INS_SUB,
    [IT_CMP] = LL_INS_CMP,
    [IT_LEA] = LL_INS_LEA,
    [IT_NOT] = LL_INS_NOT,
    [IT_NEG] = LL_INS_NEG,
    [IT_INC] = LL_INS_INC,
    [IT_DEC] = LL_INS_DEC,
    [IT_AND] = LL_INS_AND,
    [IT_OR] = LL_INS_OR,
    [IT_XOR] = LL_INS_XOR,
    [IT_TEST] = LL_INS_TEST,
    [IT_IMUL] = LL_INS_IMUL,
    [IT_MUL] = LL_INS_MUL,
    [IT_SHL] = LL_INS_SHL,
    [IT_SHR] = LL_INS_SHR,
    [IT_SAR] = LL_INS_SAR,
    [IT_CLTQ] = LL_INS_CLTQ,
    [IT_CMOVO] = LL_INS_CMOVO,
    [IT_CMOVNO] = LL_INS_CMOVNO,
    [IT_CMOVC] = LL_INS_CMOVC,
    [IT_CMOVNC] = LL_INS_CMOVNC,
    [IT_CMOVZ] = LL_INS_CMOVZ,
    [IT_CMOVNZ] = LL_INS_CMOVNZ,
    [IT_CMOVBE] = LL_INS_CMOVBE,
    [IT_CMOVA] = LL_INS_CMOVA,
    [IT_CMOVS] = LL_INS_CMOVS,
    [IT_CMOVNS] = LL_INS_CMOVNS,
    [IT_CMOVP] = LL_INS_CMOVP,
    [IT_CMOVNP] = LL_INS_CMOVNP,
    [IT_CMOVL] = LL_INS_CMOVL,
    [IT_CMOVGE] = LL_INS_CMOVGE,
    [IT_CMOVLE] = LL_INS_CMOVLE,
    [IT_CMOVG] = LL_INS_CMOVG,
    [IT_SETO] = LL_INS_SETO,
    [IT_SETNO] = LL_INS_SETNO,
    [IT_SETC] = LL_INS_SETC,
    [IT_SETNC] = LL_INS_SETNC,
    [IT_SETZ] = LL_INS_SETZ,
    [IT_SETNZ] = LL_INS_SETNZ,
    [IT_SETBE] = LL_INS_SETBE,
    [IT_SETA] = LL_INS_SETA,
    [IT_SETS] = LL_INS_SETS,
    [IT_SETNS] = LL_INS_SETNS,
    [IT_SETP] = LL_INS_SETP,
    [IT_SETNP] = LL_INS_SETNP,
    [IT_SETL] = LL_INS_SETL,
    [IT_SETGE] = LL_INS_SETGE,
    [IT_SETLE] = LL_INS_SETLE,
    [IT_SETG] = LL_INS_SETG,
    [IT_MOVD] = LL_INS_MOVD,
    [IT_MOVQ] = LL_INS_MOVQ,
    [IT_MOVSS] = LL_INS_MOVSS,
    [IT_MOVSD] = LL_INS_MOVSD,
    [IT_MOVUPS] = LL_INS_MOVUPS,
    [IT_MOVUPD] = LL_INS_MOVUPD,
    [IT_MOVAPS] = LL_INS_MOVAPS,
    [IT_MOVAPD] = LL_INS_MOVAPD,
    [IT_MOVDQU] = LL_INS_MOVDQU,
    [IT_MOVDQA] = LL_INS_MOVDQA,
    [IT_MOVLPS] = LL_INS_MOVLPS,
    [IT_MOVLPD] = LL_INS_MOVLPD,
    [IT_MOVHPS] = LL_INS_MOVHPS,
    [IT_MOVHPD] = LL_INS_MOVHPD,
    [IT_UNPCKLPS] = LL_INS_UNPCKLPS,
    [IT_UNPCKLPD] = LL_INS_UNPCKLPD,
    [IT_ADDSS] = LL_INS_ADDSS,
    [IT_ADDSD] = LL_INS_ADDSD,
    [IT_ADDPS] = LL_INS_ADDPS,
    [IT_ADDPD] = LL_INS_ADDPD,
    [IT_SUBSS] = LL_INS_SUBSS,
    [IT_SUBSD] = LL_INS_SUBSD,
    [IT_SUBPS] = LL_INS_SUBPS,
    [IT_SUBPD] = LL_INS_SUBPD,
    [IT_MULSS] = LL_INS_MULSS,
    [IT_MULSD] = LL_INS_MULSD,
    [IT_MULPS] = LL_INS_MULPS,
    [IT_MULPD] = LL_INS_MULPD,
    [IT_DIVSS] = LL_INS_DIVSS,
    [IT_DIVSD] = LL_INS_DIVSD,
    [IT_DIVPS] = LL_INS_DIVPS,
    [IT_DIVPD] = LL_INS_DIVPD,
    [IT_ORPS] = LL_INS_ORPS,
    [IT_ORPD] = LL_INS_ORPD,
    [IT_ANDPS] = LL_INS_ANDPS,
    [IT_ANDPD] = LL_INS_ANDPD,
    [IT_XORPS] = LL_INS_XORPS,
    [IT_XORPD] = LL_INS_XORPD,
    [IT_PXOR] = LL_INS_PXOR,
    [IT_JMP] = LL_INS_JMP,
    [IT_JO] = LL_INS_JO,
    [IT_JNO] = LL_INS_JNO,
    [IT_JC] = LL_INS_JC,
    [IT_JNC] = LL_INS_JNC,
    [IT_JZ] = LL_INS_JZ,
    [IT_JNZ] = LL_INS_JNZ,
    [IT_JBE] = LL_INS_JBE,
    [IT_JA] = LL_INS_JA,
    [IT_JS] = LL_INS_JS,
    [IT_JNS] = LL_INS_JNS,
    [IT_JP] = LL_INS_JP,
    [IT_JNP] = LL_INS_JNP,
    [IT_JL] = LL_INS_JL,
    [IT_JGE] = LL_INS_JGE,
    [IT_JLE] = LL_INS_JLE,
    [IT_JG] = LL_INS_JG,
};

static LLReg
convert_reg(Reg reg)
{
    int rt;
    switch (reg.rt) {
        case RT_None: rt = LL_RT_None; break;
        case RT_GP8Leg: rt = LL_RT_GP8Leg; break;
        case RT_GP8: rt = LL_RT_GP8; break;
        case RT_GP16: rt = LL_RT_GP16; break;
        case RT_GP32: rt = LL_RT_GP32; break;
        case RT_GP64: rt = LL_RT_GP64; break;
        case RT_IP: rt = LL_RT_IP; break;
        case RT_X87: rt = LL_RT_X87; break;
        case RT_MMX: rt = LL_RT_MMX; break;
        case RT_XMM: rt = LL_RT_XMM; break;
        case RT_YMM: rt = LL_RT_YMM; break;
        case RT_ZMM: rt = LL_RT_ZMM; break;
        default: printf("%d %d\n", reg.rt, reg.ri); assert(0);
    }
    int idx = reg.ri == RI_None ? LL_RI_None : reg.ri;
    return ((LLReg) {rt, idx});
}

static void
convert_operand(Operand* dbo, LLInstrOp* llo)
{
    llo->val = dbo->val;
    llo->type = LL_OP_NONE;
    if (opIsReg(dbo))
        llo->type = LL_OP_REG;
    if (opIsImm(dbo))
        llo->type = LL_OP_IMM;
    if (opIsInd(dbo))
        llo->type = LL_OP_MEM;
    llo->scale = dbo->scale;
    if (llo->type == LL_OP_REG || llo->type == LL_OP_MEM)
        llo->reg = convert_reg(dbo->reg);
    if (llo->type == LL_OP_MEM && llo->scale != 0)
        llo->ireg = convert_reg(dbo->ireg);
    llo->seg = LL_RI_None; // TODO!
    llo->size = opTypeWidth(dbo) / 8;
}

static void
lldbrew_convert_instr(void* dbi_, LLInstr* lli)
{
    Instr* dbi = dbi_;
    printf("Converting %s\n", instr2string(dbi, 0, NULL));
    lli->type = _llinst_lut[dbi->type];
    lli->addr = dbi->addr;
    lli->len = dbi->len;
    switch (dbi->form) {
        case OF_0: lli->operand_count = 0; break;
        case OF_1: lli->operand_count = 1; break;
        case OF_2: lli->operand_count = 2; break;
        case OF_3: lli->operand_count = 3; break;
        default: assert(0);
    }

    if (lli->operand_count >= 1)
        convert_operand(&dbi->dst, &lli->ops[0]);
    if (lli->operand_count >= 2)
        convert_operand(&dbi->src, &lli->ops[1]);
    if (lli->operand_count >= 3)
        convert_operand(&dbi->src2, &lli->ops[2]);
}

static void
lldbrew_convert_cbb(void* cbb_, LLInstr* lli)
{
    CBB* cbb = cbb_;
    assert(instrIsJcc(cbb->endType) || cbb->endType == IT_RET);
    lli->type = _llinst_lut[cbb->endType];
    // For Jcc, we need to tweak addresses.
    if (instrIsJcc(cbb->endType))
    {
        lli->operand_count = 1;
        lli->addr = (size_t) cbb->nextFallThrough->generatorData - 1;
        lli->len = 1;
        size_t next_branch = (size_t) cbb->nextBranch->generatorData;
        convert_operand(getImmOp(VT_64, next_branch), &lli->ops[0]);
    }
}

/**
 * Code generation back-end for DBrew.
 *
 * \author Alexis Engelke
 *
 * \param rewriter The DBrew rewriter
 **/
void
dbrew_llvm_backend(Rewriter* rewriter)
{
    LLEngine* state = ll_engine_init();

    LLFunctionConfig config = {
        .stackSize = 128,
        .signature = 026, // 6 pointer params, returns i64
        .name = "__dbrew__"
    };

    LLFunction* function = ll_function_new_definition(&config, state);

    LLInstr instr;
    for (int i = 0; i < rewriter->capBBCount; i++)
        rewriter->capBB[i].generatorData = (void*) ((size_t) i + (1ul<<48));

    for (int i = 0; i < rewriter->capBBCount; i++)
    {
        CBB* cbb = rewriter->capBB + i;
        uint64_t block_addr = (size_t) cbb->generatorData;

        for (int j = 0; j < cbb->count; j++)
        {
            lldbrew_convert_instr(cbb->instr + j, &instr);
            ll_func_add_inst(function->func, block_addr, &instr);
        }
        lldbrew_convert_cbb(cbb, &instr);
        if (instr.type != LL_INS_RET)
            ll_func_add_inst(function->func, block_addr, &instr);
    }

    LLVMTypeRef pi8 = LLVMPointerType(LLVMInt8TypeInContext(state->context), 0);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(state->context);
    LLVMTypeRef types[6] = {pi8, pi8, pi8, pi8, pi8, pi8};
    LLVMTypeRef fn_ty = LLVMFunctionType(i64, types, 6, false);

    function->llvmFunction = ll_func_wrap_sysv(ll_func_lift(function->func), fn_ty, state->module, 128);

    if (function->llvmFunction == NULL)
    {
        warn_if_reached();
        rewriter->generatedCodeAddr = 0;

        return;
    }

    ll_engine_optimize(state, 3);

    if (rewriter->showOptSteps)
        ll_engine_dump(state);

    rewriter->generatedCodeAddr = (uintptr_t) ll_function_get_pointer(function, state);
    rewriter->generatedCodeSize = 0;
}

/**
 * Rewrite a function using DBrew and the LLVM optimization and code generation.
 *
 * \author Alexis Engelke
 *
 * \param r The DBrew rewriter
 **/
uintptr_t
dbrew_llvm_rewrite(Rewriter* r, ...)
{
    va_list argptr;

    va_start(argptr, r);
    vEmulateAndCapture(r, argptr);
    va_end(argptr);

    // runOptsOnCaptured(r);
    dbrew_llvm_backend(r);

    return r->generatedCodeAddr;
}
