#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define INF 1000000000

int **createGraph(int n) {
    int **graph = malloc(n * sizeof(int *));
    if (graph == NULL) {
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        graph[i] = malloc(n * sizeof(int));
        if (graph[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(graph[j]);
            }
            free(graph);
            return NULL;
        }

        for (int j = 0; j < n; j++) {
            graph[i][j] = INF;
        }
    }

    return graph;
}

void freeGraph(int **graph, int n) {
    if (graph == NULL) {
        return;
    }

    for (int i = 0; i < n; i++) {
        free(graph[i]);
    }

    free(graph);
}

int findMinDistanceNode(int *dist, int *visited, int n) {
    int minDistance = INF;
    int minIndex = -1;

    for (int i = 0; i < n; i++) {
        if (!visited[i] && dist[i] < minDistance) {
            minDistance = dist[i];
            minIndex = i;
        }
    }

    return minIndex;
}

void printPath(int *parent, int src, int dst) {
    int path[1000];
    int count = 0;

    int current = dst;

    while (current != -1) {
        path[count++] = current;

        if (current == src) {
            break;
        }

        current = parent[current];
    }

    for (int i = count - 1; i >= 0; i--) {
        printf("%d", path[i]);

        if (i > 0) {
            printf(" -> ");
        }
    }

    printf("\n");
}

void dijkstra(int **graph, int n, int src, int dst) {
    int *dist = malloc(n * sizeof(int));
    int *visited = malloc(n * sizeof(int));
    int *parent = malloc(n * sizeof(int));

    if (dist == NULL || visited == NULL || parent == NULL) {
        printf("Memory allocation failed\n");
        free(dist);
        free(visited);
        free(parent);
        return;
    }

    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        visited[i] = 0;
        parent[i] = -1;
    }

    dist[src] = 0;

    for (int i = 0; i < n; i++) {
        int u = findMinDistanceNode(dist, visited, n);

        if (u == -1) {
            break;
        }

        visited[u] = 1;

        for (int v = 0; v < n; v++) {
            if (!visited[v] &&
                graph[u][v] != INF &&
                dist[u] != INF &&
                dist[u] + graph[u][v] < dist[v]) {

                dist[v] = dist[u] + graph[u][v];
                parent[v] = u;
            }
        }
    }

    if (dist[dst] == INF) {
        printf("No path found\n");
    } else {
        printPath(parent, src, dst);
        printf("%d\n", dist[dst]);
    }

    free(dist);
    free(visited);
    free(parent);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        printf("Could not open file\n");
        return 1;
    }

    int n, m;

    if (fscanf(file, "%d %d", &n, &m) != 2) {
        printf("Invalid input\n");
        fclose(file);
        return 1;
    }

    if (n <= 0 || m < 0) {
        printf("Invalid input\n");
        fclose(file);
        return 1;
    }

    int **graph = createGraph(n);
    if (graph == NULL) {
        printf("Memory allocation failed\n");
        fclose(file);
        return 1;
    }

    for (int i = 0; i < m; i++) {
        int src, dst, weight;

        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid input\n");
            freeGraph(graph, n);
            fclose(file);
            return 1;
        }

        if (src < 0 || src >= n || dst < 0 || dst >= n || weight < 0) {
            printf("Invalid input\n");
            freeGraph(graph, n);
            fclose(file);
            return 1;
        }

        /*
         * Directed graph:
         * src -> dst only.
         * If there are duplicate edges, keep the smaller weight.
         */
        if (weight < graph[src][dst]) {
            graph[src][dst] = weight;
        }
    }

    int querySrc, queryDst;

    if (fscanf(file, "%d %d", &querySrc, &queryDst) != 2) {
        printf("Invalid input\n");
        freeGraph(graph, n);
        fclose(file);
        return 1;
    }

    if (querySrc < 0 || querySrc >= n || queryDst < 0 || queryDst >= n) {
        printf("Invalid input\n");
        freeGraph(graph, n);
        fclose(file);
        return 1;
    }

    if (querySrc == queryDst) {
        printf("%d\n", querySrc);
        printf("0\n");
    } else {
        dijkstra(graph, n, querySrc, queryDst);
    }

    freeGraph(graph, n);
    fclose(file);

    return 0;
}