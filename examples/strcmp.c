/*
 * Example for DBrew API
 *
 * Rewrite a shared library function
 */

#include <string.h>
#include "dbrew.h"

typedef int (*f_t)(char*);

int isHello(char* s)
{
  return strcmp(s, "Hello");
}

int main(int argc, char* argv[])
{
    // force relocation of strcmp
    if (strcmp("Foo", argv[0])==0) return 0;

    dbrew_def_verbose(true, true, true);
    f_t f = (f_t) dbrew_rewrite_func((uint64_t) isHello, "Bla");

    return f(argv[1]);
}
