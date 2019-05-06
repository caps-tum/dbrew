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

#ifndef _DBREW_INTROSPECT_H
#define _DBREW_INTROSPECT_H

#include "config.h"

#include "dbrew.h"

#include <stddef.h>

// TODO: Strongly consider consolidating the two following structs and only have
// a single retrieval function for all info

// Always defined
typedef struct _ElfAddrInfo {
    #define ELF_MAX_NAMELEN 255
    int lineno;
    char filePath[ELF_MAX_NAMELEN];
    char* fileName;
    const char symbName[ELF_MAX_NAMELEN];
    char* line;
    uint64_t addr;
} ElfAddrInfo;

typedef struct _AddrSymInfo {
    uint64_t size;
    uint64_t offset;
    uint64_t addr;
    char name[ELF_MAX_NAMELEN];
} AddrSymInfo;

// Never build with dwarf support for testing target
#ifdef TEST_BUILD
#undef HAVE_LIBDW
#endif

// Conditional exports
#ifdef HAVE_LIBDW

#include <elf.h>
#include <elfutils/libdwfl.h>

typedef struct _LineMap {
    char* srcPtr;
    int lineno;
} LineMap;


typedef struct _SourceFile {
    uint64_t start; // File start addr
    uint64_t len; // File end addr
    char* basePtr; // Starting pointer of mmaped file
    char* curPtr; // Current location
    uint64_t fileLen; // Length of file
    char filePath[ELF_MAX_NAMELEN]; // File path
    int curLine; // Current line pointer
    int maxLines; // Current upper boundary of linePtr array
    char** linePtr; // Array of pointers to line beginnings
    struct _SourceFile* next; // Chain of SourceFiles
} SourceFile;

typedef struct _ElfContext {
    Dwfl* dwfl;
    SourceFile* sf;
    Dwfl_Callbacks* callbacks;
    char* debuginfo_path;
} ElfContext;

// Return a file descriptor to actual current path
int initElfData(Rewriter* r, int pid);
void freeElfData(Rewriter* r);
bool addrToLine(Rewriter* r, uint64_t addr, ElfAddrInfo* retInfo);
int addrToFun(Rewriter* r, uint64_t addr, char* buf, size_t len);
bool addrToSym(Rewriter* r, uint64_t addr, AddrSymInfo* retInfo);
uint64_t symToAddr(Rewriter* r, const char* symName);

// Line handling functions
char* getSourceLine(Rewriter* r, const char* fp, int lineno);
void freeSourceFile(SourceFile* s);

__attribute__((unused))
static inline bool haveIntrospection( void ) {
    return true;
}

#else

typedef struct { int a; } ElfContext;
typedef struct { int a; } LineMap;

#define getSelfFd(a) 0
static inline int initElfData(Rewriter* r __attribute__((unused)),
                              int pid __attribute__((unused))) {
    return 1;
}

static inline void freeElfData(Rewriter* r __attribute__((unused)))
{}


static inline bool addrToLine(Rewriter* r __attribute__((unused)),
                              uint64_t addr __attribute__((unused)),
                              ElfAddrInfo* retInfo __attribute__((unused))) {
    return false;
}

static inline char* getSourceLine(Rewriter* r __attribute__((unused)),
                                  const char* fn __attribute__((unused)),
                                  int lineno __attribute__((unused))) {
    return 0;
}

static inline int addrToFun(Rewriter* r __attribute__((unused)),
                            uint64_t addr __attribute__((unused)),
                            char* buf __attribute__((unused)),
                            size_t len __attribute__((unused))) {
    return 1;
}

static inline bool addrToSym(Rewriter* r __attribute__((unused)),
                             uint64_t addr __attribute__((unused)),
                             AddrSymInfo* sumInfo __attribute__((unused))) {
    return 0;
}

static inline uint64_t symToAddr(Rewriter* r __attribute__((unused)),
                                 const char* symName __attribute__((unused))) {
    return 0;
}


__attribute__((unused))
static inline bool haveIntrospection( void ) {
    return false;
}

#endif // HAS_LIBDW

#endif // _DBREW_INTROSPECT_H
