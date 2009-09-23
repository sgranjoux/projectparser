
## Created by Anjuta

CC = gcc
CFLAGS = -g -Wall
OBJECTS = foobar.o
INCFLAGS = 
LDFLAGS = -Wl,-rpath,/usr/local/lib
LIBS = 

all: foobar

foobar: $(OBJECTS)
	$(CC) -o foobar $(OBJECTS) $(LDFLAGS) $(LIBS)

.SUFFIXES:
.SUFFIXES:	.c .cc .C .cpp .o

.c.o :
	$(CC) -o $@ -c $(CFLAGS) $< $(INCFLAGS)

count:
	wc *.c *.cc *.C *.cpp *.h *.hpp

clean:
	rm -f *.o

.PHONY: all
.PHONY: count
.PHONY: clean
