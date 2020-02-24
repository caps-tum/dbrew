#include "dbrew.h"

int foo(int i, int j)
{
  if (i == 5) return 0;
  return i+j;
}

typedef int (*foo_t)(int, int);

int main(int argc, char* argv[])
{
  Rewriter* r = dbrew_new();
  dbrew_verbose(r, true, true, true);
  dbrew_set_function(r, (uint64_t) foo);
  dbrew_config_staticpar(r, 0);
  dbrew_config_parcount(r, 2);
  foo_t f = (foo_t) dbrew_rewrite(r, 2, 3);
  return f(2, 3);
}

