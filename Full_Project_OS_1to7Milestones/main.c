#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX_NODES 100
#define MAX_EDGES 1000

typedef struct Edge {
    int src;
    int dst;
    int weight;
} Edge;

typedef struct Graph {
    int n;
    int m;
    Edge edges[MAX_EDGES];
} Graph;

int readGraphFromFile(const char *fileName, Graph *g, int *start, int *target) {
    FILE *file = fopen(fileName, "r");

    if (file == NULL) {
        printf("Cannot open file\n");
        return 0;
    }

    if (fscanf(file, "%d %d", &g->n, &g->m) != 2) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (g->n <= 0 || g->m < 0 || g->m > MAX_EDGES) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    for (int i = 0; i < g->m; i++) {
        int src, dst, weight;

        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        if (src < 0 || dst < 0 || weight < 0 || src >= g->n || dst >= g->n) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        g->edges[i].src = src;
        g->edges[i].dst = dst;
        g->edges[i].weight = weight;
    }

    if (fscanf(file, "%d %d", start, target) != 2) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (*start < 0 || *target < 0 || *start >= g->n || *target >= g->n) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

int findMinDistance(int dist[], int visited[], int n) {
    int min = INT_MAX;
    int minIndex = -1;

    for (int i = 0; i < n; i++) {
        if (!visited[i] && dist[i] < min) {
            min = dist[i];
            minIndex = i;
        }
    }

    return minIndex;
}

void printPath(int parent[], int node) {
    if (parent[node] == -1) {
        printf("%d", node);
        return;
    }

    printPath(parent, parent[node]);
    printf(" -> %d", node);
}

void dijkstra(Graph *g, int start, int target) {
    int dist[MAX_NODES];
    int visited[MAX_NODES];
    int parent[MAX_NODES];

    for (int i = 0; i < g->n; i++) {
        dist[i] = INT_MAX;
        visited[i] = 0;
        parent[i] = -1;
    }

    dist[start] = 0;

    for (int i = 0; i < g->n; i++) {
        int u = findMinDistance(dist, visited, g->n);

        if (u == -1) {
            break;
        }

        visited[u] = 1;

        for (int j = 0; j < g->m; j++) {
            if (g->edges[j].src == u) {
                int v = g->edges[j].dst;
                int weight = g->edges[j].weight;

                if (!visited[v] &&
                    dist[u] != INT_MAX &&
                    dist[u] <= INT_MAX - weight &&
                    dist[u] + weight < dist[v]) {

                    dist[v] = dist[u] + weight;
                    parent[v] = u;
                }
            }
        }
    }

    if (dist[target] == INT_MAX) {
        printf("No path found\n");
    } else {
        printPath(parent, target);
        printf("\n%d\n", dist[target]);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    Graph g;
    int start, target;

    if (!readGraphFromFile(argv[1], &g, &start, &target)) {
        return 1;
    }

    dijkstra(&g, start, target);

    return 0;
}