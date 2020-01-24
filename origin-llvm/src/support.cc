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

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm-c/ExecutionEngine.h>

#include <support-internal.h>

/**
 * \defgroup DBLLSupport Support
 * \brief Support functions for the LLVM API
 *
 * @{
 **/

/**
 * Construct a JIT execution engine with suitable properties for runtime binary
 * optimization.
 *
 * \private
 *
 * \author Alexis Engelke
 *
 * \param OutJIT The created execution engine
 * \param M The LLVM module
 * \param OutError The error string when an error occurs, to be freed with free
 * \returns Whether an error occured
 **/
extern "C"
LLVMBool
dbll_support_create_mcjit_compiler(LLVMExecutionEngineRef* OutJIT, LLVMModuleRef M, char** OutError)
{
    std::unique_ptr<llvm::Module> Mod(llvm::unwrap(M));
    std::string Error;

    // Mainly kept to set further settings in future.
    llvm::TargetOptions targetOptions;
    targetOptions.EnableFastISel = 0;

    // We use "O3" with a small code model to reduce the code size.
    llvm::EngineBuilder builder(std::move(Mod));
    builder.setEngineKind(llvm::EngineKind::JIT)
           .setErrorStr(&Error)
           .setOptLevel(llvm::CodeGenOpt::Level::Aggressive)
           .setCodeModel(llvm::CodeModel::Model::Small)
           .setRelocationModel(llvm::Reloc::Model::PIC_)
           .setTargetOptions(targetOptions);

    // Same as "-mcpu=native", but disable AVX for the moment.
    llvm::SmallVector<std::string, 1> MAttrs;
    MAttrs.push_back(std::string("-avx"));
    llvm::Triple Triple = llvm::Triple(llvm::sys::getProcessTriple());
    llvm::TargetMachine* Target = builder.selectTarget(Triple, "x86-64", llvm::sys::getHostCPUName(), MAttrs);

    if (llvm::ExecutionEngine *JIT = builder.create(Target)) {
        *OutJIT = llvm::wrap(JIT);
        return 0;
    }
    *OutError = strdup(Error.c_str());
    return 1;
}

/**
 * @}
 **/
