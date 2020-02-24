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
    RContext c;
    c.e = 0;
    c.r = dbrew_new();
    initRewriter(c.r);

    // Construct a test-CBB of one instruction. This CBB does not have a
    // terminator and is not runnable, we only want to ensure that the produced
    // instruction is correct.
    CBB* cbb = getCaptureBB(&c, 0, -1);
    Instr* instr = newCapInstr(&c);
    if ((cbb != 0) && (instr != 0)) {
        cbb->instr = instr;
        cbb->count++;
        test_fill_instruction(instr);
        c.e = (Error*) generate(c.r, cbb);
    }
    if (c.e)
        logError(c.e, (char*) "Stopped");
    else {
        printf("Instruction: %s\n", instr2string(instr, 0, cbb->fc));
        printf("Generated:  %s\n", bytes2string(instr, 0, instr->len));
    }
    return 0;
}
