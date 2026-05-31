#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_N 15
#define MAX_M 300
#define MAX_TRAVELERS 10
#define INF 1000000000
#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 750
#define NODE_RADIUS 25.0f
#define JUMP_TIME 0.3f
#define WAIT_TIME 1.0f

typedef struct { int src, dst, weight; } Edge;
typedef struct { int source, destination; } TravelerQuery;

typedef struct {
    int n, m;
    Edge edges[MAX_M];
    int graph[MAX_N][MAX_N];
    int travelerCount;
    TravelerQuery travelers[MAX_TRAVELERS];
} GraphData;

typedef struct {
    int found;
    int dist[MAX_N];
    int parent[MAX_N];
    int path[MAX_N];
    int pathLength;
} PathResult;

typedef struct {
    pid_t pid;
    Color color;
    Vector2 position;
    int isFinished;
    int wasKilled;
    int pathIndex;
    int currentJump;
    int totalJumps;
    float timer;
    float waitTimer;
    int isWaiting;
} TravelerState;

static int readInt(FILE *file, int *value) {
    char token[128];
    while (fscanf(file, "%127s", token) == 1) {
        if (token[0] == '#') {
            int ch;
            while ((ch = fgetc(file)) != '\n' && ch != EOF) { }
            continue;
        }
        char *endptr = NULL;
        long number = strtol(token, &endptr, 10);
        if (*endptr != '\0') return 0;
        *value = (int)number;
        return 1;
    }
    return 0;
}

static void initGraph(GraphData *data) {
    memset(data, 0, sizeof(GraphData));
    for (int i = 0; i < MAX_N; i++) {
        for (int j = 0; j < MAX_N; j++) data->graph[i][j] = INF;
    }
}

static int loadGraph(const char *filename, GraphData *data) {
    FILE *file = fopen(filename, "r");
    if (!file) { printf("Could not open file\n"); return 0; }

    if (!readInt(file, &data->n) || !readInt(file, &data->m)) {
        printf("Invalid input\n"); fclose(file); return 0;
    }
    if (data->n <= 0 || data->n > MAX_N || data->m < 0 || data->m > MAX_M) {
        printf("Invalid input\n"); fclose(file); return 0;
    }

    for (int i = 0; i < data->m; i++) {
        int src, dst, weight;
        if (!readInt(file, &src) || !readInt(file, &dst) || !readInt(file, &weight)) {
            printf("Invalid input\n"); fclose(file); return 0;
        }
        if (src < 0 || src >= data->n || dst < 0 || dst >= data->n || weight < 0) {
            printf("Invalid input\n"); fclose(file); return 0;
        }
        data->edges[i].src = src;
        data->edges[i].dst = dst;
        data->edges[i].weight = weight;
        if (weight < data->graph[src][dst]) data->graph[src][dst] = weight;
    }

    if (!readInt(file, &data->travelerCount)) {
        printf("Invalid input\n"); fclose(file); return 0;
    }
    if (data->travelerCount <= 0 || data->travelerCount > MAX_TRAVELERS) {
        printf("Invalid input\n"); fclose(file); return 0;
    }

    for (int i = 0; i < data->travelerCount; i++) {
        int source, destination;
        if (!readInt(file, &source) || !readInt(file, &destination)) {
            printf("Invalid input\n"); fclose(file); return 0;
        }
        if (source < 0 || source >= data->n || destination < 0 || destination >= data->n) {
            printf("Invalid input\n"); fclose(file); return 0;
        }
        data->travelers[i].source = source;
        data->travelers[i].destination = destination;
    }

    fclose(file);
    return 1;
}

static int minNode(int dist[], int visited[], int n) {
    int best = INF, index = -1;
    for (int i = 0; i < n; i++) {
        if (!visited[i] && dist[i] < best) { best = dist[i]; index = i; }
    }
    return index;
}

