/* Part 3: automatically find instances where a greedy heuristic is not
 * optimal, for Earliest Start (B), Shortest Duration (C) and Fewest
 * Conflicts (D).
 *
 * Three searches are run for each heuristic H:
 *   exhaustive  every set of n <= EX_N_MAX jobs with endpoints in
 *               [0, EX_T], smallest n first, stopping at the first hit.
 *               Each set is tried once, with its jobs numbered in order
 *               of (start, finish).
 *   random      RANDOM_PER_FAMILY instances of every gen.c family
 *               (n in [4, 18]); every hit is counted.
 *   local       random restarts of a hill climb: change one job at a
 *               time and keep the change unless H's gap to OPT shrinks.
 *               The first hit is shrunk by dropping every job it can
 *               do without.
 *
 * A candidate I counts as a hit when |H(I)| < |EarliestFinish(I)|. That
 * is a proof H is suboptimal on I whether or not Earliest Finish is
 * optimal, because its answer is a valid schedule, so OPT(I) >= |EFT(I)|.
 * It is fast enough to check millions of instances. Every hit is then
 * confirmed with the exact solver, and OPT from the exact solver is what
 * gets reported. (Concluding "no counterexample exists" from the
 * exhaustive search does rely on Earliest Finish being optimal.)
 *
 * Usage: ./search [seed] [outdir]      defaults: 4050 results
 *
 * Writes, in outdir (which must exist):
 *   part3_counterexamples.csv  best counterexample per heuristic and search
 *   part3_search.csv           instances tried and hits per search
 *   part3_timelines.txt        the chosen counterexamples, with timelines
 *
 * Exits with 1 if the exact solver ever disagrees with a hit. */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gen.h"
#include "interval.h"

#define MAX_N 24

#define EX_N_MAX 7 /* exhaustive: at most this many jobs */
#define EX_T 8     /* exhaustive: endpoints in [0, EX_T] */

#define RANDOM_PER_FAMILY 2000
#define RANDOM_N_MIN 4
#define RANDOM_N_MAX 18
#define RANDOM_INDEX_BASE 1000000 /* keeps these apart from Part 4's instances */

#define LS_N_MIN 8
#define LS_N_MAX 14
#define LS_LEN_MAX 4
#define LS_STEPS 20000 /* per restart */
#define LS_RESTARTS 500

static const struct { const char *col, *name; Heuristic run; } TARGETS[] = {
    {"est", "Earliest Start",    earliest_start},
    {"sd",  "Shortest Duration", shortest_duration},
    {"fc",  "Fewest Conflicts",  fewest_conflicts},
};
#define NTARGET ((int)(sizeof TARGETS / sizeof *TARGETS))

enum { ST_EXHAUSTIVE, ST_RANDOM, ST_LOCAL, NSTAGE };
static const char *STAGE_NAMES[NSTAGE] = {"exhaustive", "random", "local"};

/* A confirmed counterexample: its jobs, what H and OPT picked. */
typedef struct {
    int have;
    int n, h, opt;
    Job jobs[MAX_N];
    int h_sel[MAX_N], opt_sel[MAX_N];
} Found;

typedef struct {
    long tried, hits;
    double seconds;
    double worst; /* lowest |H|/OPT over hits */
} Stats;

static int bad = 0; /* hits the exact solver did not confirm */

static double now(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* |EFT(I)| - |H(I)|. Positive means H is suboptimal on I. */
static int gap(Heuristic h, const Job *jobs, int n) {
    int sel[MAX_N];
    return earliest_finish(jobs, n, sel) - h(jobs, n, sel);
}

static void renumber(Job *jobs, int n) {
    for (int i = 0; i < n; i++) jobs[i].id = i;
}

/* Drop jobs one at a time for as long as I stays a counterexample. */
static int shrink(Heuristic h, Job *jobs, int n) {
    for (int i = 0; i < n;) {
        Job fewer[MAX_N];
        int m = 0;
        for (int j = 0; j < n; j++)
            if (j != i) fewer[m++] = jobs[j];
        renumber(fewer, m);
        if (m > 0 && gap(h, fewer, m) > 0) {
            memcpy(jobs, fewer, m * sizeof *jobs);
            n = m;
            i = 0;
        } else {
            i++;
        }
    }
    return n;
}

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}

