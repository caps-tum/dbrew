//!compile = as -c -o {ofile} {infile} && {cc} {ccflags} -o {outfile} {ofile} {driver} ../libdbrew.a -I../include -I../include/priv

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "dbrew.h"
#include "emulate.h"
#include "generate.h"
#include "error.h"

int f1(int);

int main(int argc, char* argv[])
{
    Rewriter* r = dbrew_new();
    if ((argc>1) && (strcmp(argv[1],"-v")==0))
        dbrew_verbose(r, true, true, true);
    dbrew_config_function_setname(r, (uintptr_t) f1, "f1");
    dbrew_config_function_setsize(r, (uintptr_t) f1, 800);

    DBB* dbb = dbrew_decode(r, (uint64_t) f1); // decode
    CBB* cbb = createCBBfromDBB(r, dbb);       // capture all instructions
    Error* e = (Error*) generate(r, cbb);
    if (e)
        logError(e, (char*) "Stopped");
    else {
        // print generated instructions
        dbrew_config_function_setname(r, (uintptr_t) cbb->addr1, "f1gen");
        dbrew_config_function_setsize(r, (uintptr_t) cbb->addr1, 800);
        dbrew_decode_print(r, (uintptr_t) cbb->addr1, cbb->size);
    }
    return 0;
}
