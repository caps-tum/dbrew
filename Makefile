
WFLAGS_BASE=-Wall \
            -Wmissing-field-initializers -Wunused-parameter -Wold-style-definition \
            -Wmissing-declarations -Wmissing-prototypes -Wredundant-decls \
            -Wmissing-noreturn -Wshadow -Wpointer-arith -Wno-cast-align \
            -Wwrite-strings -Winline -Wformat-nonliteral -Wformat-security \
            -Wswitch-default -Winit-self -Wnested-externs  -Wstrict-prototypes \
            -Wmissing-include-dirs -Wundef -Wmissing-format-attribute
ifeq ($(CI),1)
WFLAGS=-Werror $(WFLAGS_BASE)
else
WFLAGS=$(WFLAGS_BASE)
endif

WFLAGS2=-Wextra -Wswitch-enum -Wswitch -Waggregate-return

CFLAGS=-g -std=gnu99 -Iinclude -Iinclude/priv $(WFLAGS)
LDFLAGS=-g

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard include/*.h)

SUBDIRS=tests examples
.PHONY: $(SUBDIRS)

all: libdbrew.a $(SUBDIRS)

libdbrew.a: $(OBJS)
	ar rcs libdbrew.a $(OBJS)

test: libdbrew.a
	$(MAKE) test -C tests

examples:
	cd examples && $(MAKE)

clean:
	rm -rf *~ *.o $(OBJS) libdbrew.a
	$(MAKE) clean -C tests
	cd examples && make clean
