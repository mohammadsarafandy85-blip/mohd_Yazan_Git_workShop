#define _DEFAULT_SOURCE
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>

#define MAX_N 15
#define MAX_M 300
#define MAX_TRAVELERS 10
#define INF 1000000000

#define SCREEN_WIDTH 1100
#define SCREEN_HEIGHT 780
#define NODE_RADIUS 25.0f
#define JUMP_TIME_US 300000
#define NODE_WAIT_US 1000000

#define MSG_WAITING 1
#define MSG_ENTERED 2
#define MSG_LEAVING 3
#define MSG_FINISHED 4
#define MSG_NO_PATH 5

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
    int travelerIndex;
    int type;
    int currentNode;
    int nextNode;
    int previousNode;
} IPCMessage;

typedef struct {
    pid_t pid;
    Color color;
    Vector2 position;
    Vector2 startPosition;
    Vector2 endPosition;
    float moveDuration;
    float moveTimer;
    int moving;
    int currentNode;
    int nextNode;
    int waiting;
    int insideNode;
    int finished;
    int noPath;
} TravelerView;

static sem_t *nodeSems[MAX_N];
static char semNames[MAX_N][64];

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
        if (!visited[i] && dist[i] < best) {
            best = dist[i];
            index = i;
        }
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
    int reversed[MAX_N];
    int count = 0;
    int current = destination;

    while (current != -1 && count < MAX_N) {
        reversed[count++] = current;
        if (current == source) break;
        current = result.parent[current];
    }

    for (int i = count - 1; i >= 0; i--) {
        result.path[result.pathLength++] = reversed[i];
    }

    return result;
}

static void sendMessage(int writeFd, int travelerIndex, int type, int current, int next, int previous) {
    IPCMessage msg;
    msg.pid = getpid();
    msg.travelerIndex = travelerIndex;
    msg.type = type;
    msg.currentNode = current;
    msg.nextNode = next;
    msg.previousNode = previous;
    ssize_t ignored = write(writeFd, &msg, sizeof(msg));
    (void)ignored;
}

static void childProcess(const GraphData *data, int travelerIndex, int writeFd) {
    TravelerQuery q = data->travelers[travelerIndex];
    PathResult path = dijkstra(data, q.source, q.destination);

    if (!path.found) {
        sendMessage(writeFd, travelerIndex, MSG_NO_PATH, q.source, -1, -1);
        sendMessage(writeFd, travelerIndex, MSG_FINISHED, q.source, -1, -1);
        close(writeFd);
        _exit(0);
    }

    for (int i = 0; i < path.pathLength; i++) {
        int node = path.path[i];
        int previous = (i > 0) ? path.path[i - 1] : -1;
        int next = (i < path.pathLength - 1) ? path.path[i + 1] : -1;

        sendMessage(writeFd, travelerIndex, MSG_WAITING, node, next, previous);

        sem_wait(nodeSems[node]);

        sendMessage(writeFd, travelerIndex, MSG_ENTERED, node, next, previous);
        usleep(NODE_WAIT_US);

        sem_post(nodeSems[node]);

        if (next == -1) {
            sendMessage(writeFd, travelerIndex, MSG_FINISHED, node, -1, previous);
            break;
        }

        sendMessage(writeFd, travelerIndex, MSG_LEAVING, node, next, previous);

        int weight = data->graph[node][next];
        if (weight <= 0 || weight == INF) weight = 1;
        usleep((unsigned int)weight * JUMP_TIME_US);
    }

    close(writeFd);
    _exit(0);
}

static int createSemaphores(int n) {
    pid_t parentPid = getpid();
    for (int i = 0; i < n; i++) {
        snprintf(semNames[i], sizeof(semNames[i]), "/node_sem_%d_%d", (int)parentPid, i);
        sem_unlink(semNames[i]);
        nodeSems[i] = sem_open(semNames[i], O_CREAT | O_EXCL, 0600, 1);
        if (nodeSems[i] == SEM_FAILED) {
            perror("sem_open");
            return 0;
        }
    }
    return 1;
}

static void cleanupSemaphores(int n) {
    for (int i = 0; i < n; i++) {
        if (nodeSems[i] != NULL && nodeSems[i] != SEM_FAILED) {
            sem_close(nodeSems[i]);
            sem_unlink(semNames[i]);
        }
    }
}

static void calculatePositions(Vector2 positions[], int n) {
    float centerX = SCREEN_WIDTH / 2.0f;
    float centerY = SCREEN_HEIGHT / 2.0f + 55.0f;
    float radius = (n <= 5) ? 190.0f : 270.0f;

    for (int i = 0; i < n; i++) {
        float angle = (2.0f * PI * i) / n - PI / 2.0f;
        positions[i].x = centerX + radius * cosf(angle);
        positions[i].y = centerY + radius * sinf(angle);
    }
}

