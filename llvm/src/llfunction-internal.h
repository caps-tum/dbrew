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

#ifndef LL_FUNCTION_INTERNAL_H
#define LL_FUNCTION_INTERNAL_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <llvm-c/Core.h>

#include <llfunction.h>

#include <rellume/func.h>
#include <llengine.h>


/**
 * \ingroup LLFunction
 **/
enum LLFunctionKind {
    /**
     * \brief The function is only declared
     **/
    LL_FUNCTION_DECLARATION,
    /**
     * \brief The function is defined from assembly code
     **/
    LL_FUNCTION_DEFINITION,
    /**
     * \brief The function is specialized
     **/
    LL_FUNCTION_SPECIALIZATION,
    /**
     * \brief The function was loaded from a LLVM input file
     **/
    LL_FUNCTION_EXTERNAL,
};

typedef enum LLFunctionKind LLFunctionKind;

/**
 * \ingroup LLFunction
 **/
struct LLFunction {
    /**
     * \brief The name of the function
     **/
    const char* name;

    /**
     * \brief Address of the function
     **/
    uintptr_t address;

    /**
     * \brief The LLVM function value
     **/
    LLVMValueRef llvmFunction;

    /**
     * \brief The kind of the function
     **/
    LLFunctionKind kind;

    LLFunc* func;

    uint64_t noaliasParams;
};

LLFunction* ll_function_new_definition(uintptr_t, LLFunctionConfig*, LLEngine*);

#endif
