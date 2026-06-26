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

#define MAX_N 15
#define MAX_M 300
#define MAX_TRAVELERS 10
#define INF 1000000000
#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 750
#define NODE_RADIUS 25.0f
#define JUMP_TIME_US 300000
#define WAIT_TIME_US 1000000

typedef struct {
    int src, dst, weight;
} Edge;

typedef struct {
    int source, destination;
} TravelerQuery;

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
    int currentNode;
    int nextNode;
    int isFinished;
} IPCMessage;

typedef struct {
    pid_t pid;
    Color color;
    Vector2 position;
    int currentNode;
    int nextNode;
    int finished;
    int waitDone;
} TravelerView;

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

        if (*endptr != '\0') {
            return 0;
        }

        *value = (int)number;
        return 1;
    }

    return 0;
}

static void initGraph(GraphData *data) {
    memset(data, 0, sizeof(GraphData));

    for (int i = 0; i < MAX_N; i++) {
        for (int j = 0; j < MAX_N; j++) {
            data->graph[i][j] = INF;
        }
    }
}

static int loadGraph(const char *filename, GraphData *data) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        printf("Could not open file\n");
        return 0;
    }

    if (!readInt(file, &data->n) || !readInt(file, &data->m)) {
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

        if (!readInt(file, &src) || !readInt(file, &dst) || !readInt(file, &weight)) {
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

    if (!readInt(file, &data->travelerCount)) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (data->travelerCount <= 0 || data->travelerCount > MAX_TRAVELERS) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    for (int i = 0; i < data->travelerCount; i++) {
        int source, destination;

        if (!readInt(file, &source) || !readInt(file, &destination)) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        if (source < 0 || source >= data->n || destination < 0 || destination >= data->n) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        data->travelers[i].source = source;
        data->travelers[i].destination = destination;
    }

    fclose(file);
    return 1;
}

static int minNode(int dist[], int visited[], int n) {
    int best = INF;
    int index = -1;

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

    if (result.dist[destination] == INF) {
        return result;
    }

    result.found = 1;

    int reversed[MAX_N];
    int count = 0;
    int current = destination;

    while (current != -1 && count < MAX_N) {
        reversed[count++] = current;

        if (current == source) {
            break;
        }

        current = result.parent[current];
    }

    for (int i = count - 1; i >= 0; i--) {
        result.path[result.pathLength++] = reversed[i];
    }

    return result;
}

/*
    NEW:
    The child waits here for confirmation from the parent.
    Parent sends one char ACK after handling each IPCMessage.
*/
static int waitForAck(int ackFd) {
    char ack;
    ssize_t bytesRead;

    do {
        bytesRead = read(ackFd, &ack, sizeof(ack));
    } while (bytesRead == -1 && errno == EINTR);

    if (bytesRead <= 0) {
        return 0;
    }

    return 1;
}

/*
    NEW:
    Sends message to parent, then waits for ACK before continuing.
*/
static int sendMessageAndWait(int writeFd, int ackFd, IPCMessage *msg) {
    ssize_t bytesWritten;

    do {
        bytesWritten = write(writeFd, msg, sizeof(*msg));
    } while (bytesWritten == -1 && errno == EINTR);

    if (bytesWritten != sizeof(*msg)) {
        return 0;
    }

    return waitForAck(ackFd);
}

/*
    CHANGED:
    Added ackFd.
    Child sends update, then waits for parent approval before continuing.
*/
static void childProcess(const GraphData *data, int travelerIndex, int writeFd, int ackFd) {
    TravelerQuery q = data->travelers[travelerIndex];
    PathResult path = dijkstra(data, q.source, q.destination);

    if (!path.found) {
        IPCMessage msg;
        msg.pid = getpid();
        msg.travelerIndex = travelerIndex;
        msg.currentNode = q.source;
        msg.nextNode = -1;
        msg.isFinished = 1;

        sendMessageAndWait(writeFd, ackFd, &msg);

        close(writeFd);
        close(ackFd);
        _exit(0);
    }

    for (int i = 0; i < path.pathLength; i++) {
        IPCMessage msg;
        msg.pid = getpid();
        msg.travelerIndex = travelerIndex;
        msg.currentNode = path.path[i];
        msg.nextNode = (i < path.pathLength - 1) ? path.path[i + 1] : -1;
        msg.isFinished = (i == path.pathLength - 1);

        if (!sendMessageAndWait(writeFd, ackFd, &msg)) {
            close(writeFd);
            close(ackFd);
            _exit(1);
        }

        if (msg.isFinished) {
            break;
        }

        int weight = data->graph[path.path[i]][path.path[i + 1]];

        if (weight <= 0 || weight == INF) {
            weight = 1;
        }

        usleep((unsigned int)weight * JUMP_TIME_US);

        if (i + 1 < path.pathLength - 1) {
            usleep(WAIT_TIME_US);
        }
    }

    close(writeFd);
    close(ackFd);
    _exit(0);
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
    Vector2 dir = {
        end.x - start.x,
        end.y - start.y
    };

    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len == 0) {
        return;
    }

    dir.x /= len;
    dir.y /= len;

    Vector2 a = {
        start.x + dir.x * NODE_RADIUS,
        start.y + dir.y * NODE_RADIUS
    };

    Vector2 b = {
        end.x - dir.x * NODE_RADIUS,
        end.y - dir.y * NODE_RADIUS
    };

    DrawLineEx(a, b, thickness, color);

    float s = 12.0f;

    Vector2 left = {
        b.x - dir.x * s - dir.y * s * 0.6f,
        b.y - dir.y * s + dir.x * s * 0.6f
    };

    Vector2 right = {
        b.x - dir.x * s + dir.y * s * 0.6f,
        b.y - dir.y * s - dir.x * s * 0.6f
    };

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

        DrawText(
            txt,
            (int)positions[i].x - MeasureText(txt, 22) / 2,
            (int)positions[i].y - 11,
            22,
            BLACK
        );
    }
}

