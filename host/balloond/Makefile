CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

SRC := src/main.c src/shm.c src/policy.c src/log.c src/qmp.c
OBJ := $(SRC:.c=.o)

all: balloond

balloond: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -f $(OBJ) balloond
