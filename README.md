# Operating Systems Project - Graph Traffic Simulation

## Milestone 6

This milestone adds synchronization for access to graph nodes.

The program reads a directed weighted graph and several travelers from an input file. Each traveler is a separate child process created with `fork()`. Each child computes its own Dijkstra shortest path and reports its state to the parent using pipes. The parent process is the only process that prints logs and manages the raylib GUI.

## IPC mechanism

The IPC mechanism is **pipes**.

Each child has a pipe to the parent. The child sends messages when it waits outside a node, enters a node, leaves a node, and finishes. The parent reads these messages, prints logs, and updates the GUI.

## Synchronization mechanism

The synchronization mechanism is **POSIX named semaphores**.

The parent creates one semaphore for each graph node. Every node semaphore is initialized to `1`, so only one traveler can enter a node at a time. When a child reaches a node, it sends a WAITING message, calls `sem_wait()`, enters the critical section, waits inside the node for exactly 1 second, and then calls `sem_post()` to allow another waiting traveler to enter.

This prevents two travelers from being inside the same node at the same time.

## Build commands

```bash
make milestone4
make milestone5
make milestone6
```

## Run command

```bash
./sim input6.txt
```

## Clean

```bash
make clean
```

## Example input for milestone 6

```text
# graph definition
5 7
0 3 2
1 3 2
2 3 2
3 4 2
0 4 10
1 4 10
2 4 10
# travelers
3
0 4
1 4
2 4
```

This input creates three travelers that all need to pass through node `3`, so the GUI clearly shows travelers waiting outside the same node and entering one after another.
