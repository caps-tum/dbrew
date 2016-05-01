//!driver = test-driver-generate.c

#include <priv/instr.h>

void test_fill_instruction(Instr*, int*, char*);

void
test_fill_instruction(Instr* instr, int* length, char* buffer)
{
    initSimpleInstr(instr, IT_RET);

    *length = 1;
    buffer[0] = 0xc3;
}