static PathResult dijkstra(const GraphData *data, int source, int destination) {
    PathResult result;
    int visited[MAX_N];
    memset(&result, 0, sizeof(PathResult));
    for (int i = 0; i < data->n; i++) {
        result.dist[i] = INF;
        result.parent[i] = -1;
        visited[i] = 0;
    }
    result.dist[source] = 0;

    for (int i = 0; i < data->n; i++) {
        int u = minNode(result.dist, visited, data->n);
        if (u == -1) break;
        visited[u] = 1;
        for (int v = 0; v < data->n; v++) {
            if (!visited[v] && data->graph[u][v] != INF && result.dist[u] != INF &&
                result.dist[u] + data->graph[u][v] < result.dist[v]) {
                result.dist[v] = result.dist[u] + data->graph[u][v];
                result.parent[v] = u;
            }
        }
    }

    if (result.dist[destination] == INF) return result;
    result.found = 1;

    int reversed[MAX_N], count = 0;
    int current = destination;
    while (current != -1 && count < MAX_N) {
        reversed[count++] = current;
        if (current == source) break;
        current = result.parent[current];
    }
    for (int i = count - 1; i >= 0; i--) result.path[result.pathLength++] = reversed[i];
    return result;
}

static void printPath(const PathResult *path, int source, int destination) {
    if (source == destination) { printf("%d\n0\n", source); return; }
    if (!path->found) { printf("No path found\n"); return; }
    for (int i = 0; i < path->pathLength; i++) {
        printf("%d", path->path[i]);
        if (i < path->pathLength - 1) printf(" -> ");
    }
    printf("\n%d\n", path->dist[destination]);
}

static void calculatePositions(Vector2 positions[], int n) {
    float centerX = SCREEN_WIDTH / 2.0f;
    float centerY = SCREEN_HEIGHT / 2.0f + 50.0f;
    float radius = (n <= 5) ? 180.0f : 260.0f;
    for (int i = 0; i < n; i++) {
        float angle = (2.0f * PI * i) / n - PI / 2.0f;
        positions[i].x = centerX + radius * cosf(angle);
        positions[i].y = centerY + radius * sinf(angle);
    }
}

static void drawArrow(Vector2 start, Vector2 end, Color color, float thickness) {
    Vector2 dir = { end.x - start.x, end.y - start.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len == 0) return;
    dir.x /= len; dir.y /= len;
    Vector2 a = { start.x + dir.x * NODE_RADIUS, start.y + dir.y * NODE_RADIUS };
    Vector2 b = { end.x - dir.x * NODE_RADIUS, end.y - dir.y * NODE_RADIUS };
    DrawLineEx(a, b, thickness, color);
    float s = 12.0f;
    Vector2 left  = { b.x - dir.x * s - dir.y * s * 0.6f, b.y - dir.y * s + dir.x * s * 0.6f };
    Vector2 right = { b.x - dir.x * s + dir.y * s * 0.6f, b.y - dir.y * s - dir.x * s * 0.6f };
    DrawTriangle(b, left, right, color);
}

static int edgeInAnyPath(const PathResult paths[], int count, int src, int dst) {
    for (int t = 0; t < count; t++) {
        for (int i = 0; i < paths[t].pathLength - 1; i++) {
            if (paths[t].path[i] == src && paths[t].path[i + 1] == dst) return 1;
        }
    }
    return 0;
}

static void drawGraph(const GraphData *data, const PathResult paths[], Vector2 positions[]) {
    for (int i = 0; i < data->m; i++) {
        int src = data->edges[i].src, dst = data->edges[i].dst, w = data->edges[i].weight;
        Color c = edgeInAnyPath(paths, data->travelerCount, src, dst) ? RED : DARKGRAY;
        float thick = (c.r == RED.r && c.g == RED.g && c.b == RED.b) ? 4.0f : 2.0f;
        drawArrow(positions[src], positions[dst], c, thick);
        int mx = (int)((positions[src].x + positions[dst].x) / 2.0f);
        int my = (int)((positions[src].y + positions[dst].y) / 2.0f);
        char txt[16]; snprintf(txt, sizeof(txt), "%d", w);
        DrawCircle(mx, my, 14, RAYWHITE);
        DrawCircleLines(mx, my, 14, GRAY);
        DrawText(txt, mx - MeasureText(txt, 18) / 2, my - 9, 18, BLACK);
    }
    for (int i = 0; i < data->n; i++) {
        DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
        DrawCircleLines((int)positions[i].x, (int)positions[i].y, NODE_RADIUS, BLACK);
        char txt[16]; snprintf(txt, sizeof(txt), "%d", i);
        DrawText(txt, (int)positions[i].x - MeasureText(txt, 22) / 2, (int)positions[i].y - 11, 22, BLACK);
    }
}

