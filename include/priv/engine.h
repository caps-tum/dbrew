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

#ifndef ENGINE_H
#define ENGINE_H

#include "common.h"
#include "error.h"

#include <stdarg.h>
#include <stdint.h>

// rewriter context
typedef struct _RContext RContext;
struct _RContext
{
    Rewriter* r;
    uint64_t exit;
    Error* e;
};

Rewriter* allocRewriter(void);
void initRewriter(Rewriter* r);
void freeRewriter(Rewriter* r);

// Rewrite engine
Error* vEmulateAndCapture(Rewriter* r, va_list args);
void runOptsOnCaptured(RContext *c);
void generateBinaryFromCaptured(RContext* c);

#endif // ENGINE_H
