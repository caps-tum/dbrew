/*
 * Example for DBrew API
 *
 * Generic 2d stencil code.
 */

#include <stdio.h>
#include <stdlib.h>
#include "dbrew.h"

typedef struct {
    int xdiff, ydiff;
    double factor;
} StencilPoint;

typedef struct {
    int points;
    StencilPoint p[];
} Stencil;

typedef struct {
    double factor;
    int points;
    StencilPoint* p;
} StencilFactor;

typedef struct {
    int factors;
    StencilFactor f[];
} SortedStencil;

#define CO1 (.4)
#define CO2 (.15)

Stencil s5 = {5,{ { 0, 0, CO1},
                  {-1, 0, CO2},
                  { 1, 0, CO2},
                  { 0,-1, CO2},
                  { 0, 1, CO2} }};

SortedStencil s5s = {2,{ {CO1, 1, &(s5.p[0])},
                         {CO2, 4, &(s5.p[1])} }};


typedef double (*apply_func)(double*, int, Stencil*);

double apply(double *m, int xsize, Stencil* s)
{
    double res;
    int i;

    res = 0;
    for(i=0; i<s->points; i++) {
        StencilPoint* p = s->p + i;
        res += p->factor * m[p->xdiff + p->ydiff * xsize];
    }
    return res;
}

double applyS(double *m, int xsize, SortedStencil* s)
{
    double sum, res;
    int f, i;

    res = 0;
    sum = 0;
    for(f=0; f < s->factors; f++) {
        StencilFactor* sf = s->f + f;
        StencilPoint* p = sf->p;
        sum = m[p->xdiff + p->ydiff * xsize];
        for(i=1; i < sf->points; i++) {
            p = sf->p + i;
            sum += m[p->xdiff + p->ydiff * xsize];
        }
        res += sf->factor * sum;
    }
    return res;
}

double apply2(double *m, int xsize, Stencil* s)
{
    (void)s; // unused
    return CO1 * m[0] + CO2 * (m[-1] + m[1] + m[-xsize] + m[xsize]);
}

double apply3(double *m, int xsize, Stencil* s)
{
    (void)xsize, (void)s; // unused
    return m[0];
}


typedef void (*apply_loop)(int, double*, double*, apply_func, Stencil*);


void applyLoop(int size, double* src, double* dst, apply_func af, Stencil* s)
{
    int x,y;

    for(y=1;y<size-1;y++)
        for(x=1;x<size-1;x++)
            dst[x+y*size] = af(&(src[x+y*size]), size, s);
}


// only top line
void applyLoop1(int size, double* src, double* dst, apply_func af, Stencil* s)
{
    int x;
    for(x=1;x<size-1;x++)
        dst[x+size] = af(&(src[x+size]), size, s);
}

void apply4(int size, double* src, double* dst, apply_func af, Stencil* s)
{
    dst[0] = af(&(src[0]), size, s);
    dst[1] = af(&(src[1]), size, s);
    dst[2] = af(&(src[2]), size, s);
    dst[3] = af(&(src[3]), size, s);
}


