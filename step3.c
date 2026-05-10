#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define MAX_N 15
#define MAX_M 300
#define INF 1000000000

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 750

#define NODE_RADIUS 25.0f
#define JUMP_TIME 0.3f
#define WAIT_TIME 1.0f

typedef struct {
    int src;
    int dst;
    int weight;
} Edge;

typedef struct {
    int n;
    int m;
    Edge edges[MAX_M];
    int graph[MAX_N][MAX_N];
    int querySrc;
    int queryDst;
} GraphData;

typedef struct {
    int dist[MAX_N];
    int parent[MAX_N];
    int found;
    int path[MAX_N];
    int pathLength;
} DijkstraResult;

typedef struct {
    int isPlaying;
    int isFinished;

    int pathIndex;
    int currentJump;
    int totalJumps;

    float timer;
    float waitTimer;
    int isWaiting;

    Vector2 position;
} AnimationState;

void initGraph(GraphData *data) {
    data->n = 0;
    data->m = 0;
    data->querySrc = 0;
    data->queryDst = 0;

    for (int i = 0; i < MAX_N; i++) {
        for (int j = 0; j < MAX_N; j++) {
            data->graph[i][j] = INF;
        }
    }
}

int loadGraphFromFile(const char *filename, GraphData *data) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Could not open file\n");
        return 0;
    }

    if (fscanf(file, "%d %d", &data->n, &data->m) != 2) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (data->n <= 0 || data->n > MAX_N || data->m < 0 || data->m > MAX_M) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    for (int i = 0; i < data->m; i++) {
        int src, dst, weight;

        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        if (src < 0 || src >= data->n || dst < 0 || dst >= data->n || weight < 0) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        data->edges[i].src = src;
        data->edges[i].dst = dst;
        data->edges[i].weight = weight;

        if (weight < data->graph[src][dst]) {
            data->graph[src][dst] = weight;
        }
    }

    if (fscanf(file, "%d %d", &data->querySrc, &data->queryDst) != 2) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (data->querySrc < 0 || data->querySrc >= data->n ||
        data->queryDst < 0 || data->queryDst >= data->n) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

