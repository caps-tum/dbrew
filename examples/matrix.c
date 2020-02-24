/*
 * Example for DBrew API
 *
 * Analysis of matrix multiplication kernel.
 */

#include <stdio.h>
#include <stdlib.h>
#include "dbrew.h"

typedef void (*mm_t)(int s,
                     double a[][s], double b[][s], double c[][s],
                     int i, int j, int k);

void mm_kernel(int s,
               double a[][s], double b[][s], double c[][s],
               int i, int j, int k)
{
    a[i][k] += b[i][j] * c[j][k];
}

void init(int s, double m[][s], double v)
{
    int i, j;

    for(i=0; i<s; i++)
        for(j=0; j<s; j++)
            m[i][j] = v;
}

double sum(int s, double m[][s])
{
    int i, j;
    int sum = 0.0;

    for(i=0; i<s; i++)
        for(j=0; j<s; j++)
            sum += m[i][j];

    return sum;
}

int main(int argc, char* argv[])
{
    int i, j, k;
    double* a, *b, *c;
    mm_t mmf;
    Rewriter* r = 0;
    int s = 0;
    int verbose = 0;
    int arg = 1;

    while ((argc > arg) && (argv[arg][0] == '-')) {
        if (argv[arg][1] == 'v') verbose++;
        if (argv[arg][2] == 'v') verbose++;
        arg++;
    }
    if (argc > arg) { s = atoi(argv[arg]); arg++; }
    if (s == 0) s = 102;

    a = (double*) malloc(sizeof(double) * s * s);
    b = (double*) malloc(sizeof(double) * s * s);
    c = (double*) malloc(sizeof(double) * s * s);

    init(s, (double(*)[s]) a, 0.0);
    init(s, (double(*)[s]) b, 2.0);
    init(s, (double(*)[s]) c, 3.0);

    for(i=0; i<s; i++)
        for(j=0; j<s; j++)
            for(k=0; k<s; k++)
                mm_kernel(s,
                          (double(*)[s]) a, (double(*)[s]) b, (double(*)[s]) c,
                          i, j, k);

    printf("Sum: %f\n", sum(s, (double(*)[s]) a));

    // rewrite kernel for analysis
    r = dbrew_new();
    if (verbose>1) {
        dbrew_verbose(r, true, true, true);
        dbrew_optverbose(r, true);
        dbrew_config_function_setname(r, (uint64_t) mm_kernel, "mm");
    }
    dbrew_set_function(r, (uint64_t) mm_kernel);
    dbrew_config_staticpar(r, 0); // size is constant
    dbrew_config_parcount(r, 7);
    mmf = (mm_t) dbrew_rewrite(r, s, a, b, c, 0, 0, 0);

    if (verbose > 0) {
        // use another rewriter to show generated code
        Rewriter* r2 = dbrew_new();
        uint64_t genfunc = dbrew_generated_code(r);
        int gensize = dbrew_generated_size(r);
        dbrew_config_function_setname(r2, genfunc, "gmm");
        dbrew_config_function_setsize(r2, genfunc, gensize);
        dbrew_decode_print(r2, genfunc, gensize);
        dbrew_free(r2);
    }

    init(s, (double(*)[s]) a, 0.0);
    for(i=0; i<s; i++)
        for(j=0; j<s; j++)
            for(k=0; k<s; k++)
                mmf(s, (double(*)[s]) a, (double(*)[s]) b, (double(*)[s]) c,
                    i, j, k);

    printf("Sum: %f\n", sum(s, (double(*)[s]) a));

    return 0;
}
