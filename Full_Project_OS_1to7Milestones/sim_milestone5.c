#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "raylib.h"

#define MAX_NODES 15
#define MAX_EDGES 100
#define MAX_TRAVELERS 10

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700
#define NODE_RADIUS 25
#define MOVING_RADIUS 12

#define JUMP_TIME_MS 300
#define WAIT_TIME_MS 1000

#define MSG_NODE 1
#define MSG_FINISHED 2
#define MSG_NO_PATH 3

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

typedef struct Traveler {
    int source;
    int destination;
    pid_t pid;

    int currentNode;
    int nextNode;
    int finished;
    int noPath;

    float stateTimer;
    float waitSeconds;
    float travelSeconds;

    Color color;
} Traveler;

typedef struct Message {
    int type;
    pid_t pid;
    int travelerIndex;
    int currentNode;
    int nextNode;
    int isDestination;
    int waitMs;
    int travelMs;
} Message;

int readIntSkippingComments(FILE *file, int *value) {
    int c;

    while ((c = fgetc(file)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(file)) != EOF && c != '\n') {
            }
            continue;
        }

        if (isspace(c)) {
            continue;
        }

        ungetc(c, file);
        return fscanf(file, "%d", value) == 1;
    }

    return 0;
}

int readInputFile(const char *fileName, Graph *g, Traveler travelers[], int *travelerCount) {
    FILE *file = fopen(fileName, "r");

    if (file == NULL) {
        printf("Cannot open file\n");
        return 0;
    }

    if (!readIntSkippingComments(file, &g->n) ||
        !readIntSkippingComments(file, &g->m)) {
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

        if (!readIntSkippingComments(file, &src) ||
            !readIntSkippingComments(file, &dst) ||
            !readIntSkippingComments(file, &weight)) {
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

    if (!readIntSkippingComments(file, travelerCount)) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (*travelerCount <= 0 || *travelerCount > MAX_TRAVELERS) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    for (int i = 0; i < *travelerCount; i++) {
        int source, destination;

        if (!readIntSkippingComments(file, &source) ||
            !readIntSkippingComments(file, &destination)) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        if (source < 0 || destination < 0 || source >= g->n || destination >= g->n) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        travelers[i].source = source;
        travelers[i].destination = destination;
        travelers[i].pid = -1;

        travelers[i].currentNode = source;
        travelers[i].nextNode = -1;
        travelers[i].finished = 0;
        travelers[i].noPath = 0;

        travelers[i].stateTimer = 0.0f;
        travelers[i].waitSeconds = 0.0f;
        travelers[i].travelSeconds = 0.0f;
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

int dijkstra(Graph *g, int start, int target, int path[], int pathWeights[], int *pathLength) {
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
    return 1;
}

void sleepMilliseconds(int ms) {
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;

    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

int writeFull(int fd, const void *buffer, size_t size) {
    const char *data = (const char *)buffer;
    size_t totalWritten = 0;

    while (totalWritten < size) {
        ssize_t result = write(fd, data + totalWritten, size - totalWritten);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }

        totalWritten += (size_t)result;
    }

    return 1;
}

void sendMessage(int fd, Message msg) {
    writeFull(fd, &msg, sizeof(Message));
}

void childProcess(Graph *g, Traveler traveler, int travelerIndex, int writeFd) {
    int path[MAX_NODES];
    int pathWeights[MAX_NODES];
    int pathLength = 0;

    pid_t myPid = getpid();

    int hasPath = dijkstra(
        g,
        traveler.source,
        traveler.destination,
        path,
        pathWeights,
        &pathLength
    );

    if (!hasPath) {
        Message msg;

        msg.type = MSG_NO_PATH;
        msg.pid = myPid;
        msg.travelerIndex = travelerIndex;
        msg.currentNode = traveler.source;
        msg.nextNode = -1;
        msg.isDestination = 0;
        msg.waitMs = 0;
        msg.travelMs = 0;
        sendMessage(writeFd, msg);

        msg.type = MSG_FINISHED;
        sendMessage(writeFd, msg);

        close(writeFd);
        exit(0);
    }

    for (int i = 0; i < pathLength; i++) {
        Message msg;

        msg.type = MSG_NODE;
        msg.pid = myPid;
        msg.travelerIndex = travelerIndex;
        msg.currentNode = path[i];

        if (i == pathLength - 1) {
            msg.nextNode = -1;
            msg.isDestination = 1;
            msg.waitMs = 0;
            msg.travelMs = 0;
        } else {
            msg.nextNode = path[i + 1];
            msg.isDestination = 0;

            if (i > 0 && i < pathLength - 1) {
                msg.waitMs = WAIT_TIME_MS;
            } else {
                msg.waitMs = 0;
            }

            msg.travelMs = pathWeights[i] * JUMP_TIME_MS;
        }

        sendMessage(writeFd, msg);

        if (i == pathLength - 1) {
            break;
        }

        sleepMilliseconds(msg.waitMs + msg.travelMs);
    }

    Message finishedMsg;

    finishedMsg.type = MSG_FINISHED;
    finishedMsg.pid = myPid;
    finishedMsg.travelerIndex = travelerIndex;
    finishedMsg.currentNode = path[pathLength - 1];
    finishedMsg.nextNode = -1;
    finishedMsg.isDestination = 1;
    finishedMsg.waitMs = 0;
    finishedMsg.travelMs = 0;

    sendMessage(writeFd, finishedMsg);

    close(writeFd);
    exit(0);
}

void calculateNodePositions(int n, Vector2 positions[]) {
    float centerX = SCREEN_WIDTH / 2.0f;
    float centerY = SCREEN_HEIGHT / 2.0f + 30.0f;
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
        drawArrow(
            positions[g->edges[i].src],
            positions[g->edges[i].dst],
            g->edges[i].weight,
            GRAY,
            2.0f
        );
    }
}

void drawNodes(Graph *g, Vector2 positions[]) {
    for (int i = 0; i < g->n; i++) {
        DrawCircleV(positions[i], NODE_RADIUS, RAYWHITE);
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

Vector2 getTravelerPosition(Traveler *traveler, Vector2 positions[]) {
    if (traveler->noPath) {
        Vector2 empty = {0, 0};
        return empty;
    }

    if (traveler->nextNode == -1) {
        return positions[traveler->currentNode];
    }

    if (traveler->stateTimer < traveler->waitSeconds) {
        return positions[traveler->currentNode];
    }

    float moveTime = traveler->stateTimer - traveler->waitSeconds;

    if (traveler->travelSeconds <= 0.0f) {
        return positions[traveler->nextNode];
    }

    float t = moveTime / traveler->travelSeconds;

    if (t < 0.0f) {
        t = 0.0f;
    }

    if (t > 1.0f) {
        t = 1.0f;
    }

    Vector2 from = positions[traveler->currentNode];
    Vector2 to = positions[traveler->nextNode];

    Vector2 pos = {
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t
    };

    return pos;
}

void drawTravelers(Traveler travelers[], int travelerCount, Vector2 positions[]) {
    for (int i = 0; i < travelerCount; i++) {
        if (travelers[i].noPath) {
            continue;
        }

        Vector2 pos = getTravelerPosition(&travelers[i], positions);

        DrawCircleV(pos, MOVING_RADIUS, travelers[i].color);
        DrawCircleLines((int)pos.x, (int)pos.y, MOVING_RADIUS, BLACK);

        const char *label = TextFormat("%d", i + 1);
        int textWidth = MeasureText(label, 12);

        DrawText(label, (int)(pos.x - textWidth / 2), (int)(pos.y - 6), 12, BLACK);
    }
}

void drawInfoPanel(Traveler travelers[], int travelerCount) {
    DrawText("Milestone 5 - IPC with pipe()", 20, 20, 26, BLACK);
    DrawText("Children compute Dijkstra and report node arrivals to the parent.", 20, 55, 18, DARKGRAY);

    int y = 90;

    for (int i = 0; i < travelerCount; i++) {
        const char *status;

        if (travelers[i].noPath) {
            status = "No path";
        } else if (travelers[i].finished) {
            status = "Finished";
        } else if (travelers[i].nextNode == -1) {
            status = "At destination";
        } else if (travelers[i].stateTimer < travelers[i].waitSeconds) {
            status = "Waiting";
        } else {
            status = "Traveling";
        }

        DrawText(
            TextFormat("Traveler %d: PID=%d | current=%d | next=%d | %s",
                       i + 1,
                       travelers[i].pid,
                       travelers[i].currentNode,
                       travelers[i].nextNode,
                       status),
            20,
            y,
            16,
            travelers[i].color
        );

        y += 22;
    }
}

int allFinished(Traveler travelers[], int travelerCount) {
    for (int i = 0; i < travelerCount; i++) {
        if (!travelers[i].finished) {
            return 0;
        }
    }

    return 1;
}

void handleMessage(Message msg, Traveler travelers[]) {
    int i = msg.travelerIndex;

    if (msg.type == MSG_NODE) {
        travelers[i].currentNode = msg.currentNode;
        travelers[i].nextNode = msg.nextNode;
        travelers[i].stateTimer = 0.0f;
        travelers[i].waitSeconds = msg.waitMs / 1000.0f;
        travelers[i].travelSeconds = msg.travelMs / 1000.0f;

        if (msg.isDestination) {
            printf("[PID=%d] arrived at node %d | DESTINATION\n",
                   msg.pid,
                   msg.currentNode);
        } else {
            printf("[PID=%d] arrived at node %d | next node: %d\n",
                   msg.pid,
                   msg.currentNode,
                   msg.nextNode);
        }
    } else if (msg.type == MSG_NO_PATH) {
        travelers[i].noPath = 1;
        printf("[PID=%d] no path found\n", msg.pid);
    } else if (msg.type == MSG_FINISHED) {
        travelers[i].finished = 1;
        travelers[i].nextNode = -1;
        printf("[PID=%d] finished\n", msg.pid);
        waitpid(msg.pid, NULL, 0);
    }
}

void readMessagesFromPipe(int readFd, Traveler travelers[]) {
    Message msg;

    while (1) {
        ssize_t result = read(readFd, &msg, sizeof(Message));

        if (result == sizeof(Message)) {
            handleMessage(msg, travelers);
        } else {
            if (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }

            if (result == -1 && errno == EINTR) {
                continue;
            }

            break;
        }
    }
}

void updateTravelerTimers(Traveler travelers[], int travelerCount, float deltaTime) {
    for (int i = 0; i < travelerCount; i++) {
        if (!travelers[i].finished && !travelers[i].noPath && travelers[i].nextNode != -1) {
            travelers[i].stateTimer += deltaTime;
        }
    }
}

void killRemainingChildren(Traveler travelers[], int travelerCount) {
    for (int i = 0; i < travelerCount; i++) {
        if (travelers[i].pid > 0 && !travelers[i].finished) {
            kill(travelers[i].pid, SIGTERM);
            waitpid(travelers[i].pid, NULL, 0);
            travelers[i].finished = 1;
        }
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc != 2) {
        printf("Usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    Graph g;
    Traveler travelers[MAX_TRAVELERS];
    int travelerCount = 0;

    if (!readInputFile(argv[1], &g, travelers, &travelerCount)) {
        return 1;
    }

    Color colors[MAX_TRAVELERS] = {
        RED, BLUE, ORANGE, PURPLE, MAROON,
        DARKGREEN, GOLD, PINK, BROWN, DARKBLUE
    };

    for (int i = 0; i < travelerCount; i++) {
        travelers[i].color = colors[i % MAX_TRAVELERS];
    }

    int pipeFd[2];

    if (pipe(pipeFd) == -1) {
        perror("pipe");
        return 1;
    }

    for (int i = 0; i < travelerCount; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            killRemainingChildren(travelers, i);
            close(pipeFd[0]);
            close(pipeFd[1]);
            return 1;
        }

        if (pid == 0) {
            close(pipeFd[0]);
            childProcess(&g, travelers[i], i, pipeFd[1]);
        }

        travelers[i].pid = pid;
    }

    close(pipeFd[1]);

    int flags = fcntl(pipeFd[0], F_GETFL, 0);
    if (flags != -1) {
        fcntl(pipeFd[0], F_SETFL, flags | O_NONBLOCK);
    }

    Vector2 positions[MAX_NODES];
    calculateNodePositions(g.n, positions);

    SetTraceLogLevel(LOG_WARNING);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Milestone 5 - IPC");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        readMessagesFromPipe(pipeFd[0], travelers);
        updateTravelerTimers(travelers, travelerCount, deltaTime);

        BeginDrawing();

        ClearBackground(RAYWHITE);

        drawInfoPanel(travelers, travelerCount);

        drawEdges(&g, positions);
        drawNodes(&g, positions);
        drawTravelers(travelers, travelerCount, positions);

        if (allFinished(travelers, travelerCount)) {
            DrawText("All travelers finished.", 390, SCREEN_HEIGHT - 35, 20, DARKGREEN);
        }

        EndDrawing();
    }

    CloseWindow();

    killRemainingChildren(travelers, travelerCount);

    close(pipeFd[0]);

    return 0;
}