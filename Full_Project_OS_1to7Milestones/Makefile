CC = gcc
CFLAGS = -Wall -Wextra -std=c99
RAYLIB_FLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

milestone1:
	$(CC) $(CFLAGS) main.c -o dijkstra

milestone4:
	$(CC) $(CFLAGS) sim_milestone4.c -o sim $(RAYLIB_FLAGS)

milestone5:
	$(CC) $(CFLAGS) sim_milestone5.c -o sim $(RAYLIB_FLAGS)

milestone6:
	$(CC) $(CFLAGS) sim_milestone6.c -o sim $(RAYLIB_FLAGS)
milestone7:
	$(CC) $(CFLAGS) sim.c -o sim $(RAYLIB_FLAGS)
clean:
	rm -f dijkstra sim read_test ipc_test