int findMinDistanceNode(int dist[], int visited[], int n) {
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

void buildPath(DijkstraResult *result, int src, int dst) {
    int reversed[MAX_N];
    int count = 0;

    int current = dst;

    while (current != -1 && count < MAX_N) {
        reversed[count++] = current;

        if (current == src) {
            break;
        }

        current = result->parent[current];
    }

    result->pathLength = 0;

    for (int i = count - 1; i >= 0; i--) {
        result->path[result->pathLength++] = reversed[i];
    }
}

DijkstraResult dijkstra(GraphData *data) {
    DijkstraResult result;
    int visited[MAX_N];

    result.found = 0;
    result.pathLength = 0;

    for (int i = 0; i < data->n; i++) {
        result.dist[i] = INF;
        result.parent[i] = -1;
        visited[i] = 0;
    }

    result.dist[data->querySrc] = 0;

    for (int i = 0; i < data->n; i++) {
        int u = findMinDistanceNode(result.dist, visited, data->n);

        if (u == -1) {
            break;
        }

        visited[u] = 1;

        for (int v = 0; v < data->n; v++) {
            if (!visited[v] &&
                data->graph[u][v] != INF &&
                result.dist[u] != INF &&
                result.dist[u] + data->graph[u][v] < result.dist[v]) {

                result.dist[v] = result.dist[u] + data->graph[u][v];
                result.parent[v] = u;
            }
        }
    }

    if (result.dist[data->queryDst] != INF) {
        result.found = 1;
        buildPath(&result, data->querySrc, data->queryDst);
    }

    return result;
}

void printDijkstraResult(GraphData *data, DijkstraResult *result) {
    if (data->querySrc == data->queryDst) {
        printf("%d\n", data->querySrc);
        printf("0\n");
        return;
    }

    if (!result->found) {
        printf("No path found\n");
        return;
    }

    for (int i = 0; i < result->pathLength; i++) {
        printf("%d", result->path[i]);

        if (i < result->pathLength - 1) {
            printf(" -> ");
        }
    }

    printf("\n");
    printf("%d\n", result->dist[data->queryDst]);
}

int isEdgeInShortestPath(DijkstraResult *result, int src, int dst) {
    for (int i = 0; i < result->pathLength - 1; i++) {
        if (result->path[i] == src && result->path[i + 1] == dst) {
            return 1;
        }
    }

    return 0;
}

void calculateNodePositions(Vector2 positions[], int n) {
    float centerX = SCREEN_WIDTH / 2.0f;
    float centerY = SCREEN_HEIGHT / 2.0f + 40.0f;
    float radius = 250.0f;

    if (n <= 5) {
        radius = 180.0f;
    }

    for (int i = 0; i < n; i++) {
        float angle = (2.0f * PI * i) / n - PI / 2.0f;

        positions[i].x = centerX + radius * cosf(angle);
        positions[i].y = centerY + radius * sinf(angle);
    }
}

void drawArrow(Vector2 start, Vector2 end, Color color, float thickness) {
    Vector2 direction = {
        end.x - start.x,
        end.y - start.y
    };

    float length = sqrtf(direction.x * direction.x + direction.y * direction.y);

    if (length == 0) {
        return;
    }

    direction.x /= length;
    direction.y /= length;

    Vector2 arrowStart = {
        start.x + direction.x * NODE_RADIUS,
        start.y + direction.y * NODE_RADIUS
    };

    Vector2 arrowEnd = {
        end.x - direction.x * NODE_RADIUS,
        end.y - direction.y * NODE_RADIUS
    };

    DrawLineEx(arrowStart, arrowEnd, thickness, color);

    float arrowSize = 12.0f;

    Vector2 left = {
        arrowEnd.x - direction.x * arrowSize - direction.y * arrowSize * 0.6f,
        arrowEnd.y - direction.y * arrowSize + direction.x * arrowSize * 0.6f
    };

    Vector2 right = {
        arrowEnd.x - direction.x * arrowSize + direction.y * arrowSize * 0.6f,
        arrowEnd.y - direction.y * arrowSize - direction.x * arrowSize * 0.6f
    };

    DrawTriangle(arrowEnd, left, right, color);
}

void drawGraph(GraphData *data, DijkstraResult *result, Vector2 positions[]) {
    for (int i = 0; i < data->m; i++) {
        int src = data->edges[i].src;
        int dst = data->edges[i].dst;
        int weight = data->edges[i].weight;

        Color edgeColor = DARKGRAY;
        float thickness = 2.0f;

        if (result->found && isEdgeInShortestPath(result, src, dst)) {
            edgeColor = RED;
            thickness = 4.0f;
        }

        Vector2 start = positions[src];
        Vector2 end = positions[dst];

        drawArrow(start, end, edgeColor, thickness);

        int midX = (int)((start.x + end.x) / 2.0f);
        int midY = (int)((start.y + end.y) / 2.0f);

        char weightText[20];
        sprintf(weightText, "%d", weight);

        DrawCircle(midX, midY, 14, RAYWHITE);
        DrawCircleLines(midX, midY, 14, GRAY);
        DrawText(weightText, midX - MeasureText(weightText, 18) / 2, midY - 9, 18, BLACK);
    }

    for (int i = 0; i < data->n; i++) {
        Color nodeColor = SKYBLUE;

        if (i == data->querySrc) {
            nodeColor = GREEN;
        } else if (i == data->queryDst) {
            nodeColor = ORANGE;
        }

        DrawCircleV(positions[i], NODE_RADIUS, nodeColor);
        DrawCircleLines((int)positions[i].x, (int)positions[i].y, NODE_RADIUS, BLACK);

        char nodeText[20];
        sprintf(nodeText, "%d", i);

        int textWidth = MeasureText(nodeText, 22);
        DrawText(nodeText,
                 (int)positions[i].x - textWidth / 2,
                 (int)positions[i].y - 11,
                 22,
                 BLACK);
    }
}

void initAnimation(AnimationState *anim, GraphData *data, DijkstraResult *result, Vector2 positions[]) {
    anim->isPlaying = 0;
    anim->isFinished = 0;
    anim->pathIndex = 0;
    anim->currentJump = 0;
    anim->timer = 0.0f;
    anim->waitTimer = 0.0f;
    anim->isWaiting = 0;

    anim->position = positions[data->querySrc];

    if (result->found && result->pathLength > 1) {
        int from = result->path[0];
        int to = result->path[1];
        anim->totalJumps = data->graph[from][to];

        if (anim->totalJumps <= 0) {
            anim->totalJumps = 1;
        }
    } else {
        anim->totalJumps = 1;
        anim->isFinished = 1;
    }
}

void resetAnimation(AnimationState *anim, GraphData *data, DijkstraResult *result, Vector2 positions[]) {
    initAnimation(anim, data, result, positions);
}

void updateAnimation(AnimationState *anim, GraphData *data, DijkstraResult *result, Vector2 positions[], float deltaTime) {
    if (!anim->isPlaying || anim->isFinished || !result->found || result->pathLength <= 1) {
        return;
    }

    if (anim->isWaiting) {
        anim->waitTimer += deltaTime;

        if (anim->waitTimer >= WAIT_TIME) {
            anim->isWaiting = 0;
            anim->waitTimer = 0.0f;
        }

        return;
    }

    int fromNode = result->path[anim->pathIndex];
    int toNode = result->path[anim->pathIndex + 1];

    Vector2 start = positions[fromNode];
    Vector2 end = positions[toNode];

    anim->timer += deltaTime;

    if (anim->timer >= JUMP_TIME) {
        anim->timer = 0.0f;
        anim->currentJump++;

        float t = (float)anim->currentJump / (float)anim->totalJumps;

        if (t > 1.0f) {
            t = 1.0f;
        }

        anim->position.x = start.x + (end.x - start.x) * t;
        anim->position.y = start.y + (end.y - start.y) * t;

        if (anim->currentJump >= anim->totalJumps) {
            anim->position = end;
            anim->pathIndex++;

            if (anim->pathIndex >= result->pathLength - 1) {
                anim->isFinished = 1;
                anim->isPlaying = 0;
                return;
            }

            fromNode = result->path[anim->pathIndex];
            toNode = result->path[anim->pathIndex + 1];

            anim->totalJumps = data->graph[fromNode][toNode];

            if (anim->totalJumps <= 0) {
                anim->totalJumps = 1;
            }

            anim->currentJump = 0;

            if (anim->pathIndex > 0 && anim->pathIndex < result->pathLength - 1) {
                anim->isWaiting = 1;
                anim->waitTimer = 0.0f;
            }
        }
    }
}

int isMouseOnButton(Rectangle button) {
    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, button);
}