static Vector2 waitingPosition(Vector2 positions[], int previousNode, int targetNode, int travelerIndex) {
    Vector2 target = positions[targetNode];
    Vector2 dir;

    if (previousNode >= 0) {
        Vector2 previous = positions[previousNode];
        dir.x = previous.x - target.x;
        dir.y = previous.y - target.y;
    } else {
        float angle = (float)travelerIndex * 0.9f + 0.5f;
        dir.x = cosf(angle);
        dir.y = sinf(angle);
    }

    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len == 0.0f) {
        dir.x = 1.0f;
        dir.y = 0.0f;
        len = 1.0f;
    }

    dir.x /= len;
    dir.y /= len;

    Vector2 p;
    p.x = target.x + dir.x * 55.0f + (float)(travelerIndex % 3) * 8.0f;
    p.y = target.y + dir.y * 55.0f + (float)(travelerIndex % 3) * 8.0f;
    return p;
}

static void drawArrow(Vector2 start, Vector2 end, Color color, float thickness) {
    Vector2 dir = { end.x - start.x, end.y - start.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len == 0.0f) return;

    dir.x /= len;
    dir.y /= len;

    Vector2 a = { start.x + dir.x * NODE_RADIUS, start.y + dir.y * NODE_RADIUS };
    Vector2 b = { end.x - dir.x * NODE_RADIUS, end.y - dir.y * NODE_RADIUS };

    DrawLineEx(a, b, thickness, color);

    float s = 12.0f;
    Vector2 left  = { b.x - dir.x * s - dir.y * s * 0.6f, b.y - dir.y * s + dir.x * s * 0.6f };
    Vector2 right = { b.x - dir.x * s + dir.y * s * 0.6f, b.y - dir.y * s - dir.x * s * 0.6f };
    DrawTriangle(b, left, right, color);
}

static void drawGraph(const GraphData *data, Vector2 positions[]) {
    for (int i = 0; i < data->m; i++) {
        int src = data->edges[i].src;
        int dst = data->edges[i].dst;
        int w = data->edges[i].weight;

        drawArrow(positions[src], positions[dst], DARKGRAY, 2.0f);

        int mx = (int)((positions[src].x + positions[dst].x) / 2.0f);
        int my = (int)((positions[src].y + positions[dst].y) / 2.0f);
        char txt[16];
        snprintf(txt, sizeof(txt), "%d", w);
        DrawCircle(mx, my, 14, RAYWHITE);
        DrawCircleLines(mx, my, 14, GRAY);
        DrawText(txt, mx - MeasureText(txt, 18) / 2, my - 9, 18, BLACK);
    }

    for (int i = 0; i < data->n; i++) {
        DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
        DrawCircleLines((int)positions[i].x, (int)positions[i].y, NODE_RADIUS, BLACK);
        char txt[16];
        snprintf(txt, sizeof(txt), "%d", i);
        DrawText(txt, (int)positions[i].x - MeasureText(txt, 22) / 2,
                 (int)positions[i].y - 11, 22, BLACK);
    }
}

static const char *statusText(const TravelerView *view) {
    if (view->noPath) return "NO PATH";
    if (view->finished) return "FINISHED";
    if (view->waiting) return "WAITING";
    if (view->insideNode) return "IN NODE";
    if (view->moving) return "MOVING";
    return "READY";
}

static void startMove(TravelerView *view, Vector2 from, Vector2 to, int weight) {
    view->startPosition = from;
    view->endPosition = to;
    view->position = from;
    view->moveTimer = 0.0f;
    view->moveDuration = (float)weight * 0.3f;
    if (view->moveDuration <= 0.0f) view->moveDuration = 0.3f;
    view->moving = 1;
}

static void updateMove(TravelerView *view, float dt) {
    if (!view->moving) return;

    view->moveTimer += dt;
    float t = view->moveTimer / view->moveDuration;
    if (t >= 1.0f) {
        t = 1.0f;
        view->moving = 0;
    }

    view->position.x = view->startPosition.x + (view->endPosition.x - view->startPosition.x) * t;
    view->position.y = view->startPosition.y + (view->endPosition.y - view->startPosition.y) * t;
}

