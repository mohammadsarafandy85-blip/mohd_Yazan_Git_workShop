#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "raylib.h"

#define INF 1000000000
#define MAX_NODES 15
#define MAX_EDGES 300

typedef struct {
    int src;
    int dst;
    int weight;
} Edge;

typedef struct {
    float x;
    float y;
} Point;

typedef struct {
    int n;
    int m;
    int **matrix;
    Edge edges[MAX_EDGES];
    int querySrc;
    int queryDst;
} Graph;

int **createMatrix(int n) {
    int **matrix = malloc(n * sizeof(int *));
    if (matrix == NULL) {
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        matrix[i] = malloc(n * sizeof(int));
        if (matrix[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(matrix[j]);
            }
            free(matrix);
            return NULL;
        }

        for (int j = 0; j < n; j++) {
            matrix[i][j] = INF;
        }
    }

    return matrix;
}

void freeMatrix(int **matrix, int n) {
    if (matrix == NULL) {
        return;
    }

    for (int i = 0; i < n; i++) {
        free(matrix[i]);
    }

    free(matrix);
}

int readGraphFromFile(const char *filename, Graph *graph) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Could not open file\n");
        return 0;
    }

    if (fscanf(file, "%d %d", &graph->n, &graph->m) != 2) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (graph->n <= 0 || graph->n > MAX_NODES || graph->m < 0 || graph->m > MAX_EDGES) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    graph->matrix = createMatrix(graph->n);
    if (graph->matrix == NULL) {
        printf("Memory allocation failed\n");
        fclose(file);
        return 0;
    }

    for (int i = 0; i < graph->m; i++) {
        int src, dst, weight;

        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid input\n");
            freeMatrix(graph->matrix, graph->n);
            fclose(file);
            return 0;
        }

        if (src < 0 || src >= graph->n || dst < 0 || dst >= graph->n || weight < 0) {
            printf("Invalid input\n");
            freeMatrix(graph->matrix, graph->n);
            fclose(file);
            return 0;
        }

        graph->edges[i].src = src;
        graph->edges[i].dst = dst;
        graph->edges[i].weight = weight;

        if (weight < graph->matrix[src][dst]) {
            graph->matrix[src][dst] = weight;
        }
    }

    if (fscanf(file, "%d %d", &graph->querySrc, &graph->queryDst) != 2) {
        printf("Invalid input\n");
        freeMatrix(graph->matrix, graph->n);
        fclose(file);
        return 0;
    }

    if (graph->querySrc < 0 || graph->querySrc >= graph->n ||
        graph->queryDst < 0 || graph->queryDst >= graph->n) {
        printf("Invalid input\n");
        freeMatrix(graph->matrix, graph->n);
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
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

void dijkstra(int **matrix, int n, int src, int dst) {
    if (src == dst) {
        printf("%d\n", src);
        printf("0\n");
        return;
    }

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
                matrix[u][v] != INF &&
                dist[u] != INF &&
                dist[u] + matrix[u][v] < dist[v]) {

                dist[v] = dist[u] + matrix[u][v];
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

void calculateNodePositions(Point positions[], int n, int screenWidth, int screenHeight) {
    float centerX = screenWidth / 2.0f;
    float centerY = screenHeight / 2.0f;
    float radius = 240.0f;

    for (int i = 0; i < n; i++) {
        float angle = (2.0f * PI * i) / n;
        positions[i].x = centerX + radius * cosf(angle);
        positions[i].y = centerY + radius * sinf(angle);
    }
}

void drawArrow(Point start, Point end) {
    float nodeRadius = 25.0f;

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = sqrtf(dx * dx + dy * dy);

    if (length == 0) {
        return;
    }

    float unitX = dx / length;
    float unitY = dy / length;

    Point lineStart;
    Point lineEnd;

    lineStart.x = start.x + unitX * nodeRadius;
    lineStart.y = start.y + unitY * nodeRadius;

    lineEnd.x = end.x - unitX * nodeRadius;
    lineEnd.y = end.y - unitY * nodeRadius;

    DrawLineEx(
        (Vector2){lineStart.x, lineStart.y},
        (Vector2){lineEnd.x, lineEnd.y},
        2.0f,
        DARKGRAY
    );

    float arrowSize = 14.0f;
    float angle = atan2f(dy, dx);

    Point arrow1;
    Point arrow2;

    arrow1.x = lineEnd.x - arrowSize * cosf(angle - PI / 6);
    arrow1.y = lineEnd.y - arrowSize * sinf(angle - PI / 6);

    arrow2.x = lineEnd.x - arrowSize * cosf(angle + PI / 6);
    arrow2.y = lineEnd.y - arrowSize * sinf(angle + PI / 6);

    DrawTriangle(
        (Vector2){lineEnd.x, lineEnd.y},
        (Vector2){arrow1.x, arrow1.y},
        (Vector2){arrow2.x, arrow2.y},
        DARKGRAY
    );
}

void drawGraph(Graph *graph, Point positions[]) {
    for (int i = 0; i < graph->m; i++) {
        int srcIndex = graph->edges[i].src;
        int dstIndex = graph->edges[i].dst;

        Point src = positions[srcIndex];
        Point dst = positions[dstIndex];

        drawArrow(src, dst);

        float midX = (src.x + dst.x) / 2.0f;
        float midY = (src.y + dst.y) / 2.0f;

        char weightText[20];
        sprintf(weightText, "%d", graph->edges[i].weight);

        DrawRectangle((int)midX - 15, (int)midY - 12, 35, 24, RAYWHITE);
        DrawText(weightText, (int)midX - 8, (int)midY - 9, 18, RED);
    }

    for (int i = 0; i < graph->n; i++) {
        DrawCircle((int)positions[i].x, (int)positions[i].y, 25, SKYBLUE);
        DrawCircleLines((int)positions[i].x, (int)positions[i].y, 25, BLUE);

        char nodeText[20];
        sprintf(nodeText, "%d", i);

        int textWidth = MeasureText(nodeText, 22);

        DrawText(
            nodeText,
            (int)positions[i].x - textWidth / 2,
            (int)positions[i].y - 11,
            22,
            BLACK
        );
    }
}

int main(int argc, char *argv[]) {
    const char *filename = "graph.txt";

    if (argc == 2) {
        filename = argv[1];
    }

    Graph graph;
    graph.matrix = NULL;

    if (!readGraphFromFile(filename, &graph)) {
        return 1;
    }

    dijkstra(graph.matrix, graph.n, graph.querySrc, graph.queryDst);

    int screenWidth = 900;
    int screenHeight = 700;

    Point positions[MAX_NODES];
    calculateNodePositions(positions, graph.n, screenWidth, screenHeight);

    InitWindow(screenWidth, screenHeight, "Milestone 2 - Directed Weighted Graph");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("Directed Weighted Graph", 20, 20, 28, BLACK);

        char queryText[100];
        sprintf(queryText, "Dijkstra query: %d -> %d", graph.querySrc, graph.queryDst);
        DrawText(queryText, 20, 55, 22, DARKBLUE);

        drawGraph(&graph, positions);

        EndDrawing();
    }

    CloseWindow();

    freeMatrix(graph.matrix, graph.n);

    return 0;
}