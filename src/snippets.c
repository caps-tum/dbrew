/**
 * This file is part of DBrew, the dynamic binary rewriting library.
 *
 * (c) 2016, Josef Weidendorfer <josef.weidendorfer@gmx.de>
 *
 * DBrew is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * DBrew is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DBrew.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "dbrew.h"
#include "vector.h"

#include <assert.h>
#include <immintrin.h>

/*
 * Functions provided by DBrew for users to call
 *
 * The semantics of these functions are clearly defined and known
 * to DBrew. This way, they can be used to provide custom meta data
 * to help rewriting, and DBrew can arbitrarly replace their
 * instructions, e.g. make a NOP instruction out of it or substitue
 * with other variants.
 *
 * This file always should be compiled with optimizations flags ("-O2 -mavx"),
 * as the functions may end up being embedded into rewritten code.
 */


// mark a passed-through value as dynamic

__attribute__ ((noinline))
uint64_t makeDynamic(uint64_t v)
{
    return v;
}

// mark a passed-through value as static

__attribute__ ((noinline))
uint64_t makeStatic(uint64_t v)
{
    return v;
}



/* Vector API:
 * The rewriter will try to generated vectorized variants from a given
 * function. Further, it will replace the functions of the vector API
 * with generated code calling the vectorized variants.
 */

// 4x call f (signature double => double) and map to input/output vector iv/ov
__attribute__ ((noinline))
void dbrew_apply4_R8V8(dbrew_func_R8V8_t f, double* ov, double* iv)
{
    ov[0] = (f)(iv[0]);
    ov[1] = (f)(iv[1]);
    ov[2] = (f)(iv[2]);
    ov[3] = (f)(iv[3]);
}

// 4x call f (signature double,double => double) and map to vectors i1v,i2v,ov
__attribute__ ((noinline))
void dbrew_apply4_R8V8V8(dbrew_func_R8V8V8_t f,
                         double* ov, double* i1v, double* i2v)
{
    ov[0] = (f)(i1v[0], i2v[0]);
    ov[1] = (f)(i1v[1], i2v[1]);
    ov[2] = (f)(i1v[2], i2v[2]);
    ov[3] = (f)(i1v[3], i2v[3]);
}

// 4x call f (signature *double => double) and map to successive pointers/vector
// R8P8:
// "8-byte return value, parameter 1 is pointer to 8-byte element"
__attribute__ ((noinline))
void dbrew_apply4_R8P8(dbrew_func_R8P8_t f,
                       double* ov, double* iv)
{
    ov[0] = (f)(iv + 0);
    ov[1] = (f)(iv + 1);
    ov[2] = (f)(iv + 2);
    ov[3] = (f)(iv + 3);
}


//
// replacement functions
//

// for dbrew_apply4_R8V8

typedef __m128d (*dbrew_func_R8V8_X2_t)(__m128d);
void apply4_R8V8_X2(uint64_t f, double* ov, double* iv)
{
    // ov[0] = (f)(iv[0]);
    // ov[1] = (f)(iv[1]);
    // ov[2] = (f)(iv[2]);
    // ov[3] = (f)(iv[3]);
    dbrew_func_R8V8_X2_t vf = (dbrew_func_R8V8_X2_t) f;
    ((__m128d*)ov)[0] = (*vf)( ((__m128d*)iv)[0] );
    ((__m128d*)ov)[1] = (*vf)( ((__m128d*)iv)[1] );
}

#ifdef __AVX__
typedef __m256d (*dbrew_func_R8V8_X4_t)(__m256d);
void apply4_R8V8_X4(uint64_t f, double* ov, double* iv)
{
    dbrew_func_R8V8_X4_t vf = (dbrew_func_R8V8_X4_t) f;
#if 0
    // only works with 32-byte aligned iv/ov (TODO: provide both variants)
    ((__m256d*)ov)[0] = (*vf)( ((__m256d*)iv)[0] );
#else
    // use intrinsics for generation of instructions coping with unalignedness
    __m256d i = _mm256_loadu_pd(iv);
    __m256d o = (*vf)(i);
    _mm256_storeu_pd(ov, o);
#endif
}
#endif // __AVX__


// for dbrew_apply4_R8V8V8

