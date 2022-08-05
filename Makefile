CC=gcc
CFLAGS=-Wall -Wextra -g3
LIBS=-lSDL2 -lSDL2_image -lm

.PHONY: clean

all: world_of_ants

world_of_ants: world_of_ants.c
	$(CC) $(CFLAGS) $(LIBS) -o world_of_ants world_of_ants.c

clean:
	rm world_of_ants
