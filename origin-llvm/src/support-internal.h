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

#ifndef DBLL_SUPPORT_H
#define DBLL_SUPPORT_H

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#ifdef __cplusplus
extern "C" {
#endif

LLVMBool dbll_support_create_mcjit_compiler(LLVMExecutionEngineRef*, LLVMModuleRef, char**);

#ifdef __cplusplus
}
#endif

#endif
