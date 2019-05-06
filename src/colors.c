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

// TODO: If somebody is really bored at some point do this properly, i.e., with
// terminal capability detection, etc...

#include "colors.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>

static bool enabled = false;

#define _RESET "\033[0m"

void setColors(bool state) {
    enabled = state;
}

int cprintf(int colors, const char* fmt, ...)
{
    uint8_t attr = (uint8_t) (colors & CAMask);
    uint8_t fg = ((uint8_t) ((colors & CFMask)) >> 4);
    uint8_t bg = ((uint8_t) ((colors & CBMask)) >> 8);

    assert(attr < _CMaxVal);
    assert(fg < _CMaxVal);
    assert(bg < _CMaxVal);

    int ret;

    va_list args;
    va_start(args, fmt);
    if (enabled && fg > 0 && bg > 0) {
        printf("\x1B[%d;%d;%dm", attr, fg + 29, bg + 39);
    } else if (enabled && fg > 0) {
        printf("\x1B[%d;%dm", attr, fg + 29);
    } else if (enabled) {
        printf("\x1B[%dm", attr);
    }
    ret = vprintf(fmt, args);
    if (enabled) printf(_RESET);
    va_end(args);
    return ret;
}
