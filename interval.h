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

/* True if a and b overlap. Jobs that only touch (a.f == b.s) don't. */
static inline int overlaps(const Job *a, const Job *b) {
    return a->s < b->f && b->s < a->f;
}

#endif
