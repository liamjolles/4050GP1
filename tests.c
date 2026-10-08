/* Part 1 tests: the two required sanity tests plus a few small cases.
 * Each test checks that a heuristic picks exactly the expected jobs and
 * that none of them overlap. */

#include <stdio.h>
#include "gen.h"
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

    /* Textbook Fewest Conflicts trap: job 4 [7,9) has the fewest conflicts
     * (2), but taking it removes jobs 1 and 2 from the optimal chain
     * 0,1,2,3. Fewest Conflicts then gets one job per side: 3 vs OPT 4. */
    Job t7[] = {{0, 0, 4},  {1, 4, 8},   {2, 8, 12},  {3, 12, 16},
                {4, 7, 9},  {5, 3, 5},   {6, 3, 5},   {7, 3, 5},
                {8, 11, 13}, {9, 11, 13}, {10, 11, 13}};
    check("Fewest Conflicts trap picks 0,4,8", fewest_conflicts, t7, 11,
          (int[]){0, 4, 8}, 3);
    check("Fewest Conflicts trap, Exact OPT 4", exact_solver, t7, 11,
          (int[]){0, 1, 2, 3}, 4);

    /* Generator: same seed gives the same instance, a different seed a
     * different one, and every job is valid (s < f, id = position). */
    for (int fam = 0; fam < FAM_COUNT; fam++) {
        Job a[12], b[12], c[12];
        Rng ra = rng_seed(7), rb = rng_seed(7), rc = rng_seed(8);
        gen_instance(fam, 12, &ra, a);
        gen_instance(fam, 12, &rb, b);
        gen_instance(fam, 12, &rc, c);
        int same = 1, differs = 0;
        for (int i = 0; i < 12; i++) {
            same &= a[i].s == b[i].s && a[i].f == b[i].f;
            differs |= a[i].s != c[i].s || a[i].f != c[i].f;
        }
        printf("%s  Generator %-9s same seed -> same jobs\n",
               same ? "PASS" : "FAIL", family_name(fam));
        printf("%s  Generator %-9s new seed -> new jobs\n",
               differs ? "PASS" : "FAIL", family_name(fam));
        failures += !same + !differs;

        int valid = 1;
        for (uint64_t seed = 0; seed < 200; seed++) {
            for (int n = 1; n <= 20; n++) {
                Job g[20];
                Rng r = rng_seed(seed);
                gen_instance(fam, n, &r, g);
                for (int i = 0; i < n; i++)
                    valid &= g[i].id == i && g[i].s < g[i].f;
            }
        }
        printf("%s  Generator %-9s jobs have s < f and ids 0..n-1\n",
               valid ? "PASS" : "FAIL", family_name(fam));
        failures += !valid;
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
           failures, failures == 1 ? "" : "s");
    return failures != 0;
}
