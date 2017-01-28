WFLAGS_BASE=-Wall -Wextra -Wmissing-field-initializers -Wunused-parameter \
            -Wold-style-definition -Wmissing-declarations -Wmissing-prototypes \
            -Wredundant-decls -Wmissing-noreturn -Wshadow -Wpointer-arith \
            -Wwrite-strings -Winline -Wformat-nonliteral -Wformat-security \
            -Wswitch-default -Winit-self -Wnested-externs -Wstrict-prototypes \
            -Wmissing-include-dirs -Wundef -Wmissing-format-attribute
ifeq ($(CI),1)
WFLAGS=-Werror $(WFLAGS_BASE)
else
WFLAGS=$(WFLAGS_BASE)
endif

WFLAGS2=-Wswitch-enum -Wswitch -Waggregate-return

CFLAGS=-g -std=gnu99 -Iinclude -Iinclude/priv $(WFLAGS)
LDFLAGS=-g

# always compile examples and DBrew snippets with optimizations
# this allows other DBrew code to still be debuggable
OPTFLAGS=-O2 -mavx

## flags dependent on compiler
CCNAME:=$(strip $(shell $(CC) --version | head -c 3))
ifeq ($(CCNAME),$(filter $(CCNAME),gcc cc icc))
 # gcc/cc
 $(info ** gcc compatible compiler detected: $(CC))
 CFLAGS  += -fno-pie
 ifeq ($(shell expr `$(CC) -dumpversion | cut -f1 -d.` \>= 5),1)
  LDFLAGS += -no-pie
 endif

 # some snippets 'switch' to AVX mode. hack to avoid 32-byte stack alignment
 # FIXME: rewriter should move such stack alignment to outermost level
 # clang does not know these options
 SNIPPETSFLAGS=$(OPTFLAGS) -mno-vzeroupper -mpreferred-stack-boundary=5

else ifeq ($(shell $(CC) -v 2>&1 | grep -c "clang version"), 1)
 # clang
 $(info ** clang detected: $(CC))
 CFLAGS += -fno-pie
 SNIPPETSFLAGS=$(OPTFLAGS)
else
 $(error Compiler $(CC) not supported)
endif

SRCS = $(wildcard src/*.c)
HEADERS = $(wildcard include/*.h)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

# instruct GCC to produce dependency files
CFLAGS += -MMD -MP

SUBDIRS=tests examples
.PHONY: $(SUBDIRS)

all: libdbrew.a $(SUBDIRS)

libdbrew.a: $(OBJS)
	ar rcs libdbrew.a $(OBJS)

test: libdbrew.a
	$(MAKE) test -C tests CC=$(CC)

src/snippets.o: src/snippets.c
	$(CC) $(CFLAGS) $(SNIPPETSFLAGS) -c $< -o $@

examples: libdbrew.a
	cd examples && $(MAKE) OPTS='$(OPTFLAGS)' CC=$(CC)

clean:
	rm -f *~ *.o $(OBJS) $(DEPS) libdbrew.a
	$(MAKE) clean -C tests
	cd examples && make clean

# include previously generated dependency rules if existing
-include $(DEPS)

tidy: compile_commands.json
	git ls-files '*.c' | xargs -P 1 -I{} clang-tidy -header-filter=.* -p . {}; fi

compile_commands.json:
	bear make