/* Make a counterexample easier to read: list the jobs by start, then
 * finish, and replace the endpoints by their ranks 0, 1, 2, ... Both keep
 * every overlap the same, but they can change ties (and relabelling
 * changes lengths), so each is only kept if I still fails. */
static void tidy(Heuristic h, Job *jobs, int n) {
    Job t[MAX_N];
    memcpy(t, jobs, n * sizeof *t);
    qsort(t, n, sizeof *t, order_start);
    renumber(t, n);
    if (gap(h, t, n) <= 0) return;
    memcpy(jobs, t, n * sizeof *jobs);

    int ends[2 * MAX_N], m = 0, k = 0;
    for (int i = 0; i < n; i++) { ends[m++] = t[i].s; ends[m++] = t[i].f; }
    qsort(ends, m, sizeof *ends, cmp_int);
    for (int i = 0; i < m; i++)
        if (i == 0 || ends[i] != ends[i - 1]) ends[k++] = ends[i];
    for (int i = 0; i < n; i++) {
        int *s = bsearch(&t[i].s, ends, k, sizeof *ends, cmp_int);
        int *f = bsearch(&t[i].f, ends, k, sizeof *ends, cmp_int);
        t[i].s = (int)(s - ends);
        t[i].f = (int)(f - ends);
    }
    if (gap(h, t, n) > 0) memcpy(jobs, t, n * sizeof *jobs);
}

/* A hit: tidy it, confirm it with the exact solver, update the stats,
 * and keep it if it beats best (fewer jobs, then a bigger gap). */
static void record(int t, const Job *in, int n, Found *best, Stats *st) {
    Found c = {0};
    c.n = n;
    memcpy(c.jobs, in, n * sizeof *c.jobs);
    tidy(TARGETS[t].run, c.jobs, n);
    c.h = TARGETS[t].run(c.jobs, n, c.h_sel);
    c.opt = exact_solver(c.jobs, n, c.opt_sel);
    if (c.opt <= c.h) {
        fprintf(stderr, "error: %s hit not confirmed by exact solver (|H| %d, OPT %d)\n",
                TARGETS[t].col, c.h, c.opt);
        bad++;
        return;
    }
    c.have = 1;
    double ratio = (double)c.h / c.opt;
    if (st->hits == 0 || ratio < st->worst) st->worst = ratio;
    st->hits++;
    if (!best->have || c.n < best->n ||
        (c.n == best->n && c.opt - c.h > best->opt - best->h))
        *best = c;
}

/* ---- the three searches ---- */

static void search_exhaustive(int t, Found *best, Stats *st) {
    Job grid[(EX_T + 1) * EX_T / 2];
    int ng = 0;
    for (int s = 0; s < EX_T; s++)
        for (int f = s + 1; f <= EX_T; f++) grid[ng++] = (Job){0, s, f};

    for (int n = 1; n <= EX_N_MAX; n++) {
        /* idx is non-decreasing, so each set of jobs is tried once */
        int idx[EX_N_MAX] = {0};
        for (;;) {
            Job jobs[EX_N_MAX];
            for (int i = 0; i < n; i++) {
                jobs[i] = grid[idx[i]];
                jobs[i].id = i;
            }
            st->tried++;
            if (gap(TARGETS[t].run, jobs, n) > 0) {
                record(t, jobs, n, best, st);
                return;
            }
            int p = n - 1;
            while (p >= 0 && idx[p] == ng - 1) p--;
            if (p < 0) break;
            idx[p]++;
            for (int i = p + 1; i < n; i++) idx[i] = idx[p];
        }
    }
}

static void search_random(int t, uint64_t master, Found *best, Stats *st) {
    for (int fam = 0; fam < FAM_COUNT; fam++) {
        for (int i = 0; i < RANDOM_PER_FAMILY; i++) {
            Job jobs[MAX_N];
            Rng r = rng_seed(instance_seed(master, fam, RANDOM_INDEX_BASE + i));
            int n = rng_range(&r, RANDOM_N_MIN, RANDOM_N_MAX);
            gen_instance(fam, n, &r, jobs);
            st->tried++;
            if (gap(TARGETS[t].run, jobs, n) > 0) record(t, jobs, n, best, st);
        }
    }
}

static Job random_job(Rng *r, int id, int n) {
    int s = rng_range(r, 0, 2 * n - 1);
    return (Job){id, s, s + rng_range(r, 1, LS_LEN_MAX)};
}

