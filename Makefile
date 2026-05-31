CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE
RAYLIB_FLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: milestone5

milestone4:
	$(CC) $(CFLAGS) step4.c -o sim $(RAYLIB_FLAGS)

milestone5:
	$(CC) $(CFLAGS) step5.c -o sim $(RAYLIB_FLAGS)

clean:
	rm -f sim dijkstra *.o
