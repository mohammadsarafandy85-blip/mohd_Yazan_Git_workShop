#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define MAX_NODES 15
#define MAX_EDGES 100
#define MAX_TRAVELERS 10

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
} Traveler;

typedef struct Message {
    int type;
    pid_t pid;
    int travelerIndex;
    int currentNode;
    int nextNode;
    int isDestination;
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
        if (!readIntSkippingComments(file, &travelers[i].source) ||
            !readIntSkippingComments(file, &travelers[i].destination)) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        if (travelers[i].source < 0 ||
            travelers[i].destination < 0 ||
            travelers[i].source >= g->n ||
            travelers[i].destination >= g->n) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }
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
             int *pathLength) {
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

void childWork(Graph *g, Traveler traveler, int travelerIndex, int writeFd) {
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
        } else {
            msg.nextNode = path[i + 1];
            msg.isDestination = 0;
        }

        sendMessage(writeFd, msg);

        if (i == pathLength - 1) {
            break;
        }

        if (i > 0 && i < pathLength - 1) {
            sleepMilliseconds(WAIT_TIME_MS);
        }

        sleepMilliseconds(pathWeights[i] * JUMP_TIME_MS);
    }

    Message finishedMsg;

    finishedMsg.type = MSG_FINISHED;
    finishedMsg.pid = myPid;
    finishedMsg.travelerIndex = travelerIndex;
    finishedMsg.currentNode = path[pathLength - 1];
    finishedMsg.nextNode = -1;
    finishedMsg.isDestination = 1;

    sendMessage(writeFd, finishedMsg);

    close(writeFd);
    exit(0);
}

void handleMessage(Message msg) {
    if (msg.type == MSG_NODE) {
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
        printf("[PID=%d] no path found\n", msg.pid);
    } else if (msg.type == MSG_FINISHED) {
        printf("[PID=%d] finished\n", msg.pid);
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    Graph g;
    Traveler travelers[MAX_TRAVELERS];
    int travelerCount = 0;

    if (!readInputFile("input5.txt", &g, travelers, &travelerCount)) {
        return 1;
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
            return 1;
        }

        if (pid == 0) {
            close(pipeFd[0]);
            childWork(&g, travelers[i], i, pipeFd[1]);
        }
    }

    close(pipeFd[1]);

    Message msg;

    while (read(pipeFd[0], &msg, sizeof(Message)) == sizeof(Message)) {
        handleMessage(msg);
    }

    close(pipeFd[0]);

    for (int i = 0; i < travelerCount; i++) {
        wait(NULL);
    }

    return 0;
}
