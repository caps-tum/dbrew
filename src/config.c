/**
 * This file is part of DBrew, the dynamic binary rewriting library.
 *
 * (c) 2015-2016, Josef Weidendorfer <josef.weidendorfer@gmx.de>
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

#include "common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static
void cc_init(CaptureConfig* cc)
{
    for(int i=0; i < CC_MAXPARAM; i++)
        initMetaState(&(cc->par_state[i]), CS_DYNAMIC);
    for(int i=0; i < CC_MAXPARAM; i++)
        cc->par_name[i] = 0;
    for(int i=0; i < CC_MAXCALLDEPTH; i++)
        cc->force_unknown[i] = false;
    cc->hasReturnFP = false;
    cc->parCount = -1; // unknown
    cc->branches_known = false;
    cc->range_configs = 0;

}

static
void cc_free(CaptureConfig* cc)
{
    if (!cc) return;

    for(int i=0; i < CC_MAXPARAM; i++)
        free(cc->par_name[i]);

    MemRangeConfig* fc = cc->range_configs;
    while(fc) {
        MemRangeConfig* next = fc->next;
        free(fc);
        fc = next;
    }
    free(cc);
}

static
CaptureConfig* cc_new(void)
{
    CaptureConfig* cc;

    cc = (CaptureConfig*) malloc(sizeof(CaptureConfig));
    cc_init(cc);

    return cc;
}

// allocate a configuration for the rewriter if not already existing
static
CaptureConfig* cc_get(Rewriter* r)
{
    if (r->cc == 0)
        r->cc = cc_new();

    return r->cc;
}

static
MemRangeConfig* mrc_new(MemRangeType type, char* name,
                       uint64_t start, int size,
                       MemRangeConfig* next, CaptureConfig* cc)
{
    MemRangeConfig* mrc;

    if (type == MR_Function) {
        FunctionConfig* fc = (FunctionConfig*) malloc(sizeof(FunctionConfig));
        mrc = (MemRangeConfig*) fc;
    }
    else
        mrc = (MemRangeConfig*) malloc(sizeof(MemRangeConfig));

    mrc->type = type;
    mrc->name = (name == 0) ? 0 : strdup(name);
    mrc->start = start;
    mrc->size = size;
    mrc->next = next;
    mrc->cc = cc;

    return mrc;
}

static
MemRangeConfig* mrc_find(CaptureConfig* cc, MemRangeType type, uint64_t addr)
{
    MemRangeConfig* mrc;

    mrc = cc->range_configs;
    while(mrc) {
        if ((type == MR_Unknown) || (mrc->type == type)) {
            // on exact match, size does not matter
            if (addr == mrc->start) return mrc;
            // check if we fall into address range
            if ((addr > mrc->start) && (addr < mrc->start+mrc->size))
                return mrc;
        }
        mrc = mrc->next;
    }
    return 0;
}

static
FunctionConfig* fc_get(CaptureConfig* cc, uint64_t func)
{
    MemRangeConfig* fc;

    fc = mrc_find(cc, MR_Function, func);
    if (!fc) {
        fc = mrc_new(MR_Function, 0, func, 0, cc->range_configs, cc);
        cc->range_configs = fc;
    }
    assert(fc->type == MR_Function);
    return (FunctionConfig*) fc;
}

// DBrew internal, called by other modules

bool config_is_constant(Rewriter* r, uint64_t addr, size_t size)
{
    CaptureConfig* cc = cc_get(r);
    MemRangeConfig* mrc = mrc_find(cc, MR_ConstantData, addr);
    if (mrc) {
        assert(mrc->type == MR_ConstantData);
        if (mrc->start + mrc->size >= addr + size)
            return true;
    }
    return false;
}

FunctionConfig* config_find_function(Rewriter* r, uint64_t f)
{
    CaptureConfig* cc = cc_get(r);
    MemRangeConfig* mrc = mrc_find(cc, MR_Function, f);
    if (mrc) {
        assert(mrc->type == MR_Function);
        return (FunctionConfig*) mrc;
    }
    return 0;
}


//---------------------------------------------------------------------
// DBrew API functions for configuration

void dbrew_config_reset(Rewriter* r)
{
    if (r->cc)
        cc_free(r->cc);

    r->cc = cc_new();
}

void dbrew_config_staticpar(Rewriter* r, int staticParPos)
{
    CaptureConfig* cc = cc_get(r);

    assert((staticParPos >= 0) && (staticParPos < CC_MAXPARAM));
    initMetaState(&(cc->par_state[staticParPos]), CS_STATIC2);
}

void dbrew_config_par_setname(Rewriter* c, int par, char* name)
{
    CaptureConfig* cc = cc_get(c);

    assert((par >= 0) && (par < CC_MAXPARAM));
    cc->par_name[par] = strdup(name);
}

/**
 * This allows to specify for a given function inlining depth that
 * values produced by binary operations always should be forced to unknown.
 * Thus, when result is known, it is converted to unknown state with
 * the value being loaded as immediate into destination.
 *
 * Brute force approach to prohibit loop unrolling.
 */
void dbrew_config_force_unknown(Rewriter* r, int depth)
{
    CaptureConfig* cc = cc_get(r);

    assert((depth >= 0) && (depth < CC_MAXCALLDEPTH));
    cc->force_unknown[depth] = true;
}

void dbrew_config_returnfp(Rewriter* r)
{
    CaptureConfig* cc = cc_get(r);
    cc->hasReturnFP = true;
}

void dbrew_config_parcount(Rewriter* r, int parCount)
{
    CaptureConfig* cc = cc_get(r);
    cc->parCount = parCount;
}

void dbrew_config_branches_known(Rewriter* r, bool b)
{
    CaptureConfig* cc = cc_get(r);
    cc->branches_known = b;
}

void dbrew_config_function_setname(Rewriter* r, uint64_t f, const char* name)
{
    CaptureConfig* cc = cc_get(r);
    FunctionConfig* fc = fc_get(cc, f);
    fc->name = strdup(name);
}

void dbrew_config_function_setsize(Rewriter* r, uint64_t f, int size)
{
    CaptureConfig* cc = cc_get(r);
    FunctionConfig* fc = fc_get(cc, f);
    fc->size = size;
}

void dbrew_config_set_memrange(Rewriter* r, char* name, bool isWritable,
                               uint64_t start, int size)
{
    MemRangeConfig* mrc;
    CaptureConfig* cc = cc_get(r);

    // TODO: check that we do not specify overlapping ranges

    mrc = mrc_new(isWritable ? MR_MutableData : MR_ConstantData,
                  name, start, size, cc->range_configs, cc);
    cc->range_configs = mrc;
}
