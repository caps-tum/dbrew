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

/**
 * Error processing in DBrew
 *
 * At any step, there may be a (recoverable) error situation
 * Any such situation is passed to callers using the defined Error struct
 */

#ifndef ERROR_H
#define ERROR_H

#include "dbrew.h"

typedef struct _Error Error;
typedef struct _DecodeError DecodeError;
typedef struct _GenerateError GenerateError;
typedef struct _RewriteError RewriteError;
typedef struct _EmulationError EmulationError;

typedef enum _ErrorModule {
    EM_Unknown,
    EM_Decoder, EM_Emulator, EM_Generator, EM_Capture, EM_Rewriter,
    EM_Max
} ErrorModule;

typedef enum _ErrorType {
    ET_NoError,
    ET_Unknown,
    ET_InvalidRequest, // Rewriter
    ET_BufferOverflow, // Decoder, Generator, Rewriter
    ET_UnsupportedInstr, ET_UnsupportedOperands, // Generator, Emulator
    // Decoder
    ET_BadPrefix, ET_BadOpcode, ET_BadOperands,
    //
    ET_Max
} ErrorType;

struct _Error {
    ErrorModule em;
    ErrorType et;
    Rewriter* r;

    const char* desc; // textual description
};

void setErrorNone(Error* e);
bool isErrorSet(Error*);
void initError(Error* e);
void setError(Error* e, ErrorType et, ErrorModule em, Rewriter* r, const char *d);
const char* errorString(Error*);
// add error to some log, with further description (e.g. recover action)
// currently just to stderror
void logError(Error* e, char* d);


// extensions with more context info

struct _DecodeError {
    Error e; // must be first

    // additional context
    DBB* dbb;
    int offset;
};

void setDecodeError(DecodeError* de, Rewriter* r, char* d,
                    ErrorType et, DBB* dbb, int off);
const char* decodeErrorContext(Error*); // used by errorString()


struct _GenerateError {
    Error e; // must be first

    // additional context
    CBB* cbb;
    int offset;
};

void setGenerateError(GenerateError* ge, Rewriter* r, char* d,
                      ErrorType et, CBB* cbb, int off);
const char* generateErrorContext(Error*); // used by errorString()


#endif // ERROR_H
