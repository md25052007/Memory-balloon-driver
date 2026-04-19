CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

all: shm_agent

shm_agent: main.c
	$(CC) $(CFLAGS) -o $@ main.c

clean:
	rm -f shm_agent
