//!compile = {cc} {ccflags} -std=c99 -g -o {outfile} {infile} {driver} ../libdbrew.a -I../include

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "dbrew.h"

int f1(int);

int main()
{
    // Decode the function.
    Rewriter* r = dbrew_new();
    // to get rid of changing addresses, assume gen code to be 200 bytes max
    dbrew_config_function_setname(r, (uintptr_t) f1, "f1");
    dbrew_config_function_setsize(r, (uintptr_t) f1, 200);
    dbrew_decode_print(r, (uintptr_t) f1, 1);

    return 0;
}
