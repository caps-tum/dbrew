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

#ifndef DBREW_ABB
#define DBREW_ABB

#include <stdbool.h>

#include <common.h>

struct AbstractBB;

typedef struct AbstractBB AbstractBB;

typedef void (*IntructionIterator)(Instr*, void*);

AbstractBB* dbrew_abb_new_captured(CBB*);
AbstractBB* dbrew_abb_new_decoded(DBB*);
void dbrew_abb_free(AbstractBB*);

void dbrew_abb_instructions_each(AbstractBB*, IntructionIterator, void*);
AbstractBB* dbrew_abb_get_next_block(AbstractBB*, bool);

#endif
