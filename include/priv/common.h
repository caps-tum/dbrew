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

#include "dbrew.h"
#include "buffers.h"
#include "expr.h"
#include "instr.h"

#include <stdint.h>

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
    bool preferBranch;

    // for code generation/relocation
    int size;
    uint64_t addr1, addr2;
    bool genJcc8, genJump;
};

char* cbb_prettyName(CBB* bb);



#define CC_MAXPARAM     6
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

// includes capture state and analysis information for values stored
// in registers or on (private) stack
typedef struct _MetaState {
    CaptureState cState;
    ExprNode* range;  // constrains for dynamic value
    ExprNode* parDep; // analysis: dependency from input parameters
} MetaState;

void initMetaState(MetaState* ms, CaptureState cs);


//
// Rewriter Configuration
//

struct _FunctionConfig
{
    uint64_t func;
    int size;
    char* name;

    FunctionConfig* next; // chain
};

typedef struct _CaptureConfig
{
    // specialise for some parameters to be constant?
    MetaState par_state[CC_MAXPARAM];
    // for debug: allow parameters to be named
    char* par_name[CC_MAXPARAM];

     // does function to rewrite return floating point?
    bool hasReturnFP;
    // avoid unrolling at call depths
    bool force_unknown[CC_MAXCALLDEPTH];
    // all branches forced known
    bool branches_known;

    // linked list of configurations per function
    FunctionConfig* function_configs;

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
    MetaState state;
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
    MetaState reg_state[Reg_Max];

    // x86 flags: carry (CF), zero (ZF), sign (SF), overflow (OF), parity (PF)
    // TODO: auxiliary carry
    bool flag[FT_Max];
    MetaState flag_state[FT_Max];

    // stack
    int stackSize;
    uint8_t* stack; // real memory backing
    uint64_t stackStart, stackAccessed, stackTop; // virtual stack boundaries
    // capture state of stack
    MetaState *stackState;

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

    // expressions for analysis
    ExprPool * ePool;

    // function to capture
    uint64_t func;

    // buffer for generated binary code
    int capCodeCapacity;
    CodeStorage* cs;
    uint64_t generatedCodeAddr;
    int generatedCodeSize;

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

    // capture order
#define GENORDER_MAX 20
    int genOrderCount;
    CBB* genOrder[GENORDER_MAX];

    // for optimization passes
    bool addInliningHints;
    bool doCopyPass; // test pass

    // debug output
    bool showDecoding, showEmuState, showEmuSteps, showOptSteps;
};


// REX prefix, used in parseModRM
#define REX_MASK_B 1
#define REX_MASK_X 2
#define REX_MASK_R 4
#define REX_MASK_W 8

#endif // COMMON_H
