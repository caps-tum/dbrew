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

/*
 * Header file defining the DBrew API.
 *
 * This library allows functions to be rewritten at runtime in specified
 * ways, that is, rewriting happens on the binary level. Rewritten functions
 * are called in exact the same way as the original function.
 *
 * Use cases:
 * - partial evaluation/specialization at rewriting time.
 *   This allows to capture values of some variables during rewriting,
 *   such that each rewriting creates a special version of a generic function.
 * - observe/insert/modify properties of binary code.
 *   - observe reads/write accesses to variables/data structures
 *   - insert function calls before/after operations
 *   - modify existing function calls/addresses of accesses
 *
 * To allow rewriting configurations to be specified on the binary level in
 * an architecture-independent way, we use the calling convention of the ABI.
 * That is, each configuration is bound to function pointers and properties of
 * parameters and return values of functions.
 *
 * Configuration for functions (providing function pointer):
 * - keep function call, allow inlining, or remove call completely?
 * - replace by provided wrapper function?
 * - add callback on read/write accesses to variables/memory?
 * - error out when detecting unknown control path?
 *
 * Configuration for function parameters / return value:
 * - value to be captured as being constant?
 *   (for pointers: all data accessable through pointer captured)
 * - constant value to be assumed as being variable?
 * - annotate value with name for later reference
 *
 * There are callbacks called at rewriting time (observers) and to be
 * called at execution time of the rewritten function (may be inlined).
 * All callbacks can be configured to receive named values as parameters
 * for context information. Function and memory address replacement works
 * by the callback returning the information to be replaced.
 *
 * In observers, rewritten code can be requested as byte sequence.
 * Any named values used in the code is given with offsets into the sequence.
 * This can be used as macro functionality for writing fast generators from
 * code templates, provided by the rewriter.
 *
 * Simple already defined functions are provided for convenience:
 * - markInt(int, flags): returned value gets marked with meta info
 * - tagInt(int, char*):  returned value gets marked with tag
 *
 * Meta states (attached to values stored in registers/tracked memory)
 * - constant: rewrite code to specialize for the known value/condition
 * - unknown: "downgrade" a known value to be unknown for rewriting
 *            This forbids loop unrolling with known loop variable
 * - recursively known: for values same as known, dereferecing keeps state
 * - tracking pointer: request to track meta state of values dereferenced
 *            through this pointer by maintaining difference to base address.
 *            Example: on function entry, the stack pointer defaults to this
 *            meta state, using its current value as base. This allows to track
 *            the meta state for values on stack
 * - expected: when used next time, check for expected value and create
 *            new path with guard, setting value known. Multiple times allowed
 * - tracking value: maintain set of value tags this value depends on
 *
 * Configuration for calling observing hooks during rewriting:
 * - pass tagged values as parameters
 * - use return value as callback function to be inserted?
 *
 * Callback types:
 * - at memory read: address is parameter, address can be replaced
 * - at memory write: address/value is parameter, write may be suppressed
 * - at value usage: value, operations are parameters
 * - at operation: operand values are parameters
 *
 * See example usage in examples/
 */

#ifndef DBREW_H
#define DBREW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*void_func)(void);

// function which can be used in code to be rewritten
// mark a passed-through value as dynamic
uint64_t makeDynamic(uint64_t v);
// mark a passed-through value as static
uint64_t makeStatic(uint64_t v);

// opaque data structures used in interface
typedef struct _Rewriter Rewriter;
typedef struct _DBB DBB;
typedef struct _CBB CBB;
typedef struct _Instr Instr;

// allocate space for a given number of decoded instructions
Rewriter* dbrew_new(void);

// free rewriter resources
void dbrew_free(Rewriter*);

// configure size of internal buffer space of a rewriter
void dbrew_set_decoding_capacity(Rewriter* r,
                                 int instrCapacity, int bbCapacity);
void dbrew_set_capture_capacity(Rewriter* r,
                                int instrCapacity, int bbCapacity,
                                int codeCapacity);

