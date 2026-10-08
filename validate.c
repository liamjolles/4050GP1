/* Part 4: check Earliest Finish against the exact solver on many random
 * small instances from several families (see gen.c).
 *
 * Part 5: on the same instances, compare all four heuristics with OPT:
 * how often each is optimal, its average |H|/OPT, and its worst instance.
 *
 * Part 6: time the greedy algorithms (not the exact solver) on large
 * instances, plus the sorting step of A, B and C on its own.
 *
 * Usage:
 *   ./validate [seed] [per_family] [outdir]                   Parts 4 and 5
 *     defaults: 4050 200 results    (5 families x 200 = 1000 instances)
 *   ./validate time [seed] [reps] [max_n] [families] [outdir] Part 6
 *     defaults: 4050 5 100000 sparse,dense results
 *     n = 100, 1000, ... up to max_n; families is a comma list or "all"
 *
 * Parts 4 and 5 write, in outdir (which must exist):
 *   instances.csv       one row per instance: sizes found by OPT and A-D
 *   jobs.csv            the intervals of every instance
 *   part4_summary.csv   matched / tested per family and overall
 *   part5_summary.csv   per heuristic and family: optimal rate, average
 *                       and worst |H|/OPT
 *   part5_worst.txt     the worst instance of each heuristic, in full
 * Part 6 writes:
 *   part6_timing.csv    median / min / max seconds per call for every
 *                       family, algorithm and n
 *
 * Parts 4 and 5 exit with 1 if Earliest Finish ever misses OPT, or if any
 * solution has overlapping or invalid jobs. */

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gen.h"
#include "interval.h"

#define N_MIN 4
#define N_MAX 18 /* exact solver does 2^18 * 18 steps at most here */

