
#ifndef DBREW_XED_H
#define DBREW_XED_H

#include <dbrew.h>

/// Decode a basic block at a given address using Intel XED.
///
/// The returned DBB does *not* belong to a rewriter but must be freed
/// separately. If debug is >= 1, the decoded instructions are printed.
DBB* dbrew_xed_decode(int debug, uintptr_t address);

#endif
