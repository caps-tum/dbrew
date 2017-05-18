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

/**
 * \file
 **/

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <abstractbb.h>

#include <common.h>
#include <printer.h>


/**
 * \ingroup AbstractBB
 * \defgroup AbstractBB Abstract Basic Block
 * \brief An abstraction of the Captured Basic Block (#CBB) and a Decoded Basic
 * Block (#DBB)
 *
 * @{
 **/

/**
 * \brief An abstract basic block
 **/
struct AbstractBB {
    /**
     * \brief Whether the abstract basic block wraps a captured block
     **/
    bool isCaptured;

    union {
        CBB* capturedBlock;
        DBB* decodedBlock;
    } u;
};

/**
 * Construct a new abstract basic block based of a captured basic block.
 *
 * \author Alexis Engelke
 *
 * \param cbb The captured block
 * \returns An abstract basic block wrapping the captured block
 **/
AbstractBB*
dbrew_abb_new_captured(CBB* cbb)
{
    AbstractBB* abb;

    abb = malloc(sizeof(AbstractBB));
    abb->isCaptured = true;
    abb->u.capturedBlock = cbb;

    return abb;
}

/**
 * Construct a new abstract basic block based of a decoded basic block.
 *
 * \author Alexis Engelke
 *
 * \param dbb The decoded block
 * \returns An abstract basic block wrapping the decoded block
 **/
AbstractBB*
dbrew_abb_new_decoded(DBB* dbb)
{
    AbstractBB* abb;

    abb = malloc(sizeof(AbstractBB));
    abb->isCaptured = false;
    abb->u.decodedBlock = dbb;

    return abb;
}

/**
 * Free an abstract basic block. The actual captured or decoded block remains
 * unchanged.
 *
 * \author Alexis Engelke
 *
 * \param abb The abstract basic block
 **/
void
dbrew_abb_free(AbstractBB* abb)
{
    free(abb);
}

/**
 * Iterate over the instructions of a basic block. The supplied user data will
 * be passed as second argument to the iterator.
 *
 * \author Alexis Engelke
 *
 * \param abb The abstract basic block
 * \param iterator The instruction iterator
 * \param userData The user data to pass to the iterator
 **/
void
dbrew_abb_instructions_each(AbstractBB* abb, IntructionIterator iterator, void* userData)
{
    int count;
    Instr* instrs;

    if (abb->isCaptured)
    {
        count = abb->u.capturedBlock->count;
        instrs = abb->u.capturedBlock->instr;
    }
    else
    {
        count = abb->u.decodedBlock->count;
        instrs = abb->u.decodedBlock->instr;
    }

    for (int i = 0; i < count; i++)
    {
        iterator(instrs + i, userData);
    }
}

/**
 * Get the subsequent basic block.
 *
 * \todo Implement this for decoded basic blocks.
 *
 * \author Alexis Engelke
 *
 * \param abb The abstract basic block
 * \param fallThrough Whether the fall through branch should be returned
 * \returns An abstract basic block which follows the supplied block
 **/
AbstractBB*
dbrew_abb_get_next_block(AbstractBB* abb, bool fallThrough)
{
    AbstractBB* result = NULL;

    if (abb->isCaptured)
    {
        CBB* nextBranch = fallThrough ? abb->u.capturedBlock->nextFallThrough : abb->u.capturedBlock->nextBranch;

        if (nextBranch != NULL)
        {
            result = dbrew_abb_new_captured(nextBranch);
        }
    }
    else
    {
        // Not implemented yet.
    }

    return result;
}

/**
 * @}
 **/