static void handleMessage(const IPCMessage *msg, TravelerView views[], Vector2 positions[]) {
    int i = msg->travelerIndex;

    views[i].pid = msg->pid;
    views[i].currentNode = msg->currentNode;
    views[i].nextNode = msg->nextNode;
    views[i].position = positions[msg->currentNode];

    if (msg->isFinished || msg->nextNode == -1) {
        views[i].finished = 1;
        printf("[PID=%d] arrived at node %d | DESTINATION\n", msg->pid, msg->currentNode);
        printf("[PID=%d] finished\n", msg->pid);
    } else {
        printf("[PID=%d] arrived at node %d | next node: %d\n",
               msg->pid,
               msg->currentNode,
               msg->nextNode);
    }

    fflush(stdout);
}

/*
    NEW:
    Parent sends ACK to child after reading and handling message.
*/
static void sendAck(int ackWriteFd) {
    char ack = 'A';
    ssize_t ignored;

    do {
        ignored = write(ackWriteFd, &ack, sizeof(ack));
    } while (ignored == -1 && errno == EINTR);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    GraphData data;
    initGraph(&data);

    if (!loadGraph(argv[1], &data)) {
        return 1;
    }

    /*
        CHANGED:
        Two pipes for every child:
        childToParent = child writes updates, parent reads updates.
        parentToChild = parent writes ACK, child reads ACK.
    */
    int childToParent[MAX_TRAVELERS][2];
    int parentToChild[MAX_TRAVELERS][2];

    TravelerView views[MAX_TRAVELERS];

    Color colors[MAX_TRAVELERS] = {
        PURPLE, RED, BLUE, GREEN, ORANGE,
        MAROON, DARKGREEN, DARKBLUE, BROWN, MAGENTA
    };

    Vector2 positions[MAX_N];
    calculatePositions(positions, data.n);

    for (int i = 0; i < data.travelerCount; i++) {
        if (pipe(childToParent[i]) == -1) {
            perror("pipe childToParent");
            return 1;
        }

        if (pipe(parentToChild[i]) == -1) {
            perror("pipe parentToChild");
            close(childToParent[i][0]);
            close(childToParent[i][1]);
            return 1;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            /*
                Child side:
                - writes to childToParent[i][1]
                - reads ACK from parentToChild[i][0]
            */

            for (int j = 0; j <= i; j++) {
                close(childToParent[j][0]);
                close(parentToChild[j][1]);
            }

            childProcess(&data, i, childToParent[i][1], parentToChild[i][0]);
        }

        /*
            Parent side:
            - reads from childToParent[i][0]
            - writes ACK to parentToChild[i][1]
        */
        close(childToParent[i][1]);
        close(parentToChild[i][0]);

        fcntl(childToParent[i][0], F_SETFL, O_NONBLOCK);

        memset(&views[i], 0, sizeof(TravelerView));
        views[i].pid = pid;
        views[i].color = colors[i];
        views[i].currentNode = data.travelers[i].source;
        views[i].nextNode = -1;
        views[i].position = positions[data.travelers[i].source];
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Milestone 5 - IPC with Parent ACK");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        int finishedCount = 0;

        for (int i = 0; i < data.travelerCount; i++) {
            IPCMessage msg;
            ssize_t bytes;

            while ((bytes = read(childToParent[i][0], &msg, sizeof(msg))) == sizeof(msg)) {
                handleMessage(&msg, views, positions);

                /*
                    NEW:
                    After parent handles the message, it sends ACK to child.
                    Child cannot continue before receiving this ACK.
                */
                sendAck(parentToChild[i][1]);
            }

            if (views[i].finished) {
                finishedCount++;
            }
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("Milestone 5 - IPC with Pipes + Parent ACK", 30, 20, 28, DARKBLUE);
        DrawText("Child sends update -> Parent handles -> Parent sends ACK -> Child continues",
                 30, 60, 20, DARKGRAY);

        drawGraph(&data, positions);

        for (int i = 0; i < data.travelerCount; i++) {
            DrawCircleV(views[i].position, 12, views[i].color);
            DrawCircleLines((int)views[i].position.x, (int)views[i].position.y, 12, BLACK);

            char label[64];
            snprintf(label, sizeof(label), "T%d PID=%d", i + 1, views[i].pid);

            DrawText(
                label,
                (int)views[i].position.x + 14,
                (int)views[i].position.y - 10,
                18,
                views[i].color
            );
        }

        if (finishedCount == data.travelerCount) {
            DrawText("All travelers finished.", 30, 95, 24, GREEN);
        }

        EndDrawing();
    }

    CloseWindow();

    for (int i = 0; i < data.travelerCount; i++) {
        close(childToParent[i][0]);
        close(parentToChild[i][1]);
    }

    for (int i = 0; i < data.travelerCount; i++) {
        waitpid(views[i].pid, NULL, 0);
    }

    return 0;
}