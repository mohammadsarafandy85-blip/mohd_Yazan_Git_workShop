CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE
RAYLIB_FLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: milestone6

milestone1:
	$(CC) $(CFLAGS) step3.c -o dijkstra -lm

milestone2:
	$(CC) $(CFLAGS) step3.c -o sim $(RAYLIB_FLAGS)

milestone3:
	$(CC) $(CFLAGS) step3.c -o sim $(RAYLIB_FLAGS)

milestone4:
	$(CC) $(CFLAGS) step4.c -o sim $(RAYLIB_FLAGS)

milestone5:
	$(CC) $(CFLAGS) step5.c -o sim $(RAYLIB_FLAGS)

milestone6:
	$(CC) $(CFLAGS) step6.c -o sim $(RAYLIB_FLAGS)

clean:
	rm -f dijkstra sim *.o
