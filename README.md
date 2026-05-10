# Operating Systems Project - Graph Traffic Simulation

## Description

This project simulates movement on a directed weighted graph.

The graph is read from a text file.  
The program calculates the shortest path between a source node and a destination node using Dijkstra's algorithm.

Milestone 3 adds animation using raylib:
- The graph is displayed on screen.
- Nodes and directed weighted edges are shown.
- The shortest path is highlighted.
- A moving entity starts near the source node.
- A Play/Stop button controls the animation.
- The entity waits 1 second at each intermediate node.
- Each edge with weight W is divided into W jumps.
- Each jump takes 300 milliseconds.
- A message is displayed after reaching the destination.

## Build Commands

Milestone 1:

```bash
make milestone1
