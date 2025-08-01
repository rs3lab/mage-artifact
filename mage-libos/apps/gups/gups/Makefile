CC = gcc
CFLAGS = -g -Wall -O3
#CFLAGS = -g3 -Wall -O0
LIBS = -lm -lpthread

default: gups-hotset-move gups-hotset-move-notouch

all: gups-hotset-move gups-hotset-move-notouch

gups-hotset-move: gups-hotset-move.o zipf.o
	$(CC) $(CFLAGS) -o gups-hotset-move gups-hotset-move.o zipf.o $(LIBS)

gups-hotset-move-notouch: gups-hotset-move-notouch.o zipf.o
	$(CC) $(CFLAGS) -o gups-hotset-move-notouch gups-hotset-move-notouch.o zipf.o -DNOTOUCH  $(LIBS)

gups.o: gups.c gups.h
	$(CC) $(CFLAGS) -c gups.c -o gups.o

gups-hotset-move.o: gups-hotset-move.c gups.h
	$(CC) $(CFLAGS) -c gups-hotset-move.c -o gups-hotset-move.o

gups-hotset-move-notouch.o: gups-hotset-move.c gups.h
	$(CC) $(CFLAGS) -c gups-hotset-move.c -DNOTOUCH -o gups-hotset-move-notouch.o

zipf.o: zipf.c gups.h
	$(CC) $(CFLAGS) -c zipf.c -o zipf.o

clean:
	$(RM) *.o gups gups-hotset-move gups-hotset-move-notouch
