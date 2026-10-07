/* Run all four greedy heuristics on one instance and print their selections.
 *
 * Usage: ./schedule [file]      (reads stdin if no file is given)
 *
 * Input format:
 *   n
 *   s_0 f_0
 *   s_1 f_1
 *   ...
 * Each job runs from s up to (but not including) f, with s < f.
 * Jobs are numbered from 0 in input order. */

#include <stdio.h>
#include <stdlib.h>
#include "interval.h"

static const struct { const char *name; Heuristic run; } HEURISTICS[] = {
    {"A Earliest Finish",   earliest_finish},
    {"B Earliest Start",    earliest_start},
    {"C Shortest Duration", shortest_duration},
    {"D Fewest Conflicts",  fewest_conflicts},
};

int main(int argc, char **argv) {
    FILE *in = stdin;
    if (argc > 1 && !(in = fopen(argv[1], "r"))) {
        perror(argv[1]);
        return 1;
    }

    int n;
    if (fscanf(in, "%d", &n) != 1 || n < 0) {
        fprintf(stderr, "error: expected job count on first line\n");
        return 1;
    }
    Job *jobs = malloc((n + 1) * sizeof *jobs);
    int *sel = malloc((n + 1) * sizeof *sel);
    for (int i = 0; i < n; i++) {
        jobs[i].id = i;
        if (fscanf(in, "%d %d", &jobs[i].s, &jobs[i].f) != 2) {
            fprintf(stderr, "error: expected %d jobs, got %d\n", n, i);
            return 1;
        }
        if (jobs[i].s >= jobs[i].f) {
            fprintf(stderr, "error: job %d has s >= f\n", i);
            return 1;
        }
    }

    for (size_t h = 0; h < sizeof HEURISTICS / sizeof *HEURISTICS; h++) {
        int k = HEURISTICS[h].run(jobs, n, sel);
        printf("%-20s |H| = %d :", HEURISTICS[h].name, k);
        for (int i = 0; i < k; i++)
            printf(" %d[%d,%d)", sel[i], jobs[sel[i]].s, jobs[sel[i]].f);
        printf("\n");
    }

    free(jobs);
    free(sel);
    if (in != stdin) fclose(in);
    return 0;
}