// set function to rewrite
// this clears any previously decoded/captured instructions
void dbrew_set_function(Rewriter* rewriter, uint64_t f);

// set rewriter activities to be verbose or quiet
void dbrew_verbose(Rewriter* rewriter,
                   bool decode, bool emuState, bool emuSteps);
void dbrew_optverbose(Rewriter* r, bool v);

// config for printing instruction: show also machine code bytes?
void dbrew_printer_showbytes(Rewriter* r, bool v);

// decode a piece of x86 binary code starting add address <f>
DBB* dbrew_decode(Rewriter* r, uint64_t f);

// decode and print <count> instructions starting add address <f>
void dbrew_decode_print(Rewriter* r, uint64_t f, int count);

// print instructions from a decoded basic block
void dbrew_print_decoded(DBB* bb, bool printBytes);

// initialize emulator, use given stack size
void dbrew_set_stacksize(Rewriter *c, int stacksize);

// emulate the given decoded binary code
// initialize state with function parameters ('...')
uint64_t dbrew_emulate_capture(Rewriter* r, ...);

// buffer with regenerated code, captured from emulation
uint64_t dbrew_generated_code(Rewriter* r);
int dbrew_generated_size(Rewriter* r);

// configure rewriter
void dbrew_config_reset(Rewriter* r);
void dbrew_config_staticpar(Rewriter* r, int staticParPos);
void dbrew_config_returnfp(Rewriter* r);
void dbrew_config_parcount(Rewriter* r, int parCount);
// assume all calculated results to be unknown at call depth lower <depth>
void dbrew_config_force_unknown(Rewriter* r, int depth);
// assume all branches to be fixed according to rewriter input parameters
void dbrew_config_branches_known(Rewriter* r, bool);
// provide a name for a function (for debug)
void dbrew_config_function_setname(Rewriter* r, uint64_t f, const char* name);
// provide a code length in bytes for a function (for debugging)
void dbrew_config_function_setsize(Rewriter* r, uint64_t f, int len);
// provide a name for a parameter of the function to rewrite (for debug)
void dbrew_config_par_setname(Rewriter* c, int par, char* name);
// register a valid memory range with permission and name (for debug)
void dbrew_config_set_memrange(Rewriter* r, char* name, bool isWritable,
                               uint64_t start, int size);

// convenience functions, using default rewriter
void dbrew_def_verbose(bool decode, bool emuState, bool emuSteps);

// Act as drop-in replacement assuming the function is returning an integer
uint64_t dbrew_emulate(Rewriter* r, ...);

// rewrite configured function, return pointer to rewritten code
uint64_t dbrew_rewrite(Rewriter* r, ...);

// rewrite <f> using default config, return pointer to rewritten code
uint64_t dbrew_rewrite_func(uint64_t f, ...);



// Vector API:
// functions provided by DBrew with known semantics, known to rewriter

typedef double (*dbrew_func_R8V8_t)(double);
typedef double (*dbrew_func_R8V8V8_t)(double, double);
typedef double (*dbrew_func_R8P8_t)(double*);

// Configuration for expansion requests.
// <s> is size in bytes of vector registers to use; default is 16.
// May be set to 32 for AVX.
// Returns actual value used; this can differ from requested.
int dbrew_set_vectorsize(Rewriter *r, int s);

// 4x call f (signature double => double) and map to input/output vector iv/ov
void dbrew_apply4_R8V8(dbrew_func_R8V8_t f, double* ov, double* iv);
// 4x call f (signature double,double => double) and map to vectors i1v,i2v,ov
void dbrew_apply4_R8V8V8(dbrew_func_R8V8V8_t f,
                         double* ov, double* i1v, double* i2v);
// 4x call f (signature double* => double), map to input array pointers/output vector
void dbrew_apply4_R8P8(dbrew_func_R8P8_t f, double* ov, double* iv);

#ifdef __cplusplus
}
#endif

#endif // DBREW_H
