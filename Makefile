
# Can (and should) be specified via the command line.
SUBDIRS=examples

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

CFLAGS=-g -std=gnu99 -Iinclude -Iinclude/priv -fPIC $(WFLAGS)
LDFLAGS=-g

# always compile examples and DBrew snippets with optimizations
# this allows other DBrew code to still be debuggable
OPTFLAGS=-O2 -mavx

## flags dependent on compiler
CCNAME:=$(strip $(shell $(CC) --version | head -c 3))
ifeq ($(CCNAME),$(filter $(CCNAME),gcc cc icc))
 # gcc/cc
 $(info ** gcc compatible compiler detected: $(CC))

 # some snippets 'switch' to AVX mode. hack to avoid 32-byte stack alignment
 # FIXME: rewriter should move such stack alignment to outermost level
 # clang does not know these options
 SNIPPETSFLAGS=$(OPTFLAGS) -mno-vzeroupper -mpreferred-stack-boundary=5

else ifeq ($(shell $(CC) -v 2>&1 | egrep -c "(clang version|Apple LLVM version)"), 1)
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


# Build targets

SUBDIRS_BUILD=$(addprefix build_, $(SUBDIRS))
.PHONY: $(SUBDIRS_BUILD)

all: libdbrew.a $(SUBDIRS_BUILD)

libdbrew.a: $(OBJS) $(HEADERS)
	ar rcs libdbrew.a $(OBJS)

$(SUBDIRS_BUILD): build_%: libdbrew.a
	+$(MAKE) -C $*

src/snippets.o: src/snippets.c
	$(CC) $(CFLAGS) $(SNIPPETSFLAGS) -c $< -o $@


## Clean targets

SUBDIRS_CLEAN=$(addprefix clean_, $(SUBDIRS))
.PHONY: $(SUBDIRS_CLEAN)

clean: clean_dbrew $(SUBDIRS_CLEAN)

clean_dbrew:
	rm -f *~ *.o $(OBJS) $(DEPS) libdbrew.a
	+$(MAKE) clean -C tests

$(SUBDIRS_CLEAN): clean_%:
	+$(MAKE) clean -C $*


## Test targets

SUBDIRS_TEST=$(addprefix test_, $(SUBDIRS))
.PHONY: $(SUBDIRS_TEST)

test: libdbrew.a test_dbrew $(SUBDIRS_TEST)

test_dbrew: libdbrew.a
	cd tests && ./test.py

$(SUBDIRS_TEST): test_%: build_%
	+$(MAKE) test -C $*


# include previously generated dependency rules if existing
-include $(DEPS)

tidy: compile_commands.json
	git ls-files '*.c' | xargs -P 1 -I{} clang-tidy -header-filter=.* -p . {}; fi

compile_commands.json:
	bear make