typedef __m128d (*dbrew_func_R8V8V8_X2_t)(__m128d,__m128d);
void apply4_R8V8V8_X2(uint64_t f, double* ov, double* i1v, double* i2v)
{
    // ov[0] = (f)(i1v[0], i2v[0]);
    // ov[1] = (f)(i1v[1], i2v[1]);
    // ov[2] = (f)(i1v[2], i2v[2]);
    // ov[3] = (f)(i1v[3], i2v[3]);
    dbrew_func_R8V8V8_X2_t vf = (dbrew_func_R8V8V8_X2_t) f;
    ((__m128d*)ov)[0] = (*vf)( ((__m128d*)i1v)[0], ((__m128d*)i2v)[0] );
    ((__m128d*)ov)[1] = (*vf)( ((__m128d*)i1v)[1], ((__m128d*)i2v)[1] );
}

#ifdef __AVX__
typedef __m256d (*dbrew_func_R8V8V8_X4_t)(__m256d,__m256d);
void apply4_R8V8V8_X4(uint64_t f, double* ov, double* i1v, double* i2v)
{
    dbrew_func_R8V8V8_X4_t vf = (dbrew_func_R8V8V8_X4_t) f;
#if 0
    // only works with 32-byte aligned iv/ov (TODO: provide both variants)
    ((__m256d*)ov)[0] = (*vf)( ((__m256d*)i1v)[0], ((__m256d*)i2v)[0] );
#else
    // use intrinsics for generation of instructions coping with unalignedness
    __m256d i1 = _mm256_loadu_pd(i1v);
    __m256d i2 = _mm256_loadu_pd(i2v);
    __m256d o = (*vf)(i1, i2);
    _mm256_storeu_pd(ov, o);
#endif
}
#endif // __AVX__


// for dbrew_apply4_R8P8

typedef __m128d (*dbrew_func_R8P8_X2_t)(__m128d*);
void apply4_R8P8_X2(uint64_t f, double* ov, double* iv)
{
    // ov[0] = (f)(iv + 0);
    // ov[1] = (f)(iv + 1);
    // ov[2] = (f)(iv + 2);
    // ov[3] = (f)(iv + 3);
    dbrew_func_R8P8_X2_t vf = (dbrew_func_R8P8_X2_t) f;
    ((__m128d*)ov)[0] = (*vf)( ((__m128d*)iv) + 0 );
    ((__m128d*)ov)[1] = (*vf)( ((__m128d*)iv) + 1 );
}

#ifdef __AVX__
typedef __m256d (*dbrew_func_R8P8_X4_t)(__m256d*);
void apply4_R8P8_X4(uint64_t f, double* ov, double* iv)
{
    dbrew_func_R8P8_X4_t vf = (dbrew_func_R8P8_X4_t) f;
#if 0
    // only works with 32-byte aligned iv/ov (TODO: provide both variants)
    ((__m256d*)ov)[0] = (*vf)( ((__m256d*)iv) );
#else
    // use intrinsics for generation of instructions coping with unalignedness
    __m256d o = (*vf)( ((__m256d*)iv) );
    _mm256_storeu_pd(ov, o);
#endif
}
#endif // __AVX__



// helper functions

// used to restrict configuration of expansion factor
int maxVectorBytes(void)
{
#ifdef __AVX__
    return 32;
#else
    return 16; // SSE
#endif
}

uint64_t expandedVectorVariant(uint64_t f, int s, VectorizeReq* vr)
{
    if (s == 16) {
        if (f == (uint64_t)dbrew_apply4_R8V8) {
            *vr = VR_DoubleX2_RV;
            return (uint64_t) apply4_R8V8_X2;
        }
        else if (f == (uint64_t)dbrew_apply4_R8V8V8) {
            *vr = VR_DoubleX2_RVV;
            return (uint64_t) apply4_R8V8V8_X2;
        }
        else if (f == (uint64_t)dbrew_apply4_R8P8) {
            *vr = VR_DoubleX2_RP;
            return (uint64_t) apply4_R8P8_X2;
        }
    }
#ifdef __AVX__
    else if (s == 32) {
        if (f == (uint64_t)dbrew_apply4_R8V8) {
            *vr = VR_DoubleX4_RV;
            return (uint64_t) apply4_R8V8_X4;
        }
        else if (f == (uint64_t)dbrew_apply4_R8V8V8) {
            *vr = VR_DoubleX4_RVV;
            return (uint64_t) apply4_R8V8V8_X4;
        }
        else if (f == (uint64_t)dbrew_apply4_R8P8) {
            *vr = VR_DoubleX4_RP;
            return (uint64_t) apply4_R8P8_X4;
        }
    }
#endif
    assert(0);
    return 0;
}
