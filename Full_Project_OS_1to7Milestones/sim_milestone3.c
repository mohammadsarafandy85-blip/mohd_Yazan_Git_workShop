#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "raylib.h"


#define MAX_NODES 15
#define MAX_EDGES 100

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700
#define NODE_RADIUS 25

#define MOVING_RADIUS 12
#define JUMP_TIME 0.3f
#define WAIT_TIME 1.0f

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

typedef struct Animation {
    int edgeIndex;
    int step;
    float timer;
    int playing;
    int waiting;
    int arrived;
} Animation;

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

    if (g->n <= 0 || g->n > MAX_NODES || g->m < 0 || g->m > MAX_EDGES) {
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

int dijkstra(Graph *g, int start, int target, int path[], int pathWeights[],
             int *pathLength, int *totalCost) {
    int dist[MAX_NODES];
    int visited[MAX_NODES];
    int parent[MAX_NODES];
    int parentWeight[MAX_NODES];

    for (int i = 0; i < g->n; i++) {
        dist[i] = INT_MAX;
        visited[i] = 0;
        parent[i] = -1;
        parentWeight[i] = 0;
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
                    parentWeight[v] = weight;
                }
            }
        }
    }

    if (dist[target] == INT_MAX) {
        *pathLength = 0;
        *totalCost = 0;
        return 0;
    }

    int reversedPath[MAX_NODES];
    int reversedWeights[MAX_NODES];
    int count = 0;

    int current = target;

    while (current != -1) {
        reversedPath[count] = current;
        reversedWeights[count] = parentWeight[current];
        count++;
        current = parent[current];
    }

    for (int i = 0; i < count; i++) {
        path[i] = reversedPath[count - 1 - i];
    }

    for (int i = 0; i < count - 1; i++) {
        pathWeights[i] = reversedWeights[count - 2 - i];
    }

    *pathLength = count;
    *totalCost = dist[target];

    return 1;
}

void calculateNodePositions(int n, Vector2 positions[]) {
    float centerX = SCREEN_WIDTH / 2.0f;
    float centerY = SCREEN_HEIGHT / 2.0f;
    float radius = 230.0f;

    for (int i = 0; i < n; i++) {
        float angle = (2.0f * PI * i) / n - PI / 2.0f;

        positions[i].x = centerX + radius * cosf(angle);
        positions[i].y = centerY + radius * sinf(angle);
    }
}

void drawArrow(Vector2 from, Vector2 to, int weight, Color color, float thickness) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float length = sqrtf(dx * dx + dy * dy);

    if (length < 1.0f) {
        return;
    }

    float ux = dx / length;
    float uy = dy / length;

    Vector2 start = {
        from.x + ux * NODE_RADIUS,
        from.y + uy * NODE_RADIUS
    };

    Vector2 end = {
        to.x - ux * NODE_RADIUS,
        to.y - uy * NODE_RADIUS
    };

    DrawLineEx(start, end, thickness, color);

    float arrowSize = 14.0f;

    Vector2 left = {
        end.x - ux * arrowSize - uy * arrowSize * 0.6f,
        end.y - uy * arrowSize + ux * arrowSize * 0.6f
    };

    Vector2 right = {
        end.x - ux * arrowSize + uy * arrowSize * 0.6f,
        end.y - uy * arrowSize - ux * arrowSize * 0.6f
    };

    DrawTriangle(end, left, right, color);

    Vector2 middle = {
        (start.x + end.x) / 2.0f - uy * 15.0f,
        (start.y + end.y) / 2.0f + ux * 15.0f
    };

    DrawText(TextFormat("%d", weight), (int)middle.x, (int)middle.y, 18, BLACK);
}

void drawEdges(Graph *g, Vector2 positions[]) {
    for (int i = 0; i < g->m; i++) {
        int src = g->edges[i].src;
        int dst = g->edges[i].dst;
        int weight = g->edges[i].weight;

        drawArrow(positions[src], positions[dst], weight, GRAY, 2.0f);
    }
}

void drawShortestPath(Vector2 positions[], int path[], int pathWeights[], int pathLength) {
    for (int i = 0; i < pathLength - 1; i++) {
        int src = path[i];
        int dst = path[i + 1];
        int weight = pathWeights[i];

        drawArrow(positions[src], positions[dst], weight, RED, 4.0f);
    }
}

void drawNodes(Graph *g, Vector2 positions[], int start, int target) {
    for (int i = 0; i < g->n; i++) {
        Color color = RAYWHITE;

        if (i == start) {
            color = GREEN;
        } else if (i == target) {
            color = SKYBLUE;
        }

        DrawCircleV(positions[i], NODE_RADIUS, color);
        DrawCircleLines((int)positions[i].x, (int)positions[i].y, NODE_RADIUS, BLACK);

        const char *text = TextFormat("%d", i);
        int textWidth = MeasureText(text, 20);

        DrawText(
            text,
            (int)(positions[i].x - textWidth / 2),
            (int)(positions[i].y - 10),
            20,
            BLACK
        );
    }
}

void drawPathText(int path[], int pathLength, int totalCost) {
    char text[256];
    int offset = 0;

    offset += snprintf(text + offset, sizeof(text) - offset, "Shortest path: ");

    for (int i = 0; i < pathLength; i++) {
        offset += snprintf(text + offset, sizeof(text) - offset, "%d", path[i]);

        if (i < pathLength - 1) {
            offset += snprintf(text + offset, sizeof(text) - offset, " -> ");
        }
    }

    snprintf(text + offset, sizeof(text) - offset, " | Cost: %d", totalCost);

    DrawText(text, 20, 90, 20, DARKGRAY);
}

