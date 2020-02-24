/**
 * Simple x86_64 emulator/re-generator
 * (c) 2015, Josef Weidendorfer, GPLv2+
 */

#include <stdio.h>
#include "dbrew.h"

typedef int (*i2_func)(int,int);
typedef int (*i2p_func)(int,int*);

// Test 1

/* Compiled with -O0:
 *   55         push  %rbp
 *   48 89 e5   mov   %rsp,%rbp
 *   89 7d ec   mov   %edi,-0x14(%rbp)
 *   89 75 e8   mov   %esi,-0x18(%rbp)
 *   8b 55 ec   mov   -0x14(%rbp),%edx
 *   8b 45 e8   mov   -0x18(%rbp),%eax
 *   01 d0      add   %edx,%eax
 *   89 45 fc   mov   %eax,-0x4(%rbp)
 *   8b 45 fc   mov   -0x4(%rbp),%eax
 *   5d         pop   %rbp
 *   c3         ret
*/
__attribute__ ((noinline))
int test1(int a, int b)
{
    int res = a + b;
    return res;
}

// Test 2
int test2(int a, int b)
{
    int res = test1(a, b) + b;
    return res;
}

// Test 3
int test3(int a, int b)
{
    int sum = 0;
    while(a>0) {
        sum += b;
        a--;
    }
    return sum;
}

// Test 4
int a4[4] = {1,2,3,4};
int b4[4] = {5,6,7,8};
int test4(int a, int* b)
{
    return b[a];
}

// decode captured code from c1 into c2
void emulateCaptureRun(char* t1, char* t2, bool use_i2p,
                       int p1, uint64_t p2, int sp1, uint64_t sp2,
                       Rewriter* r1, Rewriter* r2)
{
    int res;

    printf("Tracing emulation of %s(%d,%ld) %s:\n", t1, sp1, sp2, t2);
    res = (int)dbrew_emulate_capture(r1, sp1, sp2);
    printf("Result from emulation: %d\n", res);

    printf("Rewritten code (size %d bytes):\n", dbrew_generated_size(r1));
    dbrew_set_function(r2, dbrew_generated_code(r1));
    dbrew_verbose(r2, false, false, false);
    dbrew_print_decoded( dbrew_decode(r2, dbrew_generated_code(r1)) );
    dbrew_verbose(r2, true, true, true);

    if (use_i2p) {
        i2p_func f = (i2p_func) dbrew_generated_code(r1);
        res = f(p1, (int*) p2);
    }
    else {
        i2_func f = (i2_func) dbrew_generated_code(r1);
        res = f(p1, p2);
    }
    printf("Run rewritten code %s(%d,%ld) = %d\n", t1, p1, p2, res);
}


/*
 * Test different specializations of a given fucntion <f>.
 * <f> must have signature i2_func (2 int parameters, returns int)
 * or i2p_func (int/pointer parameter, int return), given by <use_i2p>
*/
void runTest(char* fname, uint64_t f, bool use_i2p,
             int p1, uint64_t p2, int sp1, uint64_t sp2,
             int runOrig, int runSpec1, int runSpec2)
{
    Rewriter *c1, *c2, *c3;
    int res;

    printf(">>> Testing with function %s\n\n", fname);

    c1 = dbrew_new();
    c2 = dbrew_new();
    c3 = dbrew_new();

    dbrew_verbose(c1, true, true, true);
    dbrew_verbose(c2, true, true, true);

    if (use_i2p) {
        i2p_func ff = (i2p_func) f;
        res = ff(p1, (int*) p2);
    }
    else {
        i2_func ff = (i2_func) f;
        res = ff(p1, p2);
    }
    printf("Run native: %s(%d,%ld) = %d\n", fname, p1, p2, res);

    dbrew_set_function(c1, f);

    if (runOrig)
        emulateCaptureRun(fname, "unmodified", use_i2p, p1,p2,sp1,sp2, c1, c2);

    if (runSpec1) {
        // Specialize for p1
        dbrew_config_reset(c1);
        dbrew_config_staticpar(c1, 0);
        emulateCaptureRun(fname, "p1 fix", use_i2p, p1,p2,sp1,sp2, c1, c2);

        // Nesting
        dbrew_config_reset(c2);
        dbrew_config_staticpar(c2, 1);
        emulateCaptureRun(fname, "nested + p2 fix", use_i2p, p1,p2,sp1,sp2, c2, c3);
    }

    if (runSpec2) {
        // Specialize for p2
        dbrew_config_reset(c1);
        dbrew_config_staticpar(c1, 1);
        emulateCaptureRun(fname, "p2 fix", use_i2p, p1,p2,sp1,sp2, c1, c2);

        // Nesting
        dbrew_config_reset(c2);
        dbrew_config_staticpar(c2, 0);
        emulateCaptureRun(fname, "nested + p1 fix", use_i2p, p1,p2,sp1,sp2, c2, c3);
    }

    // Specialize Par 1 and 2
    dbrew_config_reset(c1);
    dbrew_config_staticpar(c1, 0);
    dbrew_config_staticpar(c1, 1);
    emulateCaptureRun(fname, "p1+p2 fix", use_i2p, p1,p2,sp1,sp2, c1, c2);

    dbrew_free(c1);
    dbrew_free(c2);
    dbrew_free(c3);
}


int main()
{
    runTest("test1", (uint64_t) test1, false, 4,7, 1,2, 1,1,1);
    runTest("test2", (uint64_t) test2, false, 4,7, 1,2, 1,1,1);
    // FIXME: test 3 has a loop depending on par1 and cannot be
    //        rewritten without fixing par1 for now
    runTest("test3", (uint64_t) test3, false, 4,7, 3,5, 0,1,0);
    runTest("test4", (uint64_t) test4, true,
            1, (uint64_t) a4, 3, (uint64_t) b4, 1, 1, 1);
    return 0;
}
