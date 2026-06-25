# Operating Systems Project — Graph Traffic Simulation

## Project Overview

This project implements a traffic simulation on a directed weighted graph.

The project starts with reading a graph from a text file and finding the shortest path using Dijkstra’s algorithm.
Then the project extends step by step into a graphical simulation using `raylib`, multiple travelers, child processes, IPC, synchronization, and scheduling algorithms.

The project includes all milestones from Milestone 1 to Milestone 7.

---

## Requirements

The project was developed in C on Linux.

Required libraries:

* Standard C libraries
* POSIX system calls
* `raylib`
* `pthread`
* `math`
* `X11`
* POSIX process functions such as `fork()`, `pipe()`, `waitpid()`, and signals

---

## Files in the Project

Recommended project structure:

```txt
main.c                 # Milestone 1 - Dijkstra terminal program
sim_milestone4.c       # Milestone 4 implementation
sim_milestone5.c       # Milestone 5 implementation
sim_milestone6.c       # Milestone 6 implementation
sim.c                  # Milestone 7 final implementation
Makefile
README.md

input.txt              # Input for Milestone 1
input4.txt             # Input for Milestone 4
input5.txt             # Input for Milestone 5
input6.txt             # Input for Milestone 6
input7.txt             # Input for Milestone 7
```

---

# Build Commands

Use the Makefile targets below.

## Milestone 1

```bash
make milestone1
```

Run:

```bash
./dijkstra input.txt
```

---

## Milestone 4

```bash
make milestone4
```

Run:

```bash
./sim input4.txt
```

---

## Milestone 5

```bash
make milestone5
```

Run:

```bash
./sim input5.txt
```

---

## Milestone 6

```bash
make milestone6
```

Run:

```bash
./sim input6.txt
```

---

## Milestone 7

```bash
make milestone7
```

Run with FCFS scheduling:

```bash
./sim -schd fcfs input7.txt
```

Run with SJF scheduling:

```bash
./sim -schd sjf input7.txt
```

---

## Clean Build Files

```bash
make clean
```

---

# Milestone 1 — Directed Weighted Graph and Dijkstra

## Goal

Milestone 1 builds the algorithmic base of the project.

The program:

* Reads a directed weighted graph from a text file
* Stores the graph in memory
* Runs Dijkstra’s shortest path algorithm
* Prints the shortest path and total weight to the terminal

## Input Format

```txt
N M
src dst weight
src dst weight
...
start target
```

Example:

```txt
6 8
0 1 4
0 2 2
1 3 5
2 1 1
2 3 8
3 4 2
4 5 3
2 5 10
0 5
```

## Output Example

```txt
0 -> 2 -> 5
12
```

## Special Cases

If no path exists:

```txt
No path found
```

If source equals destination:

```txt
0
0
```

If the input contains invalid negative values:

```txt
Invalid input
```

---

# Milestone 2 — Static GUI Graph Display

## Goal

Milestone 2 adds a graphical display using `raylib`.

The program:

* Opens a GUI window
* Draws all graph nodes
* Draws directed edges as arrows
* Displays edge weights
* Shows the graph in a readable layout

## Run

```bash
./sim input.txt
```

## Implementation Details

Nodes are positioned around a circle.
Each node is drawn as a circle with its node number inside.
Each edge is drawn as a directed arrow with its weight displayed near the edge.

---

# Milestone 3 — Single Traveler Animation

## Goal

Milestone 3 adds animation for one traveler moving on the Dijkstra shortest path.

The program:

* Reads the graph
* Calculates the shortest path using Dijkstra
* Draws the graph
* Highlights the shortest path
* Moves a traveler along the path
* Includes Play / Stop behavior
* Waits one second at intermediate nodes
* Moves across each edge according to its weight

## Movement Rule

Each edge with weight `W` is divided into `W` jumps.

Each jump takes:

```txt
300 milliseconds
```

Example:

