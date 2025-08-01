CC = gcc
CFLAGS = -Wall -g -ggdb -march=native -O3

SRC = rdma_monitor.c
OBJ = $(SRC:.c=.o)
EXEC = rdma_monitor

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^
%.o: $.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean

clean:
	rm -f $(OBJ) $(EXEC)
