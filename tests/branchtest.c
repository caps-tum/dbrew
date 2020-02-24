/**
 * Simple x86_64 emulator/re-generator
 * (c) 2015, Josef Weidendorfer, GPLv2+
 *
 * Test branching
 */

#include <stdio.h>
#include "dbrew.h"

typedef int (*i2_func)(int,int);

__attribute__ ((noinline))
int test1(int a, int b)
{
    if (a == 1) return b;
    return 0;
}

__attribute__ ((noinline))
int test2(int a, int b)
{
    while(a > 0) { b++; a--; }
    return b;
}

__attribute__ ((noinline))
int test3(int a, int b)
{
    int c = a;
    while(a > 0) {
        while(b > 0) {
            c++;
            b--;
        }
        b = c;
        a--;
    }
    return c;
}

__attribute__ ((noinline))
int test4(int a, int b)
{
    // printf("HELLO\n");
    int i = makeDynamic(0);
    for(; i < a; i++)
        b += i;
    return b;
}

// decode captured code from c1 into c2
void emulateCaptureRun(char* t1, char* t2,
                       int p1, uint64_t p2, int sp1, uint64_t sp2,
                       Rewriter* r1, Rewriter* r2)
{
    int res;

    printf("Tracing emulation of %s(%d,%ld) %s:\n", t1, sp1, sp2, t2);
    res = (int)dbrew_emulate_capture(r1, sp1, sp2);
    printf("Result from emulation: %d\n", res);

    printf("Rewritten code (size %d bytes):\n", dbrew_generated_size(r1));
    dbrew_set_function(r2, dbrew_generated_code(r1));
    dbrew_decode_print(r2, dbrew_generated_code(r1), dbrew_generated_size(r1));

    i2_func f = (i2_func) dbrew_generated_code(r1);
    res = f(p1,p2);

    printf("Run rewritten code %s(%d,%ld) = %d\n", t1, p1, p2, res);
}


/*
 * Test different specializations of a given fucntion <f>.
 * <f> must have signature i2_func (2 int parameters, returns int)
 * or i2p_func (int/pointer parameter, int return), given by <use_i2p>
*/
void runTest(char* fname, uint64_t f, int p1, int p2, int sp1)
{
    Rewriter *c1, *c2;
    int res;

    printf(">>> Testing with function %s\n\n", fname);

    c1 = dbrew_new();
    dbrew_verbose(c1, true, true, true);
    c2 = dbrew_new();

    i2_func ff = (i2_func) f;
    res = ff(p1, p2);
    printf("Run native: %s(%d,%d) = %d\n", fname, p1, p2, res);

    dbrew_set_function(c1, f);

    emulateCaptureRun(fname, "unmodified", p1, p2, sp1, p2, c1, c2);

    dbrew_config_reset(c1);
    dbrew_config_staticpar(c1, 0);
    emulateCaptureRun(fname, "p1 fix", p1, p2, sp1, p2, c1, c2);

    dbrew_free(c1);
    dbrew_free(c2);
}


int main()
{
    //runTest("test1", (uint64_t) test1, 1,7, 2);
    //runTest("test2", (uint64_t) test2, 4,7, 3);
    //runTest("test3", (uint64_t) test3, 4,7, 3);
    runTest("test4", (uint64_t) test4, 4,7, 3);
    return 0;
}
