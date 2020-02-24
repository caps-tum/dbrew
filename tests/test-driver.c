//!compile = {cc} {ccflags} -c -o {ofile} {infile} && {cc} {ccflags} -o {outfile} {ofile} {driver} ../libdbrew.a -I../include

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "dbrew.h"

typedef long (*f1_t)(long, long);
long f1(long, long);

// data which may be read/written by test
const uint64_t rdata[2] = {1,2}; // read-only data section (16 bytes)
long wdata[2];                   // uninitialized data section (16 bytes)

int runtest(Rewriter*r, long parameter, bool doRun, bool showBytes)
{
    f1_t ff;

    if (parameter >= 0)
        printf(">>> Testcase known par = %ld.\n", parameter);
    else
        printf(">>> Testcase unknown par = %ld.\n", -parameter);


    dbrew_set_function(r, (uint64_t) f1);
    dbrew_config_parcount(r, 2);
    // to get rid of changing addresses, assume f1 code to be 100 bytes max
    dbrew_config_function_setname(r, (uint64_t) f1, "test");
    dbrew_config_function_setsize(r, (uint64_t) f1, 100);
    dbrew_config_set_memrange(r, "rdata", false, (uint64_t) rdata, 16);
    dbrew_config_set_memrange(r, "wdata", true, (uint64_t) wdata, 16);
    if (parameter >= 0)
        dbrew_config_staticpar(r, 0);
    else
        parameter = -parameter; // make positive
    ff = (f1_t) dbrew_rewrite(r, parameter, 1);

    // print the generated function.
    Rewriter* r2 = dbrew_new();
    dbrew_printer_showbytes(r2, showBytes);
    dbrew_config_function_setname(r2, (uint64_t) ff, "gen");
    dbrew_config_function_setsize(r2, (uint64_t) ff, dbrew_generated_size(r));
    dbrew_config_set_memrange(r2, "rdata", false, (uint64_t) rdata, 16);
    dbrew_config_set_memrange(r2, "wdata", true, (uint64_t) wdata, 16);
    dbrew_decode_print(r2, (uint64_t) ff, dbrew_generated_size(r));

    if (!doRun) return 0;

    // Ensure that the program actually works.
    long orig = f1(parameter, 1);
    long rewritten = ff(parameter, 1);

    printf(">>> Run orig/rewritten: %ld/%ld\n", orig, rewritten);
    return (orig != rewritten) ? 1 : 0;
}

int main(int argc, char** argv)
{
    int arg = 1;
    int res = 0;
    bool debug = false;
    bool run = false;
    bool var = false; // also generate version with variable parameter?
    bool showBytes = true;
    while((arg<argc) && (argv[arg][0] == '-') && (argv[arg][1] == '-')) {
        if (strcmp(argv[arg], "--debug")==0) debug = true;
        if (strcmp(argv[arg], "--run")==0) run = true;
        if (strcmp(argv[arg], "--var")==0) var = true;
        if (strcmp(argv[arg], "--nobytes")==0) showBytes = false;
        arg++;
    }

    Rewriter* r = dbrew_new();
    // Only output DBB and new function but not intermediate steps as the stack
    // pointer is different on each run. However, for debugging we want this
    // information.
    dbrew_verbose(r, true, debug ? true : false, true);
    dbrew_printer_showbytes(r, showBytes);
    dbrew_optverbose(r, false);

    if (var)
        res += runtest(r, -1, run, showBytes);

    if (arg < argc) {
        // take parameter values for rewriting from command line
        for(; arg < argc; arg++)
            res += runtest(r, atoi(argv[arg]), run, showBytes);
    }
    else {
        // default parameter "1"
        res += runtest(r, 1, run, showBytes);
    }

    return res;
}