static void search_local(int t, uint64_t master, Found *best, Stats *st) {
    Heuristic h = TARGETS[t].run;
    Rng r = rng_seed(instance_seed(master, FAM_COUNT + t, 0));
    for (int restart = 0; restart < LS_RESTARTS; restart++) {
        Job jobs[MAX_N];
        int n = rng_range(&r, LS_N_MIN, LS_N_MAX);
        for (int i = 0; i < n; i++) jobs[i] = random_job(&r, i, n);
        int g = gap(h, jobs, n);
        st->tried++;
        for (int step = 0; step < LS_STEPS && g <= 0; step++) {
            int i = rng_range(&r, 0, n - 1);
            Job old = jobs[i];
            jobs[i] = random_job(&r, i, n);
            int g2 = gap(h, jobs, n);
            st->tried++;
            if (g2 >= g) g = g2;
            else jobs[i] = old;
        }
        if (g > 0) {
            n = shrink(h, jobs, n);
            record(t, jobs, n, best, st);
            return;
        }
    }
}

/* ---- output ---- */

static void print_jobs(FILE *out, const Job *jobs, const int *sel, int k) {
    for (int i = 0; i < k; i++)
        fprintf(out, " %d[%d,%d)", sel[i], jobs[sel[i]].s, jobs[sel[i]].f);
    fprintf(out, "\n");
}

static int picked(const int *sel, int k, int id) {
    for (int i = 0; i < k; i++)
        if (sel[i] == id) return 1;
    return 0;
}

/* One row per job, with # over the times it runs: column c is [c, c+1).
 * H marks the jobs the heuristic picked, O the jobs OPT picked. */
static void print_timeline(FILE *out, const Found *c) {
    int lo = c->jobs[0].s, hi = c->jobs[0].f;
    for (int i = 1; i < c->n; i++) {
        if (c->jobs[i].s < lo) lo = c->jobs[i].s;
        if (c->jobs[i].f > hi) hi = c->jobs[i].f;
    }
    fprintf(out, "  %-8s", "time");
    for (int x = lo; x <= hi; x++) fputc(x % 10 == 0 ? '0' + (x / 10) % 10 : ' ', out);
    fprintf(out, "\n  %-8s", "");
    for (int x = lo; x <= hi; x++) fputc('0' + x % 10, out);
    fprintf(out, "\n");
    for (int i = 0; i < c->n; i++) {
        fprintf(out, "  job %-4d", i);
        for (int x = lo; x <= hi; x++)
            fputc(x >= c->jobs[i].s && x < c->jobs[i].f ? '#' : '.', out);
        fprintf(out, "  %c %c\n", picked(c->h_sel, c->h, i) ? 'H' : ' ',
                picked(c->opt_sel, c->opt, i) ? 'O' : ' ');
    }
}

static void print_found(FILE *out, int t, int stage, const Found *c) {
    fprintf(out, "%s counterexample (from %s search, n = %d)\n", TARGETS[t].name,
            STAGE_NAMES[stage], c->n);
    fprintf(out, "  jobs:");
    for (int i = 0; i < c->n; i++) fprintf(out, " %d[%d,%d)", i, c->jobs[i].s, c->jobs[i].f);
    fprintf(out, "\n  %-4s:", TARGETS[t].col);
    print_jobs(out, c->jobs, c->h_sel, c->h);
    fprintf(out, "  opt :");
    print_jobs(out, c->jobs, c->opt_sel, c->opt);
    fprintf(out, "  |Greedy(I)| = %d, OPT(I) = %d\n\n", c->h, c->opt);
    print_timeline(out, c);
    fprintf(out, "\n");
}

