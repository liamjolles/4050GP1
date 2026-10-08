#ifndef INTERVAL_H
#define INTERVAL_H

/* A job runs from s up to (but not including) f.
 * id is the job's position in the input, starting at 0. */
typedef struct {
    int id;
    int s;
    int f;
} Job;

/* Every heuristic takes n jobs and fills sel (size n) with the ids of
 * the jobs it picked, sorted by start time. Returns how many it picked. */
typedef int (*Heuristic)(const Job *jobs, int n, int *sel);

int earliest_finish(const Job *jobs, int n, int *sel);   /* A */
int earliest_start(const Job *jobs, int n, int *sel);    /* B */
int shortest_duration(const Job *jobs, int n, int *sel); /* C */
int fewest_conflicts(const Job *jobs, int n, int *sel);  /* D */

/* The qsort orders A, B and C use (see greedy.c). Exposed so the
 * runtime experiment can time the sorting step on its own. */
int order_finish(const void *x, const void *y);
int order_start(const void *x, const void *y);
int order_duration(const void *x, const void *y);

/* Exact solver (Part 2). Same output as a heuristic, but always optimal.
 * Tries every subset, so it refuses n > EXACT_MAX_N and returns -1. */
#define EXACT_MAX_N 25
int exact_solver(const Job *jobs, int n, int *sel);

/* True if a and b overlap. Jobs that only touch (a.f == b.s) don't. */
static inline int overlaps(const Job *a, const Job *b) {
    return a->s < b->f && b->s < a->f;
}

#endif