static void initTravelerState(TravelerState *state, const GraphData *data, const PathResult *path, Vector2 positions[], int index, pid_t pid, Color color) {
    memset(state, 0, sizeof(TravelerState));
    state->pid = pid;
    state->color = color;
    state->position = positions[data->travelers[index].source];
    if (!path->found || path->pathLength <= 1) { state->isFinished = 1; return; }
    int from = path->path[0], to = path->path[1];
    state->totalJumps = data->graph[from][to];
    if (state->totalJumps <= 0) state->totalJumps = 1;
}

static void updateTraveler(TravelerState *state, const GraphData *data, const PathResult *path, Vector2 positions[], float dt) {
    if (state->isFinished || !path->found || path->pathLength <= 1) return;

    if (state->isWaiting) {
        state->waitTimer += dt;
        if (state->waitTimer >= WAIT_TIME) { state->isWaiting = 0; state->waitTimer = 0.0f; }
        return;
    }

    int from = path->path[state->pathIndex];
    int to = path->path[state->pathIndex + 1];
    Vector2 start = positions[from], end = positions[to];
    state->timer += dt;

    if (state->timer >= JUMP_TIME) {
        state->timer = 0.0f;
        state->currentJump++;
        float t = (float)state->currentJump / (float)state->totalJumps;
        if (t > 1.0f) t = 1.0f;
        state->position.x = start.x + (end.x - start.x) * t;
        state->position.y = start.y + (end.y - start.y) * t;

        if (state->currentJump >= state->totalJumps) {
            state->position = end;
            state->pathIndex++;
            if (state->pathIndex >= path->pathLength - 1) { state->isFinished = 1; return; }
            from = path->path[state->pathIndex];
            to = path->path[state->pathIndex + 1];
            state->totalJumps = data->graph[from][to];
            if (state->totalJumps <= 0) state->totalJumps = 1;
            state->currentJump = 0;
            state->isWaiting = 1;
            state->waitTimer = 0.0f;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) { printf("Usage: %s <file_name>\n", argv[0]); return 1; }

    GraphData data;
    initGraph(&data);
    if (!loadGraph(argv[1], &data)) return 1;

    PathResult paths[MAX_TRAVELERS];
    for (int i = 0; i < data.travelerCount; i++) {
        paths[i] = dijkstra(&data, data.travelers[i].source, data.travelers[i].destination);
        printf("Traveler %d: ", i + 1);
        printPath(&paths[i], data.travelers[i].source, data.travelers[i].destination);
    }

    Color colors[MAX_TRAVELERS] = { PURPLE, RED, BLUE, GREEN, ORANGE, MAROON, DARKGREEN, DARKBLUE, BROWN, MAGENTA };
    TravelerState states[MAX_TRAVELERS];
    Vector2 positions[MAX_N];
    calculatePositions(positions, data.n);

    for (int i = 0; i < data.travelerCount; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            printf("[%d] started\n", getpid());
            fflush(stdout);
            while (1) pause();
        }
        initTravelerState(&states[i], &data, &paths[i], positions, i, pid, colors[i]);
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Milestone 4 - Multiple Travelers with Parent Process");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int finishedCount = 0;
        for (int i = 0; i < data.travelerCount; i++) {
            updateTraveler(&states[i], &data, &paths[i], positions, dt);
            if (states[i].isFinished && !states[i].wasKilled) {
                kill(states[i].pid, SIGTERM);
                states[i].wasKilled = 1;
            }
            if (states[i].isFinished) finishedCount++;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Milestone 4 - Multiple Travelers", 30, 20, 28, DARKBLUE);
        DrawText("Parent computes all Dijkstra paths, forks children, and animates travelers.", 30, 60, 20, DARKGRAY);
        drawGraph(&data, paths, positions);

        for (int i = 0; i < data.travelerCount; i++) {
            DrawCircleV(states[i].position, 12, states[i].color);
            DrawCircleLines((int)states[i].position.x, (int)states[i].position.y, 12, BLACK);
            char label[32]; snprintf(label, sizeof(label), "T%d", i + 1);
            DrawText(label, (int)states[i].position.x + 14, (int)states[i].position.y - 10, 18, states[i].color);
        }

        if (finishedCount == data.travelerCount) DrawText("All travelers arrived.", 30, 95, 24, GREEN);
        EndDrawing();
    }

    CloseWindow();

    for (int i = 0; i < data.travelerCount; i++) {
        if (!states[i].wasKilled) kill(states[i].pid, SIGTERM);
    }
    for (int i = 0; i < data.travelerCount; i++) waitpid(states[i].pid, NULL, 0);
    return 0;
}