```txt
edge weight = 2
movement time = 2 * 300ms = 600ms
```

```txt
edge weight = 10
movement time = 10 * 300ms = 3000ms
```

Intermediate nodes require a one-second wait.

---

# Milestone 4 — Multiple Travelers and Parent Process

## Goal

Milestone 4 extends the simulation from one traveler to multiple travelers.

The parent process controls the simulation.

## Parent Process Responsibilities

The parent process:

* Reads the graph and travelers from the input file
* Calculates Dijkstra paths for all travelers
* Creates child processes using `fork()`
* Manages the `raylib` GUI
* Displays all travelers on the screen
* Gives each traveler a different color
* Animates all travelers at the same time
* Sends a signal to terminate a child when its traveler arrives
* Uses `waitpid()` to wait for child processes

## Child Process Responsibilities

Each child process:

* Prints:

```txt
[PID] started
```

* Then stays alive until the parent terminates it

In Milestone 4, the children do not calculate paths and do not update the GUI.

## Input Format

```txt
# graph definition
N M
src dst weight
src dst weight
...

# travelers
T
source destination
source destination
...
```

Example:

```txt
# graph definition
6 8
0 1 4
0 2 2
1 3 5
2 1 1
2 3 8
3 4 2
4 5 3
2 5 10

# travelers
3
0 5
1 4
2 3
```

---

# Milestone 5 — IPC Between Child Processes and Parent

## Goal

Milestone 5 changes the structure of the project.

In Milestone 4, the parent calculated all paths.
In Milestone 5, each child process becomes autonomous.

Each child:

* Computes Dijkstra for itself
* Simulates its movement logically
* Sends updates to the parent process
* Does not print directly to the terminal

The parent:

* Receives messages from children
* Prints all logs
* Updates the GUI according to the messages

## IPC Method Used

The IPC mechanism used is:

```txt
pipe()
```

## Why pipe() Was Chosen

`pipe()` was chosen because:

* Communication is mainly one-way from child processes to the parent
* Each child sends structured messages to the parent
* The parent can read these messages and update the GUI
* It is simple and fits the project requirements well

## Message Structure

Each message contains information such as:

```txt
message type
PID
traveler index
current node
next node
destination status
movement time
```

## Expected Log Format

Example:

```txt
[PID=1021] arrived at node 0 | next node: 2
[PID=1022] arrived at node 2 | next node: 1
[PID=1021] arrived at node 2 | next node: 1
[PID=1022] arrived at node 3 | DESTINATION
[PID=1022] finished
```

Only the parent prints these logs.

---

# Milestone 6 — Synchronization Access to Nodes

## Goal

Milestone 6 adds synchronization to graph nodes.

The new rule is:

```txt
At any moment, only one traveler may be inside a node.
```

If several travelers arrive at the same node, only one enters.
The others wait outside the node.

Each traveler that enters a node stays inside for one full second.
After that, the traveler leaves and another waiting traveler may enter.

## Synchronization Method Used

The synchronization mechanism used is:

```txt
POSIX named semaphores
```

## Why Semaphores Were Chosen

Semaphores were chosen because:

* Each node can be treated as a critical section
* A semaphore value of `1` allows only one traveler inside the node
* Other travelers are blocked until the node is released
* This prevents two travelers from occupying the same node at the same time

## How It Works

Each node has its own semaphore.

Before entering a node, a child process does:

```c
sem_wait(nodeSemaphore);
```

This locks the node.

After staying inside the node for one second, the child process does:

```c
sem_post(nodeSemaphore);
```

This releases the node.

## GUI Behavior

The GUI shows travelers waiting outside a busy node.

In the implementation:

```txt
Yellow ring = traveler waiting outside a node
Normal traveler color = traveler inside node or traveling
```

## Test Input for Milestone 6

```txt
# graph definition
6 6
0 3 1
1 3 1
2 3 1
3 4 2
4 5 1
3 5 5

# travelers
3
0 5
1 5
2 5
```

