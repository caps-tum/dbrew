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

typedef struct _MemRangeConfig MemRangeConfig;
typedef struct _FunctionConfig FunctionConfig;
typedef struct _CaptureConfig CaptureConfig;

// a decoded basic block
struct _DBB {
    uint64_t addr;
    FunctionConfig* fc; // if !=0, the BB is in this function
    int size; // in bytes
    int count; // number of instructions
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

    // for DBrew's own code generation backend
    int size;
    uint64_t addr1, addr2;
    bool genJcc8, genJump;

    // allow to store CBB-specific data for other backends (eg. via LLVM JIT)
    void* generatorData;
};

char* cbb_prettyName(CBB* bb);

#define CC_MAXREGPAR    6
#define CC_MAXPARAM     16
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

typedef enum _MemRangeType {
    MR_Unknown = 0,    // unspecified, wildcard when searching for range
    MR_Invalid,        // accesses not allowed
    MR_ConstantData,   // accessable, initialized with constant data
    MR_MutableData,    // accessable, writable
    MR_Function,       // accessable, compiled code
} MemRangeType;

struct _MemRangeConfig
{
    MemRangeType type;
    char* name;
    MemRangeConfig* next; // chain to next config
    CaptureConfig* cc; // capture config this belongs to
    uint64_t start;
    int size;
};

// Function parameter configuration
typedef enum _ParType {
    ParDisabled = 0,
    ParInteger,
    ParFloat,
    ParOther,
} ParType;

typedef struct _ParMap {
    ParType parType : 2;
    bool isReturn   : 1;
    bool isStatic   : 1;
} __attribute__((packed)) ParMap;

// extension of MemRangeConfig
struct _FunctionConfig
{
    // 1st 6 entries have to be same as MemRangeConfig
    MemRangeType type;
    char* name;
    MemRangeConfig* next; // chain to next config
    CaptureConfig* cc; // capture config this belongs to
    uint64_t start;
    int size;

    // Function config specific parameters below.

    // number of parameters passed to function to rewrite
    int parCount;
    // specialise for some parameters to be constant?
    MetaState par_state[CC_MAXPARAM];
    // for debug: allow parameters to be named
    char* par_name[CC_MAXPARAM];

    int flags;
    // Bypass emulation and call function directly?
    // Set return value as static if emulation is bypassed

    // Store function parameter maps
    ParMap parMap[CC_MAXPARAM];
};

struct _CaptureConfig
{

    FunctionConfig* next; // chain

    // does function to rewrite return floating point?
    bool hasReturnFP;
    // avoid unrolling at call depths
    bool force_unknown[CC_MAXCALLDEPTH];
    // all branches forced known
    bool branches_known;

    // linked list of memory range and function configurations
    MemRangeConfig* range_configs;

};


// vectorization parameter config for a Rewriter

typedef enum _VectorizeReq {
    VR_None = 0,
    VR_DoubleX2_RV,  // scalar double => 2x double vector, ret + par1
    VR_DoubleX2_RVV, // scalar double => 2x double vector, ret + par1 + par2
    VR_DoubleX2_RP,  // scalar double => 2x double vector, ret + par1 pointer
    VR_DoubleX4_RV,  // scalar double => 4x double vector, ret + par1
    VR_DoubleX4_RVV, // scalar double => 4x double vector, ret + par1 + par2
    VR_DoubleX4_RP   // scalar double => 4x double vector, ret + par1 pointer
} VectorizeReq;


FunctionConfig* config_find_function(Rewriter* r, uint64_t f);
FunctionConfig* config_get_function(Rewriter* r, uint64_t f);
InstrType config_lookup_intrinsic(FunctionConfig* cc);
void config_free(Rewriter* r);

//
// Emulation
//

// TODO: replace with RI_xxx flag register type indexes
typedef enum _FlagType {
    FT_Carry = 0, FT_Zero, FT_Sign, FT_Overflow, FT_Parity,
    FT_Max
} FlagType;

typedef enum _FlagSet {
    FS_None     = 0,
    FS_Carry    = 1,
    FS_Zero     = 2,
    FS_Sign     = 4,
    FS_Overflow = 8,
    FS_Parity   = 16
} FlagSet;

#define FS_CZSOP (FS_Carry|FS_Zero|FS_Sign|FS_Overflow|FS_Parity)
#define FS_CO    (FS_Carry|FS_Overflow)
#define FS_ZSP   (FS_Zero|FS_Sign|FS_Parity)

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

    // general purpose registers: RAX - R15
    uint64_t reg[RI_GPMax];
    MetaState reg_state[RI_GPMax];

    // instruction pointer
    uint64_t regIP;
    MetaState regIP_state;

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

    // Inhibibt loop unrolling
    bool inhibitLoopUnroll;

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
    FunctionConfig* entry_func;

    // buffer for generated binary code
    int capCodeCapacity;
    CodeStorage* cs;
    uint64_t generatedCodeAddr;
    int generatedCodeSize;

    // vectorization config
    VectorizeReq vreq;
    int vectorsize;

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

    // Don't rewrite call instructions to avoid impossible immediates
    bool keepLargeCallAddrs;

    // printer config
    bool printBytes;

    // list of related rewriters
    Rewriter* next;
};


// REX prefix, used in parseModRM
#define REX_MASK_B 1
#define REX_MASK_X 2
#define REX_MASK_R 4
#define REX_MASK_W 8

#endif // COMMON_H