void drawButton(Rectangle button, const char *text) {
    Color color = LIGHTGRAY;

    if (isMouseOnButton(button)) {
        color = GRAY;
    }

    DrawRectangleRec(button, color);
    DrawRectangleLines((int)button.x, (int)button.y, (int)button.width, (int)button.height, BLACK);

    int textWidth = MeasureText(text, 22);
    DrawText(text,
             (int)(button.x + button.width / 2 - textWidth / 2),
             (int)(button.y + button.height / 2 - 11),
             22,
             BLACK);
}

void drawShortestPathText(DijkstraResult *result, int x, int y) {
    if (!result->found) {
        DrawText("Path: No path found", x, y, 20, RED);
        return;
    }

    char text[256] = "Path: ";
    char temp[20];

    for (int i = 0; i < result->pathLength; i++) {
        sprintf(temp, "%d", result->path[i]);
        strcat(text, temp);

        if (i < result->pathLength - 1) {
            strcat(text, " -> ");
        }
    }

    DrawText(text, x, y, 20, BLACK);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    GraphData data;
    initGraph(&data);

    if (!loadGraphFromFile(argv[1], &data)) {
        return 1;
    }

    DijkstraResult result = dijkstra(&data);
    printDijkstraResult(&data, &result);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Milestone 3 - Graph Movement Simulation");
    SetTargetFPS(60);

    Vector2 positions[MAX_N];
    calculateNodePositions(positions, data.n);

    AnimationState anim;
    initAnimation(&anim, &data, &result, positions);

    Rectangle playButton = {30, 155, 120, 45};
    Rectangle resetButton = {170, 155, 120, 45};

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (isMouseOnButton(playButton)) {
                if (!anim.isFinished && result.found && result.pathLength > 1) {
                    anim.isPlaying = !anim.isPlaying;
                }
            }

            if (isMouseOnButton(resetButton)) {
                resetAnimation(&anim, &data, &result, positions);
            }
        }

        updateAnimation(&anim, &data, &result, positions, deltaTime);

        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("Milestone 3 - Movement Animation on Graph", 30, 20, 28, DARKBLUE);
        DrawText("Green = Source, Orange = Destination, Red = Shortest Path", 30, 60, 20, DARKGRAY);

        char queryText[100];
        sprintf(queryText, "Dijkstra: %d -> %d", data.querySrc, data.queryDst);
        DrawText(queryText, 30, 90, 20, BLACK);

        if (data.querySrc == data.queryDst) {
            DrawText("Shortest path cost: 0", 30, 120, 20, GREEN);
        } else if (result.found) {
            char distText[100];
            sprintf(distText, "Shortest path cost: %d", result.dist[data.queryDst]);
            DrawText(distText, 30, 120, 20, GREEN);
        } else {
            DrawText("No path found", 30, 120, 20, RED);
        }

        drawButton(playButton, anim.isPlaying ? "Stop" : "Play");
        drawButton(resetButton, "Reset");

        drawShortestPathText(&result, 320, 165);

        drawGraph(&data, &result, positions);

        if (result.found) {
            DrawCircleV(anim.position, 12, PURPLE);
            DrawCircleLines((int)anim.position.x, (int)anim.position.y, 12, BLACK);
        }

        if (anim.isWaiting) {
            DrawText("Waiting 1 second at node...", 30, 215, 20, BLUE);
        }

        if (anim.isFinished && result.found) {
            DrawText("Arrived at destination!", 30, 245, 24, GREEN);
        }

        if (!result.found) {
            DrawText("Animation disabled because no path exists.", 30, 245, 22, RED);
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
