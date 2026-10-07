/* Seeded random instance generators, used by the experiments.
 *
 * Families (n = number of jobs):
 *   sparse     start in [0, 10n), length in [1, 5]       few conflicts
 *   dense      start in [0, n),   length in [n/2, 2n]    heavy overlap
 *   varied     start in [0, 4n),  length either short [1, 3] or long [5, 3n]
 *   clustered  2-4 centres, start within 3 of a centre, length in [1, 8]
 *   ties       start in [0, 6],   length in [1, 3]       many equal and
 *              touching endpoints, to exercise the half-open rule and ties */

#include "gen.h"

static int max_int(int a, int b) { return a > b ? a : b; }

Rng rng_seed(uint64_t seed) {
    Rng r = {seed};
    return r;
}

uint64_t rng_next(Rng *r) {
    uint64_t z = (r->state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* The modulo is very slightly biased, but our ranges are tiny next to
 * 2^64 so the bias is far too small to matter. */
int rng_range(Rng *r, int lo, int hi) {
    return lo + (int)(rng_next(r) % (uint64_t)(hi - lo + 1));
}

/* rng_next is a one-to-one mix of the state, so different
 * (master, family, index) triples give different seeds. */
uint64_t instance_seed(uint64_t master, int family, int index) {
    Rng r = rng_seed(master ^ ((uint64_t)family << 32) ^ (uint64_t)index);
    return rng_next(&r);
}

const char *family_name(Family fam) {
    static const char *names[FAM_COUNT] = {
        "sparse", "dense", "varied", "clustered", "ties",
    };
    return fam < FAM_COUNT ? names[fam] : "?";
}

void gen_instance(Family fam, int n, Rng *r, Job *out) {
    int centres[4], ncentres = 0;
    if (fam == FAM_CLUSTERED) {
        ncentres = rng_range(r, 2, 4);
        for (int c = 0; c < ncentres; c++)
            centres[c] = rng_range(r, 3, 6 * n + 3);
    }

    for (int i = 0; i < n; i++) {
        int s = 0, len = 1;
        switch (fam) {
        case FAM_SPARSE:
            s = rng_range(r, 0, 10 * n - 1);
            len = rng_range(r, 1, 5);
            break;
        case FAM_DENSE:
            s = rng_range(r, 0, n - 1);
            len = rng_range(r, max_int(1, n / 2), 2 * n);
            break;
        case FAM_VARIED:
            s = rng_range(r, 0, 4 * n - 1);
            len = rng_range(r, 0, 1) ? rng_range(r, 1, 3)
                                     : rng_range(r, 5, max_int(5, 3 * n));
            break;
        case FAM_CLUSTERED:
            s = centres[rng_range(r, 0, ncentres - 1)] + rng_range(r, -3, 3);
            len = rng_range(r, 1, 8);
            break;
        case FAM_TIES:
        default:
            s = rng_range(r, 0, 6);
            len = rng_range(r, 1, 3);
            break;
        }
        out[i] = (Job){i, s, s + len};
    }
}
