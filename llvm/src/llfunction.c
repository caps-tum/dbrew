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

/**
 * \file
 **/

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Support.h>

#include <rellume/rellume.h>

#include <llfunction.h>
#include <llfunction-internal.h>

#include <llengine.h>
#include <llengine-internal.h>
#include <support-internal.h>

/**
 * \defgroup LLFunction Function
 * \brief Representation of a function
 *
 * @{
 **/

static LLVMAttributeRef
ll_support_get_enum_attr(LLVMContextRef context, const char* name)
{
    unsigned id = LLVMGetEnumAttributeKindForName(name, strlen(name));
    return LLVMCreateEnumAttribute(context, id, 0);
}

/**
 * Helper function to allocate a function.
 *
 * \private
 *
 * \author Alexis Engelke
 *
 * \param state The module state
 * \returns The declared function
 **/
static LLFunction*
ll_function_new(LLEngine* state)
{
    LLFunction* function;

    function = malloc(sizeof(LLFunction));
    function->func = NULL;

    return function;
}

static LLVMTypeRef
ll_function_unpack_type(uint64_t packedType, uint64_t* out_noalias_params, LLEngine* state)
{
    LLVMTypeRef i8 = LLVMInt8TypeInContext(state->context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(state->context);

    uint64_t tmp = packedType;
    uint64_t noaliasParams = 0;

    size_t paramCount = (size_t) (tmp & 07);
    tmp = tmp >> 3;

    LLVMTypeRef types[paramCount + 1];

    for (size_t i = 0; i < paramCount + 1; i++)
    {
        int rawType = (int) (tmp & 07);
        LLVMTypeRef type = NULL;

        switch (rawType)
        {
            case 0:
                type = LLVMPointerType(i8, 0);
                break;
            // Void for return type
            case 1:
                if (i == 0)
                    type = LLVMVoidTypeInContext(state->context);
                else
                {
                    type = LLVMPointerType(i8, 0);
                    noaliasParams = noaliasParams | (1 << (i - 1));
                }
                break;
            case 2:
                type = i64;
                break;
            case 6:
                type = LLVMFloatTypeInContext(state->context);
                break;
            case 7:
                type = LLVMDoubleTypeInContext(state->context);
                break;
            default:
                warn_if_reached();
        }

        types[i] = type;
        tmp = tmp >> 3;
    }

    if (out_noalias_params)
        *out_noalias_params = noaliasParams;

    return LLVMFunctionType(types[0], types + 1, paramCount, false);
}

static void
ll_function_apply_noalias(LLVMValueRef llvmFn, uint64_t noaliasParams, LLEngine* state)
{
    uint32_t paramCount = LLVMCountParams(llvmFn);
    LLVMAttributeRef noaliasAttr = ll_support_get_enum_attr(state->context, "noalias");
    for (size_t i = 0; i < paramCount; i++)
    {
        if (noaliasParams & (1 << i))
            LLVMAddAttributeAtIndex(llvmFn, i + 1, noaliasAttr);
    }
}

/**
 * Define a new function at the given address and configuration. After this
 * call, the function consists only of a prologue. Basic blocks can be added
 * with #ll_function_add_basic_block, the final IR can be built with
 * #ll_function_build_ir.
 *
 * \private
 *
 * \author Alexis Engelke
 *
 * \param address The address of the function
 * \param config The function configuration
 * \param state The module state
 * \returns The defined function
 **/
LLFunction*
ll_function_new_definition(LLFunctionConfig* config, LLEngine* state)
{
    LLFunction* function = ll_function_new(state);
    function->name = config->name;
    function->cfg = ll_config_new();
    ll_config_enable_fast_math(function->cfg, config->fastMath);
    ll_config_set_global_base(function->cfg, 0x1000, state->globalBase);
    function->func = ll_func_new(state->module, function->cfg);
    return function;
}

LLFunction*
ll_decode_function(uintptr_t address, LLFunctionConfig* config, LLEngine* state)
{
    LLFunction* function = ll_function_new_definition(config, state);

    // First, lift the function to LLVM-IR, but without the SysV calling
    // convention.
    ll_func_decode(function->func, address);
    LLVMValueRef llvm_fn_raw = ll_func_lift(function->func);

    // Then, apply the SysV convention wrapping the lifted function.
    uint64_t noaliasParams;
    LLVMTypeRef fnty = ll_function_unpack_type(config->signature, &noaliasParams, state);
    function->llvmFunction = ll_func_wrap_sysv(llvm_fn_raw, fnty, state->module, config->stackSize);
    ll_function_apply_noalias(function->llvmFunction, noaliasParams, state);

    // Delete original function, we no longer need that.
    LLVMDeleteFunction(llvm_fn_raw);

    return function;
}

/**
 * Specialize a function by inlining the base function into a new wrapper
 * function and fixing a parameter. If the length of the value is larger than
 * zero, the memory region starting at value will be fixed, too.
 * The base function will be deleted.
 *
 * \author Alexis Engelke
 *
 * \param base The base function
 * \param index The index of the parameter
 * \param value The value of the parameter
 * \param length The length of the memory region, or zero
 * \param state The module state
 * \returns The specialized function
 **/
LLFunction*
ll_function_specialize(LLFunction* base, uintptr_t index, uintptr_t value, size_t length, LLEngine* state)
{
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(state->context);

    LLFunction* function = ll_function_new(state);
    function->name = base->name;

    LLVMTypeRef fnType = LLVMGetElementType(LLVMTypeOf(base->llvmFunction));
    size_t paramCount = LLVMCountParamTypes(fnType);

    LLVMTypeRef paramTypes[paramCount];
    LLVMGetParamTypes(fnType, paramTypes);

    if (index >= paramCount)
        warn_if_reached();

    LLVMTypeRef i64 = LLVMInt64TypeInContext(state->context);
    function->llvmFunction = LLVMAddFunction(state->module, function->name, fnType);

    LLVMValueRef params = LLVMGetFirstParam(function->llvmFunction);
    LLVMValueRef baseParams = LLVMGetFirstParam(base->llvmFunction);
    LLVMValueRef args[paramCount];

    LLVMValueRef fixed;

    // Last check is for sanity, 1 MiB should be enough
    if (length != 0 && length < 0x100000)
    {
        size_t elem_count = (length + 7) / 8;
        LLVMTypeRef arrayType = LLVMArrayType(i64, elem_count);
        LLVMValueRef qwords[elem_count];

        uint64_t* data = (uint64_t*) value;
        for (size_t i = 0; i < elem_count; i++)
            qwords[i] = LLVMConstInt(i64, data[i], false);

        LLVMValueRef global = LLVMAddGlobal(state->module, arrayType, "globalParam0");
        LLVMSetGlobalConstant(global, true);
        LLVMSetLinkage(global, LLVMPrivateLinkage);
        LLVMSetInitializer(global, LLVMConstArray(arrayType, qwords, elem_count));

        fixed = LLVMBuildPointerCast(builder, global, paramTypes[index], "");
    }
    else
    {
        if (LLVMGetTypeKind(paramTypes[index]) == LLVMPointerTypeKind)
            fixed = LLVMConstIntToPtr(LLVMConstInt(i64, value, false), paramTypes[index]);
        else
            fixed = LLVMConstBitCast(LLVMConstInt(i64, value, false), paramTypes[index]);
    }

    for (uintptr_t i = 0; i < paramCount; i++)
    {
        if (i == index)
            args[i] = fixed;
        else
            args[i] = params;

        // TODO: Copy parameter attributes
        // if (LLVMGetAttribute(baseParams) != 0)
        //     LLVMAddAttribute(params, LLVMGetAttribute(baseParams));

        params = LLVMGetNextParam(params);
        baseParams = LLVMGetNextParam(baseParams);
    }

    LLVMBasicBlockRef llvmBB = LLVMAppendBasicBlock(function->llvmFunction, "");
    LLVMPositionBuilderAtEnd(builder, llvmBB);

    LLVMValueRef retValue = LLVMBuildCall(builder, base->llvmFunction, args, paramCount, "");

    if (LLVMGetTypeKind(LLVMGetReturnType(fnType)) != LLVMVoidTypeKind)
        LLVMBuildRet(builder, retValue);
    else
        LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);

    // Inline base function directly...
    dbll_support_inline_function(retValue);
    // ... and delete it, to reduce compilation time.
    LLVMDeleteFunction(base->llvmFunction);

    return function;
}

/**
 * Dispose a function.
 *
 * \author Alexis Engelke
 *
 * \param function The function
 **/
void
ll_function_dispose(LLFunction* function)
{
    if (function->func != NULL)
    {
        ll_config_free(function->cfg);
        ll_func_dispose(function->func);
    }

    free(function);
}

/**
 * Compile a function after generating the IR.
 *
 * \author Alexis Engelke
 *
 * \param function The function
 * \param state The module state
 * \returns A pointer to the function
 **/
void*
ll_function_get_pointer(LLFunction* function, LLEngine* state)
{
    return LLVMGetPointerToGlobal(state->engine, function->llvmFunction);
}

/**
 * @}
 **/