int main(int argc, char* argv[])
{
    int i, x, y;
    double *m1, *m2, diff;
    int size = 0, iter = 0, av = 0;
    apply_func af;
    apply_loop al;
    Stencil* s;
    Rewriter* r = 0;
    int rewriteApplyLoop = 0;
    int do4 = 0;
    int verbose = 0;
    int arg = 1;

    while ((argc > arg) && (argv[arg][0] == '-')) {
        if (argv[arg][1] == 'v') verbose++;
        if (argv[arg][2] == 'v') verbose++;
        arg++;
    }
    if (argc > arg) { av   = atoi(argv[arg]); arg++; }
    if (argc > arg) { size = atoi(argv[arg]); arg++; }
    if (argc > arg) { iter = atoi(argv[arg]); arg++; }

    if (size == 0) size = 1002;
    if (iter == 0) iter = 1000;
    if (av == 0) av = 1;

    al = applyLoop;
    if (av>40) {
        do4 = 1;
        al = apply4;
        av -= 40;
    }
    if (av>20) {
        al = applyLoop1;
        av -= 20;
    }
    if (av>10) {
        rewriteApplyLoop = 1;
        av -= 10;
    }

    m1 = (double*) malloc(sizeof(double) * size * size);
    m2 = (double*) malloc(sizeof(double) * size * size);

    // init
    for(i=0;i<size*size;i++)
        m1[i] = 0.0;
    for(i=0;i<size;i++) {
        m1[i] = 1.0;                 // upper row
        m1[(size-1)*size + i] = 2.0; // lower row
        double v = 1.0 + (double)i/size;
        m1[i*size] = v;              // left column
        m1[i*size + (size-1)] = v;   // right column
    }
    for(i=0;i<size*size;i++)
        m2[i] = m1[i];

    printf("Stencil code version: ");
    switch(av) {
    case 1:
    case 5:
        // generic version
        printf("generic");
        af = apply;
        s = &s5;
        break;
    case 2:
    case 6:
        // generic version grouped by factor
        printf("grouped generic");
        af = (apply_func) applyS;
        s = (Stencil*) &s5s;
        break;
    case 3:
    case 7:
        // manual version
        printf("manual");
        af = apply2;
        s = 0;
        break;
    case 4:
    case 8:
        // no computation, just return center value
        printf("(center)");
        af = apply3;
        s = 0;
        break;
    }

    if (rewriteApplyLoop) {
        printf(", rewriting with loops.\n");
        r = dbrew_new();
        if (verbose>1) {
            dbrew_verbose(r, true, true, true);
            dbrew_optverbose(r, true);
            dbrew_config_function_setname(r, (uint64_t) al, "ApplyLoop");
        }
        dbrew_set_function(r, (uint64_t) al);
        dbrew_config_staticpar(r, 0); // size is constant
        dbrew_config_staticpar(r, 3); // apply func is constant
        dbrew_config_staticpar(r, 4); // stencil is constant
        dbrew_config_parcount(r, 5);
        if (!do4)
            dbrew_config_force_unknown(r, 0); // do not unroll in applyLoop
        al = (apply_loop) dbrew_rewrite(r, size, m1, m2, af, s);
    }
    else {
        printf(",%s rewriting.\n", (av<5) ? " no":"");
        if (av >= 5) {
            r = dbrew_new();
            if (verbose>1) {
                dbrew_verbose(r, true, true, true);
                dbrew_optverbose(r, true);
                dbrew_config_function_setname(r, (uint64_t) af, "apply");
            }
            dbrew_set_function(r, (uint64_t) af);
            dbrew_config_staticpar(r, 1); // size is constant
            dbrew_config_staticpar(r, 2); // stencil is constant
            dbrew_config_parcount(r, 3);
            dbrew_config_returnfp(r);
            af = (apply_func) dbrew_rewrite(r, m1 + size + 1, size, s);
        }
    }

    if (r && (verbose>0)) {
        // use another rewriter to show generated code
        Rewriter* r2 = dbrew_new();
        uint64_t genfunc = dbrew_generated_code(r);
        int gensize = dbrew_generated_size(r);
        //dbrew_config_function_setname(r2, genfunc, "gen");
        dbrew_config_function_setsize(r2, genfunc, gensize);
        dbrew_decode_print(r2, genfunc, gensize);
        dbrew_free(r2);
    }

    printf("Width %d, matrix size %d, %d iterations, apply V %d\n",
           size, (int)(size*size*sizeof(double)), iter, av);
    iter = iter/2;

#if 1
    if (do4) {
        for(i=0;i<iter;i++) {
            for(y=1;y<size-1;y++)
                for(x=1;x<size-1;x+=4) {
                    int o = x+y*size;
                    al(size, m1+o, m2+o, af, s);
                }
            for(y=1;y<size-1;y++)
                for(x=1;x<size-1;x+=4) {
                    int o = x+y*size;
                    al(size, m2+o, m1+o, af, s);
                }
        }
    }
    else {
        for(i=0;i<iter;i++) {
            al(size, m1, m2, af, s);
            al(size, m2, m1, af, s);
        }
    }
#else
    // this version should allow vectorization
    for(i=0;i<iter;i++) {
        for(y=1;y<size-1;y++)
            for(x=1;x<size-1;x++)
                m1[x+y*size] = apply2(&(m2[x+y*size]), size, s);
        for(y=1;y<size-1;y++)
            for(x=1;x<size-1;x++)
                m2[x+y*size] = apply2(&(m1[x+y*size]), size, s);
    }
#endif

    diff = 0.0;
    for(y=1;y<size-1;y++) {
        for(x=1;x<size-1;x++) {
            double d = m2[x+y*size] - af(&(m1[x+y*size]), size, s);
            diff += (d>0) ? d : -d;
        }
    }
    printf("Residuum after %d iterations: %.8f\n", 2*iter, diff);

    free(m1);
    free(m2);
    if (r) dbrew_free(r);
}