This input makes three travelers try to enter node `3`.

Expected behavior:

```txt
Only one traveler enters node 3.
The other travelers wait outside.
Each traveler stays inside node 3 for one second.
Then the next traveler enters.
```

---

# Milestone 7 — Scheduling Algorithms

## Goal

Milestone 7 replaces random node entry order with scheduling algorithms.

When multiple travelers wait outside the same node, the parent process decides who enters next according to the selected scheduling algorithm.

The scheduling algorithm is selected from the command line.

## Supported Scheduling Algorithms

The project implements two scheduling algorithms:

```txt
FCFS
SJF
```

---

## FCFS — First Come First Served

FCFS means:

```txt
The traveler who requested the node first enters first.
```

The parent assigns a sequence number to every request.
The request with the smallest sequence number is selected first.

---

## SJF — Shortest Job First

SJF means:

```txt
The traveler with the shortest next movement time enters first.
```

In this project, the job length is based on the next edge travel time.

Since edge movement time is:

```txt
edge weight * 300ms
```

A traveler whose next edge has smaller weight has a shorter job.

If two travelers have the same job length, FCFS order is used as a tie breaker.

---

## Parent Responsibilities in Milestone 7

The parent process:

* Maintains a waiting queue for each node
* Receives node entry requests from children
* Selects a traveler according to the chosen scheduler
* Sends permission to the selected child
* Updates the GUI
* Prints scheduler decisions and traveler logs

---

## Child Responsibilities in Milestone 7

Each child process:

* Computes its own Dijkstra path
* Sends a request before entering a node
* Waits for permission from the parent
* Enters the node only after permission is granted
* Stays inside the node for one second
* Releases the node
* Continues to the next edge

---

## Run Milestone 7

Compile:

```bash
make milestone7
```

Run with FCFS:

```bash
./sim -schd fcfs input7.txt
```

Run with SJF:

```bash
./sim -schd sjf input7.txt
```

---

## Test Input for Milestone 7

```txt
# graph definition
6 5
0 3 1
1 3 1
2 3 1
3 4 9
3 5 1

# travelers
3
0 4
1 5
2 5
```

This input creates competition at node `3`.

Traveler going from `3 -> 4` has a longer job because the edge weight is `9`.

Travelers going from `3 -> 5` have shorter jobs because the edge weight is `1`.

---

## Difference Between FCFS and SJF

With FCFS:

```txt
Travelers enter the node based on arrival order.
```

With SJF:

```txt
Travelers with shorter next movement time are preferred.
```

In the test input, SJF should prefer travelers whose next edge is:

```txt
3 -> 5
```

because its weight is `1`, while:

```txt
3 -> 4
```

has weight `9`.

This demonstrates the effect of changing the scheduling algorithm without changing the code.

---

# Makefile

The Makefile should include these targets:

```makefile
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
```

Important: command lines under each target must begin with a real TAB character, not spaces.

---

# Final Submission

The final submission includes all seven milestones.

Before submission, run:

```bash
make clean
make milestone1
./dijkstra input.txt

make clean
make milestone4
./sim input4.txt

make clean
make milestone5
./sim input5.txt

make clean
make milestone6
./sim input6.txt

make clean
make milestone7
./sim -schd fcfs input7.txt
./sim -schd sjf input7.txt
```

---

# Git Tags

Recommended tags:

```bash
git tag 1mailstone
git tag 3mailstone
git tag 4milestone
git tag 5milestone
git tag 6milestone
git tag final
```

Final submission tag:

```bash
git tag final
git push origin final
```

---

# Final Notes

This project demonstrates:

* Graph representation
* File parsing
* Dijkstra shortest path
* GUI drawing with raylib
* Animation
* Multiple processes with fork()
* Parent-child process management
* IPC using pipe()
* Synchronization using semaphores
* Scheduling using FCFS and SJF
* GUI visualization of process states
* Waiting queues for graph nodes