void resetAnimation(Animation *anim) {
    anim->edgeIndex = 0;
    anim->step = 0;
    anim->timer = 0.0f;
    anim->playing = 0;
    anim->waiting = 0;
    anim->arrived = 0;
}

void advanceToNextNode(Animation *anim, int pathLength) {
    anim->edgeIndex++;
    anim->step = 0;
    anim->timer = 0.0f;

    if (anim->edgeIndex >= pathLength - 1) {
        anim->arrived = 1;
        anim->playing = 0;
        anim->waiting = 0;
    } else {
        anim->waiting = 1;
    }
}

void updateAnimation(Animation *anim, int pathLength, int pathWeights[], float deltaTime) {
    if (!anim->playing || anim->arrived || pathLength < 2) {
        return;
    }

    if (anim->waiting) {
        anim->timer += deltaTime;

        if (anim->timer >= WAIT_TIME) {
            anim->waiting = 0;
            anim->timer = 0.0f;
            anim->step = 0;
        }

        return;
    }

    int stepsOnEdge = pathWeights[anim->edgeIndex];

    if (stepsOnEdge <= 0) {
        advanceToNextNode(anim, pathLength);
        return;
    }

    anim->timer += deltaTime;

    while (anim->timer >= JUMP_TIME) {
        anim->timer -= JUMP_TIME;
        anim->step++;

        if (anim->step >= stepsOnEdge) {
            anim->step = stepsOnEdge;
            advanceToNextNode(anim, pathLength);
            break;
        }
    }
}

Vector2 getMovingObjectPosition(Animation *anim, Vector2 positions[],
                                int path[], int pathLength, int pathWeights[]) {
    if (pathLength == 0) {
        Vector2 empty = {0, 0};
        return empty;
    }

    if (anim->arrived || anim->edgeIndex >= pathLength - 1) {
        return positions[path[pathLength - 1]];
    }

    if (anim->waiting) {
        return positions[path[anim->edgeIndex]];
    }

    int from = path[anim->edgeIndex];
    int to = path[anim->edgeIndex + 1];
    int stepsOnEdge = pathWeights[anim->edgeIndex];

    if (stepsOnEdge <= 0) {
        return positions[to];
    }

    float t = (float)anim->step / stepsOnEdge;

    Vector2 pos = {
        positions[from].x + (positions[to].x - positions[from].x) * t,
        positions[from].y + (positions[to].y - positions[from].y) * t
    };

    return pos;
}

void drawButton(Rectangle button, const char *text) {
    DrawRectangleRec(button, LIGHTGRAY);
    DrawRectangleLinesEx(button, 2, DARKGRAY);

    int textWidth = MeasureText(text, 20);

    DrawText(
        text,
        (int)(button.x + button.width / 2 - textWidth / 2),
        (int)(button.y + button.height / 2 - 10),
        20,
        BLACK
    );
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

    int path[MAX_NODES];
    int pathWeights[MAX_NODES];
    int pathLength = 0;
    int totalCost = 0;

    int hasPath = dijkstra(&g, start, target, path, pathWeights, &pathLength, &totalCost);

    Vector2 positions[MAX_NODES];
    calculateNodePositions(g.n, positions);

    Animation anim;
    resetAnimation(&anim);

    if (hasPath && pathLength <= 1) {
        anim.arrived = 1;
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Simulation");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        Rectangle playButton = {20, 125, 120, 45};
        Vector2 mouse = GetMousePosition();

        if (hasPath &&
            CheckCollisionPointRec(mouse, playButton) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

            if (anim.arrived) {
                resetAnimation(&anim);

                if (pathLength <= 1) {
                    anim.arrived = 1;
                }
            } else {
                anim.playing = !anim.playing;
            }
        }

        if (hasPath && IsKeyPressed(KEY_R)) {
            resetAnimation(&anim);

            if (pathLength <= 1) {
                anim.arrived = 1;
            }
        }

        if (hasPath) {
            updateAnimation(&anim, pathLength, pathWeights, deltaTime);
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("Milestone 3 - Graph Animation", 20, 20, 28, BLACK);
        DrawText("Gray = Graph edges | Red = Shortest path", 20, 60, 20, DARKGRAY);

        if (hasPath) {
            drawPathText(path, pathLength, totalCost);
        } else {
            DrawText("No path found", 20, 90, 24, RED);
        }

        drawEdges(&g, positions);

        if (hasPath) {
            drawShortestPath(positions, path, pathWeights, pathLength);
        }

        drawNodes(&g, positions, start, target);

        if (hasPath) {
            Vector2 objectPosition = getMovingObjectPosition(
                &anim,
                positions,
                path,
                pathLength,
                pathWeights
            );

            DrawCircleV(objectPosition, MOVING_RADIUS, ORANGE);
            DrawCircleLines(
                (int)objectPosition.x,
                (int)objectPosition.y,
                MOVING_RADIUS,
                BLACK
            );

            const char *buttonText;

            if (anim.arrived) {
                buttonText = "Restart";
            } else if (anim.playing) {
                buttonText = "Stop";
            } else {
                buttonText = "Play";
            }

            drawButton(playButton, buttonText);

            DrawText("Press R to reset", 20, 645, 18, DARKGRAY);
        }

        if (hasPath && anim.waiting) {
            DrawText("Waiting 1 second at node...", 20, 180, 20, ORANGE);
        }

        if (hasPath && anim.arrived) {
            DrawText("Arrived at target!", 20, 180, 24, GREEN);
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}