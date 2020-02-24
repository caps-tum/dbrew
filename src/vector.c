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

#include "vector.h"

#include <assert.h>
#include <stdio.h>

#include "dbrew.h"
#include "instr.h"
#include "emulate.h"

/*
 *  Support for DBrew vector API
 *
 * - generate vectorized variants of a given function
 * - substitute vector API code with own version
 */


// returns function pointer to rewritten, vectorized variant
static
uint64_t convertToVector(Rewriter* r, uint64_t func, VectorizeReq vreq)
{
    // already done before?
    Rewriter* rr;
    for(rr = r->next; rr != 0; rr = rr->next) {
        if ((rr->vreq == vreq) && (rr->func == func)) {
            assert(rr->generatedCodeAddr != 0);
            return rr->generatedCodeAddr;
        }
    }

    rr = dbrew_new();
    // add to related rewriter list of <r>
    rr->next = r->next;
    r->next = rr;

    if (r->showEmuSteps) {
        dbrew_verbose(rr, true, true, true);
        printf("Generating vectorized variant of %lx for %d-byte vectors\n",
               func, r->vectorsize);
    }
    dbrew_set_function(rr, func);
    rr->vreq = vreq;

    bool hasVReturn = false;
    int pCount = 0;
    switch(vreq) {
    case VR_DoubleX2_RV:  pCount = 1; hasVReturn = true; break;
    case VR_DoubleX2_RVV: pCount = 2; hasVReturn = true; break;
    case VR_DoubleX2_RP:  pCount = 1; hasVReturn = true; break;
    case VR_DoubleX4_RV:  pCount = 1; hasVReturn = true; break;
    case VR_DoubleX4_RVV: pCount = 2; hasVReturn = true; break;
    case VR_DoubleX4_RP:  pCount = 1; hasVReturn = true; break;
    default: assert(0);
    }
    if (hasVReturn)
        dbrew_config_returnfp(rr);
    dbrew_config_parcount(rr, pCount);
    return dbrew_rewrite(rr, 0.0, 0.0);
}

uint64_t handleVectorCall(Rewriter* r, uint64_t f, EmuState* es)
{
    uint64_t rf;
    VectorizeReq vr;

     // redirect original vector API call to this variant
    rf = expandedVectorVariant(f, r->vectorsize, &vr);

    // re-direct from scalar to vectorized kernel (function pointer in par1)
    uint64_t func = es->reg[RI_DI];
    uint64_t vfunc = convertToVector(r, func, vr);
    if (vfunc == func) {
        // vector expansion did not work: error
        if (r->showEmuSteps)
            printf("Error: expansion of %lx failed; no redirection\n", func);
        // call original DBrew snippet
        return f;
    }
    es->reg[RI_DI] = vfunc;
    return rf;
}


//----------------------------------------------------------
// vectorization pass
//

typedef enum _VecRegType {
    VRT_Invalid = 0,
    VRT_Unknown,
    VRT_Double,    // lower double set
    VRT_DoubleX2,  // original scalar double vectorized to 2 doubles
    VRT_DoubleX4,  // original scalar double vectorized to 4 doubles
    VRT_PtrDoubleX2, // pointer to double => pointer to 2 doubles
    VRT_PtrDoubleX4, // pointer to double => pointer to 4 doubles
} VecRegType;

// maintain expansion state of 16 vector registers
VecRegType vrt[16];