static void csv_list(FILE *out, const Job *jobs, const int *sel, int k, int intervals) {
    for (int i = 0; i < k; i++) {
        if (i) fputc(';', out);
        if (intervals) fprintf(out, "%d-%d", jobs[i].s, jobs[i].f);
        else fprintf(out, "%d", sel[i]);
    }
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
    const char *outdir = argc > 2 ? argv[2] : "results";

    FILE *ce = open_out(outdir, "part3_counterexamples.csv");
    FILE *stats = open_out(outdir, "part3_search.csv");
    FILE *tl = open_out(outdir, "part3_timelines.txt");
    if (!ce || !stats || !tl) return 1;
    fprintf(ce, "heuristic,search,chosen,n,h_size,opt,jobs,h_sel,opt_sel\n");
    fprintf(stats, "heuristic,search,instances_tried,counterexamples,smallest_n,worst_ratio\n");
    fprintf(tl, "Part 3 counterexamples (seed %" PRIu64 ")\n"
                "Jobs are id[s,f). In the timelines column x is the time step [x, x+1);\n"
                "H marks the jobs the heuristic picked, O the jobs an optimal solution picked.\n\n",
            master);

    printf("Part 3: counterexample search (seed %" PRIu64 ")\n", master);
    printf("  exhaustive: n <= %d, endpoints in [0, %d]\n", EX_N_MAX, EX_T);
    printf("  random:     %d instances per family, n in [%d, %d]\n", RANDOM_PER_FAMILY,
           RANDOM_N_MIN, RANDOM_N_MAX);
    printf("  local:      n in [%d, %d], up to %d restarts of %d steps\n\n", LS_N_MIN,
           LS_N_MAX, LS_RESTARTS, LS_STEPS);

    int found_all = 1;
    for (int t = 0; t < NTARGET; t++) {
        Found best[NSTAGE] = {{0}};
        Stats st[NSTAGE] = {{0}};
        for (int s = 0; s < NSTAGE; s++) {
            double t0 = now();
            if (s == ST_EXHAUSTIVE) search_exhaustive(t, &best[s], &st[s]);
            else if (s == ST_RANDOM) search_random(t, master, &best[s], &st[s]);
            else search_local(t, master, &best[s], &st[s]);
            st[s].seconds = now() - t0;
        }

        /* the chosen one: fewest jobs, then biggest gap, then earliest search */
        int chosen = -1;
        for (int s = 0; s < NSTAGE; s++) {
            if (!best[s].have) continue;
            if (chosen < 0 || best[s].n < best[chosen].n ||
                (best[s].n == best[chosen].n &&
                 best[s].opt - best[s].h > best[chosen].opt - best[chosen].h))
                chosen = s;
        }

        printf("== %s ==\n", TARGETS[t].name);
        printf("  %-11s %12s %8s %10s %12s %9s\n", "search", "tried", "hits", "smallest n",
               "worst |H|/OPT", "seconds");
        for (int s = 0; s < NSTAGE; s++) {
            printf("  %-11s %12ld %8ld", STAGE_NAMES[s], st[s].tried, st[s].hits);
            if (best[s].have) printf(" %10d %12.4f", best[s].n, st[s].worst);
            else printf(" %10s %12s", "-", "-");
            printf(" %9.2f\n", st[s].seconds);

            fprintf(stats, "%s,%s,%ld,%ld,", TARGETS[t].col, STAGE_NAMES[s], st[s].tried,
                    st[s].hits);
            if (best[s].have) fprintf(stats, "%d,%.4f\n", best[s].n, st[s].worst);
            else fprintf(stats, ",\n");

            if (!best[s].have) continue;
            const Found *c = &best[s];
            fprintf(ce, "%s,%s,%d,%d,%d,%d,", TARGETS[t].col, STAGE_NAMES[s], s == chosen,
                    c->n, c->h, c->opt);
            csv_list(ce, c->jobs, NULL, c->n, 1);
            fputc(',', ce);
            csv_list(ce, c->jobs, c->h_sel, c->h, 0);
            fputc(',', ce);
            csv_list(ce, c->jobs, c->opt_sel, c->opt, 0);
            fputc('\n', ce);
        }
        printf("\n");

        if (chosen < 0) {
            printf("No counterexample found for %s.\n\n", TARGETS[t].name);
            fprintf(tl, "No counterexample found for %s.\n\n", TARGETS[t].name);
            found_all = 0;
            continue;
        }
        print_found(stdout, t, chosen, &best[chosen]);
        print_found(tl, t, chosen, &best[chosen]);
    }

    printf("%s\n", found_all ? "Found a counterexample for every heuristic."
                             : "Some heuristics have no counterexample in this search.");
    if (bad) printf("%d hit%s not confirmed by the exact solver (see stderr)\n", bad,
                    bad == 1 ? "" : "s");

    fclose(ce);
    fclose(stats);
    fclose(tl);
    return bad ? 1 : 0;
}
