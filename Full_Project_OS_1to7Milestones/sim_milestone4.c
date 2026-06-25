#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include "raylib.h"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_NODES 15
#define MAX_EDGES 100
#define MAX_TRAVELERS 10

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
    int waiting;
    int arrived;
    int finished;
} Animation;

typedef struct Traveler {
    int source;
    int destination;

    int hasPath;
    int path[MAX_NODES];
    int pathWeights[MAX_NODES];
    int pathLength;
    int totalCost;
    pid_t pid;

    Color color;
    Animation anim;
} Traveler;

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

void resetAnimation(Animation *anim) {
    anim->edgeIndex = 0;
    anim->step = 0;
    anim->timer = 0.0f;
    anim->waiting = 0;
    anim->arrived = 0;
    anim->finished=0;
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

void drawTravelerPath(Traveler *traveler, Vector2 positions[]) {
    if (!traveler->hasPath) {
        return;
    }

    for (int i = 0; i < traveler->pathLength - 1; i++) {
        int src = traveler->path[i];
        int dst = traveler->path[i + 1];
        int weight = traveler->pathWeights[i];

        drawArrow(positions[src], positions[dst], weight, traveler->color, 4.0f);
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

void advanceToNextNode(Animation *anim, int pathLength) {
    anim->edgeIndex++;
    anim->step = 0;
    anim->timer = 0.0f;

    if (anim->edgeIndex >= pathLength - 1) {
        anim->arrived = 1;
        anim->waiting = 0;
    } else {
        anim->waiting = 1;
    }
}

void updateAnimation(Animation *anim, int pathLength, int pathWeights[], float deltaTime) {
    if (anim->arrived || pathLength < 2) {
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

Vector2 getTravelerPosition(Traveler *traveler, Vector2 positions[]) {
    if (!traveler->hasPath || traveler->pathLength == 0) {
        Vector2 empty = {0, 0};
        return empty;
    }

    Animation *anim = &traveler->anim;

    if (anim->arrived || anim->edgeIndex >= traveler->pathLength - 1) {
        return positions[traveler->path[traveler->pathLength - 1]];
    }

    if (anim->waiting) {
        return positions[traveler->path[anim->edgeIndex]];
    }

    int from = traveler->path[anim->edgeIndex];
    int to = traveler->path[anim->edgeIndex + 1];
    int stepsOnEdge = traveler->pathWeights[anim->edgeIndex];

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

void drawTravelers(Traveler travelers[], int travelerCount, Vector2 positions[]) {
    for (int i = 0; i < travelerCount; i++) {
        if (!travelers[i].hasPath) {
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
    DrawText("Milestone 4 - Multiple Travelers", 20, 20, 26, BLACK);
    DrawText("This step: multiple travelers moving at the same time.", 20, 55, 18, DARKGRAY);

    int y = 90;

    for (int i = 0; i < travelerCount; i++) {
        const char *status;

        if (!travelers[i].hasPath) {
            status = "No path";
        } else if (travelers[i].anim.arrived) {
            status = "Arrived";
        } else if (travelers[i].anim.waiting) {
            status = "Waiting";
        } else {
            status = "Moving";
        }

        DrawText(
            TextFormat("Traveler %d: %d -> %d | Cost=%d | %s",
                       i + 1,
                       travelers[i].source,
                       travelers[i].destination,
                       travelers[i].totalCost,
                       status),
            20,
            y,
            16,
            travelers[i].color
        );

        y += 22;
    }
}
void finishTravelerProcess(Traveler *traveler ) {
    if (traveler->anim.finished ||traveler->pid<=0) {
        return;
    }
    kill(traveler->pid,SIGTERM);
    waitpid(traveler->pid ,NULL,0);

}
void killRemainingChildren(Traveler travelers[],int travelerCount) {
    for (int i=0;i<travelerCount;i++) {
        if (travelers[i].pid >0 &&!travelers[i].anim.finished) {
            kill(travelers[i].pid,SIGTERM);
            waitpid(travelers[i].pid,NULL,0);
            travelers[i].anim.finished=1;

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

        resetAnimation(&travelers[i].anim);

        travelers[i].hasPath = dijkstra(
            &g,
            travelers[i].source,
            travelers[i].destination,
            travelers[i].path,
            travelers[i].pathWeights,
            &travelers[i].pathLength,
            &travelers[i].totalCost
        );

        if (travelers[i].hasPath && travelers[i].pathLength <= 1) {
            travelers[i].anim.arrived = 1;
        }
    }
    for (int i = 0; i < travelerCount; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            killRemainingChildren(travelers, i);
            return 1;
        }

        if (pid == 0) {
            printf("[%d] started\n", getpid());

            while (1) {
                pause();
            }

            return 0;
        }

        travelers[i].pid = pid;
    }

    Vector2 positions[MAX_NODES];
    calculateNodePositions(g.n, positions);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Milestone 4 - Multiple Travelers");
    SetTargetFPS(60);

    int paused = 0;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        if (IsKeyPressed(KEY_SPACE)) {
            paused = !paused;
        }

        if (!paused) {
            for (int i = 0; i < travelerCount; i++) {
                if (travelers[i].hasPath && !travelers[i].anim.arrived) {
                    updateAnimation(
                        &travelers[i].anim,
                        travelers[i].pathLength,
                        travelers[i].pathWeights,
                        deltaTime
                    );
                }

                if (travelers[i].hasPath &&
                    travelers[i].anim.arrived &&
                    !travelers[i].anim.finished) {
                    finishTravelerProcess(&travelers[i]);
                    }
            }
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);

        drawInfoPanel(travelers, travelerCount);

        DrawText("Press SPACE to pause/resume", 20, SCREEN_HEIGHT - 35, 18, DARKGRAY);

        if (paused) {
            DrawText("PAUSED", SCREEN_WIDTH - 140, 20, 24, RED);
        }

        drawEdges(&g, positions);

        for (int i = 0; i < travelerCount; i++) {
            drawTravelerPath(&travelers[i], positions);
        }

        drawNodes(&g, positions);
        drawTravelers(travelers, travelerCount, positions);

        EndDrawing();
    }

    CloseWindow();
    killRemainingChildren(travelers, travelerCount);
    return 0;
}
