#include <stdio.h>
#include <ctype.h>

#define MAX_NODES 15
#define MAX_EDGES 100
#define MAX_TRAVELERS 10

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

int readIntSkippingComments(FILE *file, int *value) {
    int c;

    while ((c = fgetc(file)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(file)) != EOF && c != '\n') {
                // skip comment line
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

int main(void) {
    Graph g;
    Traveler travelers[MAX_TRAVELERS];
    int travelerCount = 0;

    if (!readInputFile("input4.txt", &g, travelers, &travelerCount)) {
        return 1;
    }

    printf("Graph loaded successfully\n");
    printf("Nodes: %d\n", g.n);
    printf("Edges: %d\n", g.m);

    printf("\nEdges:\n");
    for (int i = 0; i < g.m; i++) {
        printf("%d -> %d weight %d\n",
               g.edges[i].src,
               g.edges[i].dst,
               g.edges[i].weight);
    }

    printf("\nTravelers: %d\n", travelerCount);
    for (int i = 0; i < travelerCount; i++) {
        printf("Traveler %d: %d -> %d\n",
               i + 1,
               travelers[i].source,
               travelers[i].destination);
    }

    return 0;
}
