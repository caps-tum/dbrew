//!compile = {cc} {ccflags} -std=c99 -g -o {outfile} {infile} {driver} ../libdbrew.a -I../include -I../include/priv

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "dbrew.h"
#include "emulate.h"
#include "engine.h"
#include "generate.h"
#include "instr.h"
#include "printer.h"

void test_fill_instruction(Instr*);

int main()
{
    Rewriter* r = dbrew_new();

    // Construct a test-CBB of one instruction. This CBB does not have a
    // terminator and is not runnable, we only want to ensure that the produced
    // instruction is correct.

    initRewriter(r);
    Instr* instr = newCapInstr(r);
    CBB* cbb = getCaptureBB(r, 0, -1);
    cbb->instr = instr;
    cbb->count++;

    test_fill_instruction(instr);

    generate(r, cbb);

    printf("Instruction: %s\n", instr2string(instr, 0, cbb->fc));
    printf("Generated:  %s\n", bytes2string(instr, 0, instr->len));

    return 0;
}
