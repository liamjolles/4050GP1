/* Part 1 tests: the two required sanity tests plus a few small cases.
 * Each test checks that a heuristic picks exactly the expected jobs and
 * that none of them overlap. */

#include <stdio.h>
#include "interval.h"

static int failures = 0;

static void check(const char *name, Heuristic h, const Job *jobs, int n,
                  const int *want, int want_k) {
    int sel[64];
    int k = h(jobs, n, sel);
    int ok = (k == want_k);
    for (int i = 0; ok && i < k; i++) ok = (sel[i] == want[i]);
    /* picked jobs must not overlap */
    for (int i = 0; i < k; i++)
        for (int j = i + 1; j < k; j++)
            if (overlaps(&jobs[sel[i]], &jobs[sel[j]])) ok = 0;

    printf("%s  %s: got {", ok ? "PASS" : "FAIL", name);
    for (int i = 0; i < k; i++) printf(i ? ",%d" : "%d", sel[i]);
    printf("}");
    if (!ok) {
        printf(" want {");
        for (int i = 0; i < want_k; i++) printf(i ? ",%d" : "%d", want[i]);
        printf("}");
        failures++;
    }
    printf("\n");
}

int main(void) {
    /* Sanity Test 1: A=(0,10) B=(1,2) C=(2,3) D=(3,4) E=(4,5) */
    Job t1[] = {{0, 0, 10}, {1, 1, 2}, {2, 2, 3}, {3, 3, 4}, {4, 4, 5}};
    check("Sanity 1, Earliest Start picks only A ", earliest_start, t1, 5,
          (int[]){0}, 1);
    check("Sanity 1, Earliest Finish picks B,C,D,E", earliest_finish, t1, 5,
          (int[]){1, 2, 3, 4}, 4);

    /* Sanity Test 2: A=(2,4) B=(0,3) C=(3,6) */
    Job t2[] = {{0, 2, 4}, {1, 0, 3}, {2, 3, 6}};
    check("Sanity 2, Shortest Duration picks only A", shortest_duration, t2, 3,
          (int[]){0}, 1);
    check("Sanity 2, Earliest Finish picks B,C    ", earliest_finish, t2, 3,
          (int[]){1, 2}, 2);

    /* Exact solver finds the optimum on both sanity tests */
    check("Sanity 1, Exact finds B,C,D,E (OPT 4)", exact_solver, t1, 5,
          (int[]){1, 2, 3, 4}, 4);
    check("Sanity 2, Exact finds B,C (OPT 2)    ", exact_solver, t2, 3,
          (int[]){1, 2}, 2);

    /* Exact solver refuses instances that are too large */
    Job big[EXACT_MAX_N + 1];
    int big_sel[EXACT_MAX_N + 1];
    for (int i = 0; i <= EXACT_MAX_N; i++) big[i] = (Job){i, i, i + 1};
    int big_k = exact_solver(big, EXACT_MAX_N + 1, big_sel);
    printf("%s  Exact refuses n > %d: got %d\n", big_k == -1 ? "PASS" : "FAIL",
           EXACT_MAX_N, big_k);
    if (big_k != -1) failures++;

    /* [0,2) [2,4) [4,6) touch but don't overlap, so all 3 fit */
    Job t3[] = {{0, 0, 2}, {1, 2, 4}, {2, 4, 6}};
    check("Half-open, Earliest Finish  ", earliest_finish, t3, 3, (int[]){0, 1, 2}, 3);
    check("Half-open, Earliest Start   ", earliest_start, t3, 3, (int[]){0, 1, 2}, 3);
    check("Half-open, Shortest Duration", shortest_duration, t3, 3, (int[]){0, 1, 2}, 3);
    check("Half-open, Fewest Conflicts ", fewest_conflicts, t3, 3, (int[]){0, 1, 2}, 3);

    /* Job 0 overlaps the other three; they each overlap only job 0.
     * Fewest Conflicts should skip job 0 and pick 1, 2, 3. */
    Job t4[] = {{0, 0, 10}, {1, 1, 3}, {2, 4, 6}, {3, 7, 9}};
    check("Fewest Conflicts avoids long job", fewest_conflicts, t4, 4,
          (int[]){1, 2, 3}, 3);

    /* Three identical jobs: every heuristic should pick the lowest id */
    Job t5[] = {{0, 1, 3}, {1, 1, 3}, {2, 1, 3}};
    check("Ties, Earliest Finish  ", earliest_finish, t5, 3, (int[]){0}, 1);
    check("Ties, Earliest Start   ", earliest_start, t5, 3, (int[]){0}, 1);
    check("Ties, Shortest Duration", shortest_duration, t5, 3, (int[]){0}, 1);
    check("Ties, Fewest Conflicts ", fewest_conflicts, t5, 3, (int[]){0}, 1);

    /* Shortest Duration picks the middle job first, then the ones on
     * either side of it. */
    Job t6[] = {{0, 0, 4}, {1, 4, 5}, {2, 5, 9}};
    check("Shortest Duration fills both sides", shortest_duration, t6, 3,
          (int[]){0, 1, 2}, 3);

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
           failures, failures == 1 ? "" : "s");
    return failures != 0;
}