static
void doVec(RContext* c, Instr* dst, Instr* src)
{
    static Error e;
    RegIndex ri1, ri2;
    VecRegType vrt1, vrt2;

    copyInstr(dst, src);

    switch(src->type) {
    case IT_ADDSD:
        ri1 = opIsVReg(&(src->dst)) ? regVIndex(src->dst.reg) : RI_None;
        vrt1 = (ri1 != RI_None) ? vrt[ri1] : VRT_Unknown;
        ri2 = opIsVReg(&(src->src)) ? regVIndex(src->src.reg) : RI_None;
        vrt2 = (ri2 != RI_None) ? vrt[ri2] : VRT_Unknown;

        if ((vrt1 == VRT_DoubleX2) && (vrt2 == VRT_DoubleX2)) {
            dst->type = IT_ADDPD;
        }
        else {
            assert((vrt1 != VRT_DoubleX2) && (vrt1 != VRT_DoubleX4));
            assert((vrt2 != VRT_DoubleX2) && (vrt2 != VRT_DoubleX4));
        }
        break;

    case IT_VADDSD:
        ri1 = opIsVReg(&(src->dst)) ? regVIndex(src->dst.reg) : RI_None;
        vrt1 = (ri1 != RI_None) ? vrt[ri1] : VRT_Unknown;
        ri2 = opIsVReg(&(src->src)) ? regVIndex(src->src.reg) : RI_None;
        vrt2 = (ri2 != RI_None) ? vrt[ri2] : VRT_Unknown;

        if ((vrt1 == VRT_DoubleX2) && (vrt2 == VRT_DoubleX2)) {
            dst->type = IT_VADDPD;
            dst->ptLen = 0;
            attachPassthrough(dst, VEX_128, PS_66, OE_RVM, SC_None, 0x0F, 0x58, -1);
        }
        else if ((vrt1 == VRT_DoubleX4) && (vrt2 == VRT_DoubleX4)) {
            dst->type = IT_VADDPD;
            dst->ptLen = 0;
            attachPassthrough(dst, VEX_256, PS_66, OE_RVM, SC_None, 0x0F, 0x58, -1);
        }
        else {
            assert((vrt1 != VRT_DoubleX2) && (vrt1 != VRT_DoubleX4));
            assert((vrt2 != VRT_DoubleX2) && (vrt2 != VRT_DoubleX4));
        }
        break;

    case IT_HINT_CALL:
    case IT_HINT_RET:
    case IT_RET:
        break;
    default:
        setError(&e, ET_UnsupportedInstr, EM_Rewriter, c->r,
                 "Cannot handle instruction for vector expansion");
        c->e = &e;
    break;
    }
}

static
void vecPass(RContext* c, CBB* cbb)
{
    Instr *first = 0, *instr;
    Rewriter* r = c->r;
    int i;

    if (cbb->count == 0) return;

    if (r->showOptSteps) {
        printf("Run Vectorization for CBB (%lx|%d)\n",
               cbb->dec_addr, cbb->esID);
    }

    for(i = 0; i < cbb->count; i++) {
        instr = newCapInstr(c);
        if (!instr) return;
        if (!first) first = instr;

        doVec(c, instr, cbb->instr + i);
    }
    assert(first != 0);
    cbb->instr = first;
}


void runVectorization(RContext* c)
{
    int i;
    VecRegType retType;
    Rewriter* r = c->r;

    assert(r->vreq != VR_None);

    // tagging for xmm/ymm registers
    for(i=0; i<16; i++) vrt[i] = VRT_Unknown;
    retType = VRT_Unknown;

    switch(r->vreq) {
    case VR_None: break;
    case VR_DoubleX2_RV:
        vrt[0] = VRT_DoubleX2;
        retType = VRT_DoubleX2;
        break;
    case VR_DoubleX2_RVV:
        vrt[0] = VRT_DoubleX2;
        vrt[1] = VRT_DoubleX2;
        retType = VRT_DoubleX2;
        break;
    case VR_DoubleX2_RP:
        vrt[0] = VRT_PtrDoubleX2;
        retType = VRT_DoubleX2;
        break;
    case VR_DoubleX4_RV:
        vrt[0] = VRT_DoubleX4;
        retType = VRT_DoubleX4;
        break;
    case VR_DoubleX4_RVV:
        vrt[0] = VRT_DoubleX4;
        vrt[1] = VRT_DoubleX4;
        retType = VRT_DoubleX4;
        break;
    case VR_DoubleX4_RP:
        vrt[0] = VRT_PtrDoubleX4;
        retType = VRT_DoubleX4;
        break;
    default: assert(0);
    }

    assert(r->capBBCount == 1);
    vecPass(c, r->capBB);
    if (c->e) return;

    // check for expanded return value
    if (retType != VRT_Invalid)
        assert(vrt[0] == retType);
}
