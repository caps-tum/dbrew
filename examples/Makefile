EXAMPLES = stencil strcmp
CPPFLAGS=-I../include
LDLIBS=-L.. -ldbrew
CFLAGS=-O2

all: $(EXAMPLES)

stencil: stencil.o ../libdbrew.a

strcmp: strcmp.o ../libdbrew.a

clean:
	rm -f *.o *~ $(EXAMPLES)