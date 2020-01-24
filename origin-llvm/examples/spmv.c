
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <dbrew.h>
#include <dbrew-backend.h>
#include <../benchmark/timer.h>

// Include generated test matrix, provides rawMatrix and rawVector.
#include "spmv-matrix.gen"

struct Entry {
    size_t a;
    size_t b;
};

typedef struct Entry Entry;

struct SpMatrix {
    size_t width;
    size_t height;
    // First HEIGHT entries consists of start/end index in entries array.
    // Others are in order (!) tuples of column/value.
    // Lookup therefore can be done in O(width).
    Entry entries[];
};

typedef struct SpMatrix SpMatrix;


static
void
spmv_c(SpMatrix* matrix, double* restrict v, double* restrict r)
{
    for (size_t j = 0; j < matrix->height; j++)
    {
        double sum = 0;
        Entry* rowEntry = &matrix->entries[j];
        for (size_t i = rowEntry->a; i < rowEntry->b; i++)
        {
            Entry* entry = &matrix->entries[i];
            double* d = (double*) &entry->b;
            sum += v[entry->a] * *d;
        }
        r[j] = sum;
    }
}

#define ASM_BLOCK(...) __asm__(".intel_syntax noprefix; " #__VA_ARGS__ "; .att_syntax")

void spmv_asm(SpMatrix*, double*, double*);
ASM_BLOCK(
.globl spmv_asm;
.align 0x10;
spmv_asm:
    mov r9, [rdi + 8]; // r9 = rowcount
    xor r8, r8; // r8 = current row
    lea rax, [r9 * 2];
    test r9, r9;
    lea rcx, [rdi + 16]; // rcx = ptr to current row entry
    lea rdi, [rdi + rax * 8 + 16]; // rdi = ptr to current value entry
    jz .Lspmv_asm_2;
.align 0x10;
.Lspmv_asm_1:
    mov rax, [rcx + 8];
    xorpd xmm0, xmm0;
    sub rax, [rcx]; // rax = number of value entries for current row
    jz .Lspmv_asm_3;
.align 0x10;
.Lspmv_asm_4:
    mov r10, [rdi];
    add rdi, 16;
    movsd xmm2, [rsi + 8 * r10];
    mulsd xmm2, [rdi - 8];
    sub rax, 1;
    addsd xmm0, xmm2;
    jnz .Lspmv_asm_4;
.Lspmv_asm_3:
    movsd [rdx + 8 * r8], xmm0;
    add rcx, 16;
    add r8, 1;
    cmp r8, r9;
    jne .Lspmv_asm_1;
.Lspmv_asm_2:
    ret;
);

int
main(void)
{
    JTimer timer;

    Rewriter* r = dbrew_new();
    dbrew_set_capture_capacity(r, 1000000, 1, 1000000);
    dbrew_set_decoding_capacity(r, 10000, 100);
    dbrew_set_function(r, (uint64_t) spmv_asm);
    dbrew_config_parcount(r, 3);
    dbrew_config_staticpar(r, 0);
    dbrew_optverbose(r, false);
    dbrew_verbose(r, false, false, false);

    SpMatrix* mat = (SpMatrix*) rawMatrix;
    double* v = (double*) rawVector;
    double* rv1 = calloc(sizeof(double), mat->height);
    double* rv2 = calloc(sizeof(double), mat->height);
    double* rv3 = calloc(sizeof(double), mat->height);

    uint8_t* (*spmv_dbrew)(SpMatrix*, double*, double*) = (void*) dbrew_rewrite(r, mat, v, rv1);
    uint8_t* (*spmv_dbrew_llvm)(SpMatrix*, double*, double*) = (void*) dbrew_llvm_rewrite(r, mat, v, rv1);

    // Original
    JTimerInit(&timer);
    JTimerCont(&timer);
    for (int i = 0; i < 10000000; i++) spmv_c(mat, v, rv1);
    JTimerStop(&timer);
    printf("Time: %lf secs\n", JTimerRead(&timer));
    for (size_t i = 0; i < mat->height; i++) printf(" %lf", rv1[i]);
    printf("\n");

    // ASM
    JTimerInit(&timer);
    JTimerCont(&timer);
    for (int i = 0; i < 10000000; i++) spmv_asm(mat, v, rv1);
    JTimerStop(&timer);
    printf("Time: %lf secs\n", JTimerRead(&timer));
    for (size_t i = 0; i < mat->height; i++) printf(" %lf", rv1[i]);
    printf("\n");

    // DBrew
    JTimerInit(&timer);
    JTimerCont(&timer);
    for (int i = 0; i < 10000000; i++) spmv_dbrew(mat, v, rv3);
    JTimerStop(&timer);
    printf("Time: %lf secs\n", JTimerRead(&timer));
    for (size_t i = 0; i < mat->height; i++) printf(" %lf", rv3[i]);
    printf("\n");

    // DBrew+LLVM
    JTimerInit(&timer);
    JTimerCont(&timer);
    for (int i = 0; i < 10000000; i++) spmv_dbrew_llvm(mat, v, rv3);
    JTimerStop(&timer);
    printf("Time: %lf secs\n", JTimerRead(&timer));
    for (size_t i = 0; i < mat->height; i++) printf(" %lf", rv3[i]);
    printf("\n");

    return 0;
}
