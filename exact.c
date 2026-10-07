/* Part 2: brute-force exact solver.
 *
 * Tries all 2^n subsets of jobs and keeps the largest one where no two
 * jobs overlap. Since every subset is checked, the result is optimal.
 * If several subsets tie, the first one found is returned, so the
 * output is deterministic. Runs in O(2^n * n). */

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "interval.h"

/* sort by start, then id */
static int cmp_start(const void *x, const void *y) {
    const Job *a = x, *b = y;
    if (a->s != b->s) return (a->s > b->s) - (a->s < b->s);
    return (a->id > b->id) - (a->id < b->id);
}

int exact_solver(const Job *jobs, int n, int *sel) {
    if (n > EXACT_MAX_N) return -1;

    /* Sorting by start first means a subset is compatible exactly when
     * each job starts at or after the previous job in it finishes. */
    Job *order = malloc((n + 1) * sizeof *order);
    memcpy(order, jobs, n * sizeof *order);
    qsort(order, n, sizeof *order, cmp_start);

    /* bit i of mask set = order[i] is in the subset */
    unsigned long best_mask = 0;
    int best = 0;
    for (unsigned long mask = 0; mask < (1UL << n); mask++) {
        int count = 0, last_f = INT_MIN, ok = 1;
        for (int i = 0; i < n && ok; i++) {
            if (!(mask >> i & 1)) continue;
            if (order[i].s < last_f) ok = 0;
            else { last_f = order[i].f; count++; }
        }
        if (ok && count > best) { best = count; best_mask = mask; }
    }

    int k = 0;
    for (int i = 0; i < n; i++)
        if (best_mask >> i & 1) sel[k++] = order[i].id;
    free(order);
    return k;
}