static const struct { const char *col, *name; Heuristic run; } HEURISTICS[] = {
    {"eft", "earliest finish",   earliest_finish},
    {"est", "earliest start",    earliest_start},
    {"sd",  "shortest duration", shortest_duration},
    {"fc",  "fewest conflicts",  fewest_conflicts},
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

/* ---- Part 5: solution quality ---- */

/* How one heuristic did on a group of instances. The worst instance is
 * the lowest |H|/OPT; ties go to the bigger gap OPT - |H|, then to the
 * first one seen. It is kept as (family, index) so it can be rebuilt. */
typedef struct {
    int tested, optimal;
    double ratio_sum;
    double worst;
    int worst_h, worst_opt;
    int worst_fam, worst_idx, worst_id; /* worst_id < 0: nothing yet */
} Quality;

static void note_quality(Quality *q, int h, int opt, int fam, int idx, int id) {
    double ratio = (double)h / opt;
    q->tested++;
    q->optimal += h == opt;
    q->ratio_sum += ratio;
    if (q->worst_id < 0 || ratio < q->worst ||
        (ratio == q->worst && opt - h > q->worst_opt - q->worst_h)) {
        q->worst = ratio;
        q->worst_h = h;
        q->worst_opt = opt;
        q->worst_fam = fam;
        q->worst_idx = idx;
        q->worst_id = id;
    }
}

/* Rebuild the worst instance of heuristic h and write it out in full. */
static void write_worst(FILE *out, uint64_t master, int h, const Quality *q) {
    Job jobs[N_MAX];
    int sel[N_MAX], opt_sel[N_MAX];
    uint64_t seed = instance_seed(master, q->worst_fam, q->worst_idx);
    Rng r = rng_seed(seed);
    int n = rng_range(&r, N_MIN, N_MAX);
    gen_instance(q->worst_fam, n, &r, jobs);
    int k = HEURISTICS[h].run(jobs, n, sel);
    int opt = exact_solver(jobs, n, opt_sel);

    fprintf(out, "%s: |H|/OPT = %d/%d = %.4f\n", HEURISTICS[h].name, k, opt,
            (double)k / opt);
    fprintf(out, "  instance %d (%s, seed %" PRIu64 ", n = %d)\n  jobs:",
            q->worst_id, family_name(q->worst_fam), seed, n);
    for (int j = 0; j < n; j++) fprintf(out, " %d[%d,%d)", j, jobs[j].s, jobs[j].f);
    fprintf(out, "\n  %-4s", HEURISTICS[h].col);
    print_sel(out, jobs, sel, k);
    fprintf(out, "  opt ");
    print_sel(out, jobs, opt_sel, opt);
    fprintf(out, "\n");
}

static void print_part5(FILE *summary, FILE *worst, uint64_t master,
                        Quality q[NH][FAM_COUNT + 1]) {
    printf("\nPart 5: heuristics vs exact solver, |H|/OPT\n\n");
    printf("%-18s %9s %12s %12s  %s\n", "heuristic", "optimal", "avg |H|/OPT",
           "worst", "worst instance");
    for (int h = 0; h < NH; h++) {
        const Quality *a = &q[h][FAM_COUNT];
        printf("%-18s %8.2f%% %12.4f %12.4f  #%d (%s, %d/%d)\n", HEURISTICS[h].name,
               100.0 * a->optimal / a->tested, a->ratio_sum / a->tested, a->worst,
               a->worst_id, family_name(a->worst_fam), a->worst_h, a->worst_opt);
    }

    /* per family: optimal rate / average ratio */
    printf("\nBy family (optimal %% / avg |H|/OPT / worst):\n%-18s", "heuristic");
    for (int fam = 0; fam < FAM_COUNT; fam++) printf(" %24s", family_name(fam));
    printf("\n");
    for (int h = 0; h < NH; h++) {
        printf("%-18s", HEURISTICS[h].name);
        for (int fam = 0; fam < FAM_COUNT; fam++) {
            const Quality *c = &q[h][fam];
            printf("  %6.1f%% / %.3f / %.3f", 100.0 * c->optimal / c->tested,
                   c->ratio_sum / c->tested, c->worst);
        }
        printf("\n");
    }

    fprintf(summary, "heuristic,family,tested,optimal,optimal_rate,avg_ratio,"
                     "worst_ratio,worst_instance,worst_h,worst_opt\n");
    for (int h = 0; h < NH; h++)
        for (int fam = 0; fam <= FAM_COUNT; fam++) {
            const Quality *c = &q[h][fam];
            fprintf(summary, "%s,%s,%d,%d,%.4f,%.4f,%.4f,%d,%d,%d\n",
                    HEURISTICS[h].col, fam < FAM_COUNT ? family_name(fam) : "all",
                    c->tested, c->optimal, (double)c->optimal / c->tested,
                    c->ratio_sum / c->tested, c->worst, c->worst_id, c->worst_h,
                    c->worst_opt);
        }

    fprintf(worst, "Worst instance per heuristic (seed %" PRIu64 ")\n"
                   "Jobs are id[s,f); selections list the ids each one picked.\n\n",
            master);
    for (int h = 0; h < NH; h++) write_worst(worst, master, h, &q[h][FAM_COUNT]);
}

/* ---- Parts 4 and 5 ---- */

static int run_validation(uint64_t master, int per_family, const char *outdir) {
    FILE *inst = open_out(outdir, "instances.csv");
    FILE *jobs_csv = open_out(outdir, "jobs.csv");
    FILE *summary = open_out(outdir, "part4_summary.csv");
    FILE *summary5 = open_out(outdir, "part5_summary.csv");
    FILE *worst5 = open_out(outdir, "part5_worst.txt");
    if (!inst || !jobs_csv || !summary || !summary5 || !worst5) return 1;

    fprintf(inst, "instance,family,seed,n,opt");
    for (int h = 0; h < NH; h++) fprintf(inst, ",%s", HEURISTICS[h].col);
    fprintf(inst, ",eft_match\n");
    fprintf(jobs_csv, "instance,job,s,f\n");
    fprintf(summary, "family,tested,matched,match_rate\n");

    Job jobs[N_MAX];
    int opt_sel[N_MAX], sel[N_MAX];
    int tested[FAM_COUNT] = {0}, matched[FAM_COUNT] = {0};
    int bad = 0; /* infeasible answers, or a heuristic beating "OPT" */

    /* quality[h][fam]; the extra last column is all families together */
    Quality quality[NH][FAM_COUNT + 1];
    memset(quality, 0, sizeof quality);
    for (int h = 0; h < NH; h++)
        for (int fam = 0; fam <= FAM_COUNT; fam++) quality[h][fam].worst_id = -1;

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
                note_quality(&quality[h][fam], size[h], opt, fam, i, id);
                note_quality(&quality[h][FAM_COUNT], size[h], opt, fam, i, id);
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

    print_part5(summary5, worst5, master, quality);
    printf("\nWorst instances written to %s/part5_worst.txt\n", outdir);

    fclose(inst);
    fclose(jobs_csv);
    fclose(summary);
    fclose(summary5);
    fclose(worst5);
    return (all_matched != all_tested || bad) ? 1 : 0;
}

/* ---- Part 6: runtime ---- */

#define MIN_SAMPLE 0.01  /* seconds; shorter calls are repeated until this */
#define REP_BUDGET 30.0  /* seconds; past this, stop at MIN_REPS repetitions */
#define MIN_REPS 3

/* The sorting step of A, B and C alone: copy the jobs and qsort them,
 * just as greedy_by_order does before its scan. Same signature as a
 * heuristic so it can be timed the same way. */
static int sort_with(const Job *jobs, int n, int *sel,
                     int (*cmp)(const void *, const void *)) {
    Job *order = malloc(n * sizeof *order);
    memcpy(order, jobs, n * sizeof *order);
    qsort(order, n, sizeof *order, cmp);
    sel[0] = order[0].id; /* use the result so the work can't be dropped */
    free(order);
    return 0;
}
static int sort_finish(const Job *j, int n, int *s) { return sort_with(j, n, s, order_finish); }
static int sort_start(const Job *j, int n, int *s) { return sort_with(j, n, s, order_start); }
static int sort_duration(const Job *j, int n, int *s) { return sort_with(j, n, s, order_duration); }

static const struct { const char *col; Heuristic run; } TIMED[] = {
    {"eft", earliest_finish},
    {"est", earliest_start},
    {"sd", shortest_duration},
    {"fc", fewest_conflicts},
    {"sort_finish", sort_finish},
    {"sort_start", sort_start},
    {"sort_duration", sort_duration},
};
#define NT ((int)(sizeof TIMED / sizeof *TIMED))

static double now(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* Seconds per call of run on jobs. Small inputs finish faster than the
 * clock can resolve, so the call is repeated for at least MIN_SAMPLE. */
static double time_once(Heuristic run, const Job *jobs, int n, int *sel) {
    long calls = 0;
    double start = now(), elapsed;
    do {
        run(jobs, n, sel);
        calls++;
        elapsed = now() - start;
    } while (elapsed < MIN_SAMPLE);
    return elapsed / calls;
}

static int cmp_double(const void *x, const void *y) {
    double a = *(const double *)x, b = *(const double *)y;
    return (a > b) - (a < b);
}

/* families is "all" or a comma list of names; sets want[fam]. */
static int parse_families(const char *families, int want[FAM_COUNT]) {
    int any = 0;
    for (int fam = 0; fam < FAM_COUNT; fam++) {
        const char *p = families;
        size_t len = strlen(family_name(fam));
        want[fam] = strcmp(families, "all") == 0;
        while (!want[fam] && (p = strstr(p, family_name(fam))) != NULL) {
            int starts = p == families || p[-1] == ',';
            int ends = p[len] == '\0' || p[len] == ',';
            want[fam] = starts && ends;
            p += len;
        }
        any |= want[fam];
    }
    return any;
}

/* Each repetition uses a fresh random instance of the same family and n,
 * and every algorithm runs on that same instance. The median over the
 * repetitions is reported. Slow cases (fewest conflicts at large n) stop
 * after MIN_REPS once they have used REP_BUDGET seconds. */
static int run_timing(uint64_t master, int reps, int max_n, const char *families,
                      const char *outdir) {
    int want[FAM_COUNT];
    if (!parse_families(families, want)) {
        fprintf(stderr, "error: no known family in \"%s\"\n", families);
        return 1;
    }
    FILE *csv = open_out(outdir, "part6_timing.csv");
    if (!csv) return 1;
    fprintf(csv, "family,algorithm,n,reps,median_sec,min_sec,max_sec,picked\n");

    Job *jobs = malloc(max_n * sizeof *jobs);
    int *sel = malloc(max_n * sizeof *sel);
    double *samples = malloc(reps * sizeof *samples);

    printf("Part 6: greedy running times (seed %" PRIu64 ", median of up to %d runs)\n",
           master, reps);
    for (int fam = 0; fam < FAM_COUNT; fam++) {
        if (!want[fam]) continue;
        printf("\nfamily %s\n%-14s %8s %5s %12s %14s %16s %8s %8s\n", family_name(fam),
               "algorithm", "n", "runs", "median (s)", "ns per job",
               "ns / (n log2 n)", "x prev", "picked");
        for (int a = 0; a < NT; a++) {
            double prev = 0;
            for (int n = 100; n <= max_n; n *= 10) {
                int done = 0, picked = 0;
                double spent = 0;
                for (int rep = 0; rep < reps; rep++) {
                    if (rep >= MIN_REPS && spent > REP_BUDGET) break;
                    Rng r = rng_seed(instance_seed(master ^ (uint64_t)n, fam, rep));
                    gen_instance(fam, n, &r, jobs);
                    double t0 = now();
                    samples[done++] = time_once(TIMED[a].run, jobs, n, sel);
                    spent += now() - t0;
                    if (rep == 0) picked = TIMED[a].run(jobs, n, sel);
                }
                qsort(samples, done, sizeof *samples, cmp_double);
                double med = done % 2 ? samples[done / 2]
                                      : (samples[done / 2 - 1] + samples[done / 2]) / 2;
                printf("%-14s %8d %5d %12.6f %14.2f %16.3f", TIMED[a].col, n, done, med,
                       1e9 * med / n, 1e9 * med / (n * log2(n)));
                if (prev > 0) printf(" %8.1f", med / prev);
                else printf(" %8s", "-");
                if (a < NH) printf(" %8d\n", picked);
                else printf(" %8s\n", "-");
                fflush(stdout);
                fprintf(csv, "%s,%s,%d,%d,%.9f,%.9f,%.9f,%d\n", family_name(fam),
                        TIMED[a].col, n, done, med, samples[0], samples[done - 1],
                        a < NH ? picked : -1);
                prev = med;
            }
        }
    }

    free(jobs);
    free(sel);
    free(samples);
    fclose(csv);
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "time") == 0) {
        uint64_t master = argc > 2 ? strtoull(argv[2], NULL, 10) : 4050;
        int reps = argc > 3 ? atoi(argv[3]) : 5;
        int max_n = argc > 4 ? atoi(argv[4]) : 100000;
        const char *families = argc > 5 ? argv[5] : "sparse,dense";
        const char *outdir = argc > 6 ? argv[6] : "results";
        if (reps <= 0 || max_n < 100) {
            fprintf(stderr, "error: reps must be positive and max_n at least 100\n");
            return 1;
        }
        return run_timing(master, reps, max_n, families, outdir);
    }

    uint64_t master = argc > 1 ? strtoull(argv[1], NULL, 10) : 4050;
    int per_family = argc > 2 ? atoi(argv[2]) : 200;
    const char *outdir = argc > 3 ? argv[3] : "results";
    if (per_family <= 0) {
        fprintf(stderr, "error: per_family must be positive\n");
        return 1;
    }
    return run_validation(master, per_family, outdir);
}
