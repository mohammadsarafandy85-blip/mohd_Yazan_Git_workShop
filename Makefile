CC = gcc
CFLAGS = -Wall -Wextra -std=c99
RAYLIB_FLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: milestone3

milestone1:
	$(CC) $(CFLAGS) step3.c -o dijkstra -lm

milestone2:
	$(CC) $(CFLAGS) step3.c -o sim $(RAYLIB_FLAGS)

milestone3:
	$(CC) $(CFLAGS) step3.c -o sim $(RAYLIB_FLAGS)

milestone4:
	$(CC) $(CFLAGS) step3.c -o sim $(RAYLIB_FLAGS)

milestone5:
	$(CC) $(CFLAGS) step3.c -o sim $(RAYLIB_FLAGS)

milestone6:
	$(CC) $(CFLAGS) step3.c -o sim $(RAYLIB_FLAGS)

milestone7:
	$(CC) $(CFLAGS) step3.c -o sim $(RAYLIB_FLAGS)

clean:
	rm -f dijkstra sim *.o
