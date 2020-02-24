//!compile = as -c -o {ofile} {infile} && {cc} {ccflags} -o {outfile} {ofile} {driver} ../libdbrew.a -I../include

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "dbrew.h"

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
    dbrew_decode_print(r, (uintptr_t) f1, 1);

    return 0;
}
