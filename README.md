# Operating Systems Project - Graph Traffic Simulation

## Description

This project simulates travelers moving on a directed weighted graph using Dijkstra's shortest path algorithm and raylib GUI.

## Milestone 4

Milestone 4 adds multiple travelers and child processes.

Implementation details:
- The input file contains the graph and then a list of travelers.
- The parent process reads the file.
- The parent computes the Dijkstra path for each traveler.
- The parent creates a child process for each traveler using `fork()`.
- Each child prints `[PID] started` and then sleeps until the parent terminates it.
- The parent manages the raylib GUI and displays all travelers at the same time.
- Each traveler is shown using a different color.
- When a traveler reaches the destination, the parent sends a signal to terminate its child process.
- The parent waits for all child processes before exiting.

Build:

```bash
make milestone4
```

Run:

```bash
./sim input4.txt
```

## Milestone 5

Milestone 5 adds IPC between the child processes and the parent process.

Chosen IPC tool: **pipes**.

Reason:
- Pipes are simple and standard in Linux.
- Each child sends structured updates to the parent.
- The parent can read messages from all children and update the GUI.

Implementation details:
- The parent reads the graph and traveler list.
- The parent creates a pipe and a child process for each traveler.
- Each child computes Dijkstra for its own source and destination.
- Children do not print to the terminal.
- When a child reaches a node, it sends a message to the parent through a pipe.
- The parent receives messages and prints logs in this format:

```text
[PID=1021] arrived at node 0 | next node: 2
[PID=1021] arrived at node 4 | DESTINATION
[PID=1021] finished
```

Build:

```bash
make milestone5
```

Run:

```bash
./sim input5.txt
```

## Clean

```bash
make clean
```

## Input format

```text
# graph definition
N M
src dst weight
...
# travelers
T
source destination
source destination
...
```