static void handleMessage(const IPCMessage *msg, const GraphData *data, TravelerView views[], Vector2 positions[]) {
    int i = msg->travelerIndex;
    TravelerView *view = &views[i];
    view->pid = msg->pid;

    if (msg->type == MSG_WAITING) {
        view->waiting = 1;
        view->insideNode = 0;
        view->finished = 0;
        view->moving = 0;
        view->currentNode = msg->currentNode;
        view->nextNode = msg->nextNode;
        view->position = waitingPosition(positions, msg->previousNode, msg->currentNode, i);
        printf("[PID=%d] waiting outside node %d\n", msg->pid, msg->currentNode);
    } else if (msg->type == MSG_ENTERED) {
        view->waiting = 0;
        view->insideNode = 1;
        view->moving = 0;
        view->currentNode = msg->currentNode;
        view->nextNode = msg->nextNode;
        view->position = positions[msg->currentNode];
        if (msg->nextNode == -1) {
            printf("[PID=%d] entered node %d | DESTINATION\n", msg->pid, msg->currentNode);
        } else {
            printf("[PID=%d] entered node %d | next node: %d\n", msg->pid, msg->currentNode, msg->nextNode);
        }
    } else if (msg->type == MSG_LEAVING) {
        view->waiting = 0;
        view->insideNode = 0;
        view->finished = 0;
        view->currentNode = msg->currentNode;
        view->nextNode = msg->nextNode;
        int weight = data->graph[msg->currentNode][msg->nextNode];
        if (weight <= 0 || weight == INF) weight = 1;
        startMove(view, positions[msg->currentNode], positions[msg->nextNode], weight);
        printf("[PID=%d] leaving node %d -> %d\n", msg->pid, msg->currentNode, msg->nextNode);
    } else if (msg->type == MSG_FINISHED) {
        view->waiting = 0;
        view->insideNode = 0;
        view->moving = 0;
        view->finished = 1;
        view->currentNode = msg->currentNode;
        view->nextNode = -1;
        view->position = positions[msg->currentNode];
        printf("[PID=%d] finished\n", msg->pid);
    } else if (msg->type == MSG_NO_PATH) {
        view->noPath = 1;
        view->finished = 1;
        view->position = positions[msg->currentNode];
        printf("[PID=%d] no path found for traveler %d\n", msg->pid, i + 1);
    }
    fflush(stdout);
}

static void drawTravelers(const GraphData *data, TravelerView views[]) {
    for (int i = 0; i < data->travelerCount; i++) {
        Color drawColor = views[i].color;
        float radius = 12.0f;

        if (views[i].waiting) {
            drawColor = YELLOW;
            radius = 14.0f;
        } else if (views[i].insideNode) {
            radius = 15.0f;
        } else if (views[i].finished) {
            drawColor = GREEN;
        }

        DrawCircleV(views[i].position, radius, drawColor);
        DrawCircleLines((int)views[i].position.x, (int)views[i].position.y, radius, BLACK);

        char label[96];
        snprintf(label, sizeof(label), "T%d %s", i + 1, statusText(&views[i]));
        DrawText(label, (int)views[i].position.x + 16, (int)views[i].position.y - 10, 17, BLACK);
    }
}

static void drawLegend(void) {
    DrawText("Milestone 6 - Node Synchronization", 30, 20, 28, DARKBLUE);
    DrawText("Semaphore per node: only one traveler can wait inside a node for 1 second.", 30, 58, 19, DARKGRAY);
    DrawText("Yellow = waiting outside node | Larger colored circle = inside critical section | Green = finished", 30, 84, 19, DARKGRAY);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    GraphData data;
    initGraph(&data);
    if (!loadGraph(argv[1], &data)) return 1;

    if (!createSemaphores(data.n)) {
        cleanupSemaphores(data.n);
        return 1;
    }

    int pipes[MAX_TRAVELERS][2];
    TravelerView views[MAX_TRAVELERS];
    pid_t children[MAX_TRAVELERS];
    Color colors[MAX_TRAVELERS] = { PURPLE, RED, BLUE, ORANGE, MAROON, DARKGREEN, DARKBLUE, BROWN, MAGENTA, PINK };
    Vector2 positions[MAX_N];
    calculatePositions(positions, data.n);

    memset(views, 0, sizeof(views));
    memset(children, 0, sizeof(children));

    for (int i = 0; i < data.travelerCount; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            cleanupSemaphores(data.n);
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            cleanupSemaphores(data.n);
            return 1;
        }

        if (pid == 0) {
            for (int j = 0; j <= i; j++) close(pipes[j][0]);
            childProcess(&data, i, pipes[i][1]);
        }

        children[i] = pid;
        close(pipes[i][1]);
        fcntl(pipes[i][0], F_SETFL, O_NONBLOCK);

        views[i].pid = pid;
        views[i].color = colors[i];
        views[i].currentNode = data.travelers[i].source;
        views[i].nextNode = -1;
        views[i].position = positions[data.travelers[i].source];
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Milestone 6 - Node Synchronization");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        int finishedCount = 0;
        for (int i = 0; i < data.travelerCount; i++) {
            IPCMessage msg;
            ssize_t bytes;
            while ((bytes = read(pipes[i][0], &msg, sizeof(msg))) == sizeof(msg)) {
                handleMessage(&msg, &data, views, positions);
            }
            updateMove(&views[i], dt);
            if (views[i].finished) finishedCount++;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        drawLegend();
        drawGraph(&data, positions);
        drawTravelers(&data, views);

        if (finishedCount == data.travelerCount) {
            DrawText("All travelers finished.", 30, 115, 24, GREEN);
        }

        EndDrawing();
    }

    CloseWindow();

    for (int i = 0; i < data.travelerCount; i++) close(pipes[i][0]);
    for (int i = 0; i < data.travelerCount; i++) {
        if (children[i] > 0) {
            int status;
            waitpid(children[i], &status, 0);
        }
    }

    cleanupSemaphores(data.n);
    return 0;
}
