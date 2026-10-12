# Interval Scheduling: Greedy Algorithms Under Pressure

CMP_SC 4050 Group Algorithm Engineering Assignment 1
Liam Jolles, Anna Markway, Drew Stockton

Four greedy heuristics for interval scheduling (Earliest Finish, Earliest
Start, Shortest Duration, Fewest Conflicts), a brute-force exact solver, an
automatic counterexample search, and the experiments from the report.

## Language and version

- **C11**, built with `cc -std=c11 -O2 -Wall -Wextra -pedantic`. Tested with
  Apple clang 17.0.0 on macOS (Apple M3). Any C11 compiler (gcc or clang)
  should work. No external libraries.
- **Python 3** (tested with 3.9.6) with pandas, numpy, matplotlib and Jupyter,
  only for drawing the figures. Not needed to run the code or experiments.

## Building

```sh
make          # builds schedule, tests, validate and search
make clean    # removes the binaries
```

## Running on your own instance

`schedule` runs all four heuristics and the exact solver on one instance and
prints the jobs each one selected.

```sh
./schedule input.txt      # read from a file
./schedule < input.txt    # or from standard input
```

### Input format

The first line is the number of jobs n. Each of the next n lines holds one job's
start and finish time as two integers `s f`, with `s < f`.

```
5
0 10
1 2
2 3
3 4
4 5
```

Jobs are half-open intervals [s, f), so a job finishing at t is compatible with
one starting at t. Jobs are numbered from 0 in input order. The exact solver is
skipped for n > 25 (it checks all 2ⁿ subsets).

Output for the example above (Sanity Test 1):

```
A Earliest Finish    |H| = 4 : 1[1,2) 2[2,3) 3[3,4) 4[4,5)
B Earliest Start     |H| = 1 : 0[0,10)
C Shortest Duration  |H| = 4 : 1[1,2) 2[2,3) 3[3,4) 4[4,5)
D Fewest Conflicts   |H| = 4 : 1[1,2) 2[2,3) 3[3,4) 4[4,5)
Exact                OPT = 4 : 1[1,2) 2[2,3) 3[3,4) 4[4,5)
```

Each selected job is printed as `id[s,f)`.

## Reproducing the experiments

Every experiment is seeded (default seed 4050) and uses its own random number
generator, so the results are the same on any machine. Running times in
Part 6 depend on the hardware.

| Command | Report part | Time | Writes to `results/` |
|---|---|---|---|
| `make test` | Parts 1–2, sanity tests | < 1 s | (prints PASS/FAIL) |
| `make part3` | Part 3, counterexample search | ~10 s | `part3_counterexamples.csv`, `part3_search.csv`, `part3_timelines.txt` |
| `make part4` (same as `make part5`) | Parts 4 and 5 | < 1 s | `instances.csv`, `jobs.csv`, `part4_summary.csv`, `part5_summary.csv`, `part5_worst.txt` |
| `make part6` | Part 6, runtime | ~6 min | `part6_timing.csv` |

Then, to redraw every figure in `figures/` (PDF and PNG):

```sh
jupyter nbconvert --to notebook --execute --inplace figures.ipynb
```

### Options

```sh
./search [seed] [outdir]                                   # Part 3
./validate [seed] [per_family] [outdir]                    # Parts 4 and 5
./validate time [seed] [reps] [max_n] [families] [outdir]  # Part 6
```

- Part 3 defaults: seed 4050, output in `results`.
- Parts 4 and 5 defaults: seed 4050, 200 instances per family, output in
  `results`. Each instance has 4–18 jobs.
- Part 6 defaults: seed 4050, 5 runs, n = 100 up to 100,000, families
  `sparse,dense`, output in `results`. `families` is a comma-separated list
  or `all`.
- The output directory must already exist (the `make` targets create
  `results/`).

### Instance families

Generated in `gen.c` (n = number of jobs):

| Family | Start times | Lengths | Used in |
|---|---|---|---|
| sparse | uniform in [0, 10n) | 1–5 | Parts 3–6 |
| dense | uniform in [0, n) | n/2 to 2n | Parts 3–6 |
| varied | uniform in [0, 4n) | short (1–3) or long (5 to 3n), 50/50 | Parts 3–5 |
| clustered | within 3 of one of 2–4 random centres | 1–8 | Parts 3–5 |
| ties | uniform in [0, 6] | 1–3 (many equal and touching endpoints) | Part 3 only |

## Files

| File | Contents |
|---|---|
| `interval.h` | `Job` type, heuristic and exact solver declarations |
| `greedy.c` | The four greedy heuristics (Part 1) |
| `exact.c` | Brute-force exact solver (Part 2) |
| `main.c` | `schedule`: run everything on one input instance |
| `tests.c` | Sanity tests and unit tests |
| `search.c` | Counterexample search (Part 3) |
| `gen.c`, `gen.h` | Seeded random number generator and instance families |
| `validate.c` | Parts 4 and 5 (quality vs. exact solver) and Part 6 (timing) |
| `figures.ipynb` | Draws the report figures from the CSVs in `results/` |
| `results/` | Machine-readable results of every experiment |
| `figures/` | Figures used in the report |
