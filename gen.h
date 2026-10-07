#ifndef GEN_H
#define GEN_H

#include <stdint.h>
#include "interval.h"

/* splitmix64 random number generator. We use our own instead of rand()
 * because rand() gives different numbers on macOS and Linux, and the
 * experiments have to be reproducible everywhere. */
typedef struct {
    uint64_t state;
} Rng;

Rng rng_seed(uint64_t seed);
uint64_t rng_next(Rng *r);
int rng_range(Rng *r, int lo, int hi); /* uniform in [lo, hi] */

/* Seed for one instance, worked out from the master seed, the family and
 * the instance's index. Any single instance can be rebuilt from it. */
uint64_t instance_seed(uint64_t master, int family, int index);

/* Kinds of random instances (see gen.c for what each one looks like). */
typedef enum {
    FAM_SPARSE,
    FAM_DENSE,
    FAM_VARIED,
    FAM_CLUSTERED,
    FAM_TIES,
    FAM_COUNT
} Family;

const char *family_name(Family fam);

/* Fill out[0..n) with n random jobs of the given family.
 * Job i gets id i, and every job has s < f. */
void gen_instance(Family fam, int n, Rng *r, Job *out);

#endif
