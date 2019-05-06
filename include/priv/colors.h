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

#include <stdbool.h>

#ifndef _COLORS_H
#define _COLORS_H

typedef enum _Colors {
    // Attributes
    CAReset = 0,
    CABright,
    CADim,
    CAUnderline,
    CABlink,
    CAReverse,
    CAHidden,
    //CAMax,
    CAMask = 0xf,

    // Foreground colors
    CFNone = 0x00,
    CFBlack = 0x10,
    CFRed = 0x20,
    CFGreen = 0x30,
    CFYellow = 0x40,
    CFBlue = 0x50,
    CFMagenta = 0x60,
    CFCyan = 0x70,
    CFWhite = 0x80,
    //CFMax = 0x81,
    CFMask = 0xf0,

    // Foreground colors
    CBBlack = 0x100,
    CBRed = 0x200,
    CBGreen = 0x300,
    CBYellow = 0x400,
    CBBlue = 0x500,
    CBMagenta = 0x600,
    CBCyan = 0x700,
    CBWhite = 0x800,
    //CBMax = 0x801,
    CBMask = 0xf00,

    _CMaxVal = 8,
} Colors;

void setColors(bool state);
int cprintf(int colors, const char* fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

#endif // _COLORS_H
