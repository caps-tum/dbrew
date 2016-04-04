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

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#include "dbrew.h"
#include "buffers.h"
#include "instr.h"

#define debug(format, ...) printf("!DBG %s: " format "\n", __PRETTY_FUNCTION__, ##__VA_ARGS__)

typedef struct _CBB CBB;
typedef struct _FunctionConfig FunctionConfig;

// a decoded basic block
struct _DBB {
    uint64_t addr;
    FunctionConfig* fc; // if !=0, the BB is in this function
    int size; // in bytes
    int count;
    Instr* instr; // pointer to first decoded instruction
};

char* prettyAddress(uint64_t a, FunctionConfig* fc);


// a captured basic block
struct _CBB {
    // ID: address of original BB + EmuState at start
    uint64_t dec_addr;
    int esID;

    // if !=0, capturing of instructions in this BB started in this function
    FunctionConfig* fc;

    // instructions captured within this BB
    int count;
    Instr* instr;

    // two possible exits: next on branching or fall-through
    CBB *nextBranch, *nextFallThrough;
    // type of instruction ending this BB
    InstrType endType;
    // a hint for conditional branches whether branching is more likely
    Bool preferBranch;

    // for code generation/relocation
    int size;
    uint64_t addr1, addr2;
    Bool genJcc8, genJump;
};

char* cbb_prettyName(CBB* bb);



#define CC_MAXPARAM     5
#define CC_MAXCALLDEPTH 5

// emulator capture states
typedef enum _CaptureState {
    CS_DEAD = 0,      // uninitialized, should be invalid to access
    CS_DYNAMIC,       // data unknown at code generation time
    CS_STATIC,        // data known at code generation time
    CS_STACKRELATIVE, // address with known offset from stack top at start
    CS_STATIC2,       // same as static + indirection from memory static
    CS_Max
} CaptureState;



//
// Rewriter Configuration
//

struct _FunctionConfig
{
    uint64_t func;
    char* name;

    FunctionConfig* next; // chain
};

typedef struct _CaptureConfig
{
    CaptureState par_state[CC_MAXPARAM];
     // does function to rewrite return floating point?
    Bool hasReturnFP;
    // avoid unrolling at call depths
    Bool force_unknown[CC_MAXCALLDEPTH];
    // all branches forced known
    Bool branches_known;

    // linked list of configurations per function
    FunctionConfig* function_configs;

    /**
     * \brief The backend function which is responsible for code generation
     **/
    RewriterBackendFunc backend;
} CaptureConfig;


FunctionConfig* config_find_function(Rewriter* r, uint64_t f);



//
// Emulation
//

typedef enum _FlagType {
    FT_Carry = 0, FT_Zero, FT_Sign, FT_Overflow, FT_Parity,
    FT_Max
} FlagType;

// a single value with type and capture state
typedef struct _EmuValue {
    uint64_t val;
    ValType type;
    CaptureState state;
} EmuValue;


#define MAX_CALLDEPTH 5

// emulator state. for memory, use the real memory apart from stack

struct _EmuState;
typedef struct _EmuState EmuState;

struct _EmuState {

    // when saving an EmuState, remember root
    EmuState* parent;

    // general registers: Reg_AX .. Reg_R15
    uint64_t reg[Reg_Max];
    CaptureState reg_state[Reg_Max];

    // x86 flags: carry (CF), zero (ZF), sign (SF), overflow (OF), parity (PF)
    // TODO: auxiliary carry
    Bool flag[FT_Max];
    CaptureState flag_state[FT_Max];

    // stack
    int stackSize;
    uint8_t* stack; // real memory backing
    uint64_t stackStart, stackAccessed, stackTop; // virtual stack boundaries
    // capture state of stack
    CaptureState *stackState;

    // own return stack
    uint64_t ret_stack[MAX_CALLDEPTH];
    int depth;

};

struct _Rewriter {

    // decoded instructions
    int decInstrCount, decInstrCapacity;
    Instr* decInstr;

    // decoded basic blocks
    int decBBCount, decBBCapacity;
    DBB* decBB;

    // captured instructions
    int capInstrCount, capInstrCapacity;
    Instr* capInstr;

    // captured basic blocks
    int capBBCount, capBBCapacity;
    CBB* capBB;
    CBB* currentCapBB;

    // function to capture
    uint64_t func;

    // buffer for captured code
    int capCodeCapacity;
    CodeStorage* cs;

    // structs for emulator & capture config
    CaptureConfig* cc;
    EmuState* es;
    // saved emulator states
#define SAVEDSTATE_MAX 20
    int savedStateCount;
    EmuState* savedState[SAVEDSTATE_MAX];

    // stack of unfinished BBs to capture
#define CAPTURESTACK_LEN 20
    int capStackTop;
    CBB* capStack[CAPTURESTACK_LEN];

    uintptr_t generatedCodeAddr;
    uintptr_t generatedCodeSize;

    // for optimization passes
    Bool addInliningHints;
    Bool doCopyPass; // test pass

    // debug output
    Bool showDecoding, showEmuState, showEmuSteps, showOptSteps;
};


// REX prefix, used in parseModRM
#define REX_MASK_B 1
#define REX_MASK_X 2
#define REX_MASK_R 4
#define REX_MASK_W 8

#endif // COMMON_H
