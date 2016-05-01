//!compile = {cc} -std=c99 -g -o {outfile} {infile} {driver} ../libdbrew.a -I../include -I../include/priv
//!nooutput = 1

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

void test_fill_instruction(Instr*, int*, char*);

int main()
{
    int expectedLength = 0;
    char buffer[15];

    Rewriter* r = dbrew_new();
    dbrew_verbose(r, false, false, false);
    dbrew_set_capture_capacity(r, 1, 1, 15);
    initRewriter(r);

    Instr* instr = newCapInstr(r);
    CBB* cbb = getCaptureBB(r, 0, -1);
    cbb->instr = instr;
    cbb->count++;

    test_fill_instruction(instr, &expectedLength, buffer);

    generate(r, cbb);


    bool fail = false;
    if (expectedLength != instr->len)
    {
        fail = true;
    }
    else
    {
        char* data = (char*) instr->addr;
        for (int i = 0; i < expectedLength; i++)
            if (buffer[i] != data[i]) fail = true;
    }

    if (fail)
    {
        printf("Generated: %s\n", bytes2string(instr, 0, instr->len));
        instr->len = expectedLength;
        instr->addr = (uintptr_t) buffer;
        printf("Expected:  %s\n", bytes2string(instr, 0, instr->len));

        return 1;
    }

    return 0;
}
