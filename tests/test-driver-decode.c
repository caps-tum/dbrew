//!compile = as -c -o {ofile} {infile} && {cc} {ccflags} -o {outfile} {ofile} {driver} ../libdbrew.a -I../include

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "dbrew.h"
#include "priv/common.h"

int f1(int);
// possible jump target
int f2(int x) { return x; }

int main()
{
    // Decode the function.
    Rewriter* r = dbrew_new();
    // to get rid of changing addresses, assume gen code to be 800 bytes max
    dbrew_config_function_setname(r, (uintptr_t) f1, "f1");
    dbrew_config_function_setsize(r, (uintptr_t) f1, 800);

    DBB* dbb = dbrew_decode(r, (uintptr_t) f1);
    printf("BB f1 (%d instructions):\n", dbb->count);
    dbrew_print_decoded(dbb);

    return 0;
}
