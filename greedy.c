/* Part 1: four greedy strategies for interval scheduling.
 *
 * A, B and C sort the jobs, then take each job that doesn't overlap one
 * already taken. They only differ in how they sort. Ties are broken by
 * the comparators, ending with the job id, so results are deterministic.
 *
 * D (fewest conflicts) has its own loop because its counts change as
 * jobs are removed. */

#include <stdlib.h>
#include <string.h>
#include "interval.h"

#define CMP(a, b) (((a) > (b)) - ((a) < (b)))

/* ---- sort orders ---- */

/* A: finish, then start, then id */
int order_finish(const void *x, const void *y) {
    const Job *a = x, *b = y;
    if (a->f != b->f) return CMP(a->f, b->f);
    if (a->s != b->s) return CMP(a->s, b->s);
    return CMP(a->id, b->id);
}

/* B: start, then finish, then id */
int order_start(const void *x, const void *y) {
    const Job *a = x, *b = y;
    if (a->s != b->s) return CMP(a->s, b->s);
    if (a->f != b->f) return CMP(a->f, b->f);
    return CMP(a->id, b->id);
}

/* C: duration, then start, then id */
int order_duration(const void *x, const void *y) {
    const Job *a = x, *b = y;
    long long da = (long long)a->f - a->s, db = (long long)b->f - b->s;
    if (da != db) return CMP(da, db);
    if (a->s != b->s) return CMP(a->s, b->s);
    return CMP(a->id, b->id);
}

/* ---- shared machinery for A, B, C ---- */

/* chosen holds k non-overlapping jobs sorted by start time.
 * Returns where j should go in chosen, or -1 if j overlaps one of them.
 * Uses binary search, so it only needs to check j's two neighbours. */
static int fit_position(const Job *chosen, int k, const Job *j) {
    int lo = 0, hi = k;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (chosen[mid].s < j->s) lo = mid + 1; else hi = mid;
    }
    if (lo < k && chosen[lo].s < j->f) return -1;     /* overlaps next job */
    if (lo > 0 && chosen[lo - 1].f > j->s) return -1; /* overlaps previous job */
    return lo;
}

/* Sort the jobs with cmp, then keep each job that fits.
 * A and B always add to the end of chosen: O(n log n).
 * C can insert in the middle, which shifts the array: O(n log n + k^2). */
static int greedy_by_order(const Job *jobs, int n, int *sel,
                           int (*cmp)(const void *, const void *)) {
    Job *order = malloc(n * sizeof *order);
    Job *chosen = malloc(n * sizeof *chosen);
    memcpy(order, jobs, n * sizeof *order);
    qsort(order, n, sizeof *order, cmp);

    int k = 0;
    for (int i = 0; i < n; i++) {
        int pos = fit_position(chosen, k, &order[i]);
        if (pos < 0) continue;
        memmove(&chosen[pos + 1], &chosen[pos], (k - pos) * sizeof *chosen);
        chosen[pos] = order[i];
        k++;
    }

    for (int i = 0; i < k; i++) sel[i] = chosen[i].id;
    free(order);
    free(chosen);
    return k;
}

int earliest_finish(const Job *jobs, int n, int *sel) {
    return greedy_by_order(jobs, n, sel, order_finish);
}

int earliest_start(const Job *jobs, int n, int *sel) {
    return greedy_by_order(jobs, n, sel, order_start);
}

int shortest_duration(const Job *jobs, int n, int *sel) {
    return greedy_by_order(jobs, n, sel, order_duration);
}

/* ---- D: fewest conflicts ----
 *
 * Repeat until no jobs are left:
 *   1. Pick the job that overlaps the fewest remaining jobs
 *      (ties: earlier start, then smaller id).
 *   2. Remove it and every job that overlaps it.
 *   3. Update the counts of the jobs that are left.
 *
 * Any job still left can't overlap a picked job, so no extra check is needed.
 * Runs in O(n^2). */
int fewest_conflicts(const Job *jobs, int n, int *sel) {
    int *conf = calloc(n, sizeof *conf);
    char *alive = malloc(n);
    int *removed = malloc(n * sizeof *removed);
    Job *chosen = malloc(n * sizeof *chosen);
    memset(alive, 1, n);

    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (overlaps(&jobs[i], &jobs[j])) { conf[i]++; conf[j]++; }

    int k = 0;
    for (;;) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (!alive[i]) continue;
            if (best < 0 || conf[i] < conf[best] ||
                (conf[i] == conf[best] &&
                 (jobs[i].s < jobs[best].s ||
                  (jobs[i].s == jobs[best].s && jobs[i].id < jobs[best].id))))
                best = i;
        }
        if (best < 0) break;
        chosen[k++] = jobs[best];

        /* remove best and the jobs it overlaps (this includes best itself) */
        int r = 0;
        for (int i = 0; i < n; i++)
            if (alive[i] && overlaps(&jobs[i], &jobs[best])) {
                alive[i] = 0;
                removed[r++] = i;
            }
        /* jobs that are left lose one conflict for each removed job they overlapped */
        for (int a = 0; a < r; a++)
            for (int i = 0; i < n; i++)
                if (alive[i] && overlaps(&jobs[i], &jobs[removed[a]]))
                    conf[i]--;
    }

    qsort(chosen, k, sizeof *chosen, order_start);
    for (int i = 0; i < k; i++) sel[i] = chosen[i].id;
    free(conf);
    free(alive);
    free(removed);
    free(chosen);
    return k;
}
