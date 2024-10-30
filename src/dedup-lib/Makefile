CFLAGS=-Wall -fPIC
CXXFLAGS=-Wall -fPIC
LDFLAGS=-L.
LIBS=-lssl -lcrypto 
OBJECTS=rabinpoly.o msb.o
LIBDEDUP=libdedup.a

ifdef DEBUG
	CFLAGS+=-g
	CXXFLAGS+=-g
	LIBDEDUP=libdedup-dbg.a
endif

all: clean rabin-example

rabin-example : rabin-example.o $(LIBDEDUP)
	g++ $(LDFLAGS) -o $@ $^ $(LIBS)

$(LIBDEDUP) : $(OBJECTS)
	ar -rcs $@ $?

%.o : %.c 
	g++ $(CXXFLAGS) -c -o $@ $<

%.c : %.h

.PHONY: clean

clean : 
	rm -f *.o libdedup*.a rabin-example
