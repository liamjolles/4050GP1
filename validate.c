/* Part 4: check Earliest Finish against the exact solver on many random
 * small instances from several families (see gen.c).
 *
 * All four heuristics are run and recorded, so Part 5 can be worked out
 * from the same instances without running anything again.
 *
 * Usage: ./validate [seed] [per_family] [outdir]
 *   defaults: 4050 200 results    (5 families x 200 = 1000 instances)
 *
 * Writes, in outdir (which must exist):
 *   instances.csv       one row per instance: sizes found by OPT and A-D
 *   jobs.csv            the intervals of every instance
 *   part4_summary.csv   matched / tested per family and overall
 *
 * Exits with 1 if Earliest Finish ever misses OPT, or if any solution
 * has overlapping or invalid jobs. */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen.h"
#include "interval.h"

#define N_MIN 4
#define N_MAX 18 /* exact solver does 2^18 * 18 steps at most here */

static const struct { const char *col; Heuristic run; } HEURISTICS[] = {
    {"eft", earliest_finish},
    {"est", earliest_start},
    {"sd",  shortest_duration},
    {"fc",  fewest_conflicts},
};
#define NH ((int)(sizeof HEURISTICS / sizeof *HEURISTICS))

/* True if sel holds k distinct valid job ids, no two of which overlap.
 * Checked separately from the algorithms so a bad answer can't hide. */
static int feasible(const Job *jobs, int n, const int *sel, int k) {
    char seen[N_MAX] = {0};
    for (int i = 0; i < k; i++) {
        if (sel[i] < 0 || sel[i] >= n || seen[sel[i]]) return 0;
        seen[sel[i]] = 1;
    }
    for (int i = 0; i < k; i++)
        for (int j = i + 1; j < k; j++)
            if (overlaps(&jobs[sel[i]], &jobs[sel[j]])) return 0;
    return 1;
}

static void print_sel(FILE *out, const Job *jobs, const int *sel, int k) {
    for (int i = 0; i < k; i++)
        fprintf(out, " %d[%d,%d)", sel[i], jobs[sel[i]].s, jobs[sel[i]].f);
    fprintf(out, "\n");
}

static FILE *open_out(const char *dir, const char *name) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) perror(path);
    return f;
}

int main(int argc, char **argv) {
    uint64_t master = argc > 1 ? strtoull(argv[1], NULL, 10) : 4050;
    int per_family = argc > 2 ? atoi(argv[2]) : 200;
    const char *outdir = argc > 3 ? argv[3] : "results";
    if (per_family <= 0) {
        fprintf(stderr, "error: per_family must be positive\n");
        return 1;
    }

    FILE *inst = open_out(outdir, "instances.csv");
    FILE *jobs_csv = open_out(outdir, "jobs.csv");
    FILE *summary = open_out(outdir, "part4_summary.csv");
    if (!inst || !jobs_csv || !summary) return 1;

    fprintf(inst, "instance,family,seed,n,opt");
    for (int h = 0; h < NH; h++) fprintf(inst, ",%s", HEURISTICS[h].col);
    fprintf(inst, ",eft_match\n");
    fprintf(jobs_csv, "instance,job,s,f\n");
    fprintf(summary, "family,tested,matched,match_rate\n");

    Job jobs[N_MAX];
    int opt_sel[N_MAX], sel[N_MAX];
    int tested[FAM_COUNT] = {0}, matched[FAM_COUNT] = {0};
    int bad = 0; /* infeasible answers, or a heuristic beating "OPT" */

    for (int fam = 0; fam < FAM_COUNT; fam++) {
        for (int i = 0; i < per_family; i++) {
            int id = fam * per_family + i;
            uint64_t seed = instance_seed(master, fam, i);
            Rng r = rng_seed(seed);
            int n = rng_range(&r, N_MIN, N_MAX);
            gen_instance(fam, n, &r, jobs);

            for (int j = 0; j < n; j++)
                fprintf(jobs_csv, "%d,%d,%d,%d\n", id, j, jobs[j].s, jobs[j].f);

            int opt = exact_solver(jobs, n, opt_sel);
            if (!feasible(jobs, n, opt_sel, opt)) {
                fprintf(stderr, "instance %d: exact solution is not feasible\n", id);
                bad++;
            }

            int size[NH];
            for (int h = 0; h < NH; h++) {
                size[h] = HEURISTICS[h].run(jobs, n, sel);
                if (!feasible(jobs, n, sel, size[h]) || size[h] > opt) {
                    fprintf(stderr, "instance %d: %s returned a bad solution:",
                            id, HEURISTICS[h].col);
                    print_sel(stderr, jobs, sel, size[h]);
                    bad++;
                }
                if (h == 0 && size[h] != opt) {
                    fprintf(stderr, "instance %d (%s, seed %" PRIu64 "): "
                            "Earliest Finish %d != OPT %d\n  jobs:",
                            id, family_name(fam), seed, size[h], opt);
                    for (int j = 0; j < n; j++)
                        fprintf(stderr, " %d[%d,%d)", j, jobs[j].s, jobs[j].f);
                    fprintf(stderr, "\n  EFT:");
                    print_sel(stderr, jobs, sel, size[h]);
                    fprintf(stderr, "  OPT:");
                    print_sel(stderr, jobs, opt_sel, opt);
                }
            }

            int match = size[0] == opt;
            tested[fam]++;
            matched[fam] += match;

            fprintf(inst, "%d,%s,%" PRIu64 ",%d,%d", id, family_name(fam), seed, n, opt);
            for (int h = 0; h < NH; h++) fprintf(inst, ",%d", size[h]);
            fprintf(inst, ",%d\n", match);
        }
    }

    int all_tested = 0, all_matched = 0;
    printf("Part 4: Earliest Finish vs exact solver (seed %" PRIu64 ", n in [%d, %d])\n\n",
           master, N_MIN, N_MAX);
    printf("%-10s %7s %8s %8s\n", "family", "tested", "matched", "rate");
    for (int fam = 0; fam < FAM_COUNT; fam++) {
        double rate = (double)matched[fam] / tested[fam];
        printf("%-10s %7d %8d %7.2f%%\n", family_name(fam), tested[fam], matched[fam],
               100 * rate);
        fprintf(summary, "%s,%d,%d,%.4f\n", family_name(fam), tested[fam], matched[fam],
                rate);
        all_tested += tested[fam];
        all_matched += matched[fam];
    }
    double all_rate = (double)all_matched / all_tested;
    printf("%-10s %7d %8d %7.2f%%\n\n", "all", all_tested, all_matched, 100 * all_rate);
    fprintf(summary, "all,%d,%d,%.4f\n", all_tested, all_matched, all_rate);

    printf("Earliest Finish matched OPT on %d/%d instances (%.2f%%)\n",
           all_matched, all_tested, 100 * all_rate);
    if (bad) printf("%d invalid solution%s found (see stderr)\n", bad, bad == 1 ? "" : "s");

    fclose(inst);
    fclose(jobs_csv);
    fclose(summary);
    return (all_matched != all_tested || bad) ? 1 : 0;
}
