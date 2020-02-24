//!driver = test-driver-generate.c
#include <priv/instr.h>

void test_fill_instruction(Instr*);

void
test_fill_instruction(Instr* instr)
{
    initSimpleInstr(instr, IT_Invalid);
}
