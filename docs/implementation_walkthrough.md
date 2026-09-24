# Explaining the current lab

This is a reading and verification guide for the current code. The original CS475 assignments are historical coursework; the current C++17 CPU/MPI implementation and this guide were drafted with AI assistance under the maintainer's direction. A successful CI run demonstrates the behavior of the checked-in code. It does not establish who personally typed or can explain every line.

## CPU workload

`src/cpu_lab.cpp` maps the pair `(seed, sample index)` to two deterministic coordinates using a 64-bit mixing function. It counts a hit when `x² + y² <= 1`. Because this is a quarter of the unit circle in the unit square, the estimate is `4 × hits / trials`. The mixing function is a practical deterministic input generator for this experiment; this lab has not established rigorous statistical independence of the samples.

`count_hits` visits each index once. With OpenMP, the `parallel for reduction(+:hits)` directive gives workers partial counters and combines them. The serial invocation recomputes the *same* indices and seed before the timed repeats; if any parallel count differs, the program exits without reporting that row. `--self-test` also checks small sizes, seeds and thread counts. Identical hits establish execution consistency, not a statistically exact value of π.

Things to explain while reading: why `n=1` is a useful boundary test; why seeding once per worker could change the work between thread counts; why a shared atomic counter can hurt scaling; why the serial validation is outside the timed interval; and why small measurements are noisy.

## MPI workload

`src/mpi_signal.cpp` synthesizes two sine components on rank zero. For `n` samples and `p` ranks, each rank receives `n/p` elements plus one extra for the first `n%p` ranks. The displacements are cumulative counts, so all `n` samples are covered, including `p>n`. Rank zero broadcasts input validity before scatter so no rank waits forever after a bad argument. Each rank sums its sine projection; `MPI_Reduce` combines sums; rank zero compares against a serial double-precision reference with an explicit tolerance. The result is a sine projection, **not an FFT**. The timed MPI interval includes barrier, scatter, local projection and reduction; it excludes signal creation and output.

## Measured exploratory run, 2026-09-24

On a shared Linux x86-64 virtual machine reporting an Intel Xeon Platinum 8573C, nine logical CPUs visible and an eight-CPU cgroup quota, the CPU program was compiled with `g++ -std=c++17 -O3 -fopenmp`. With 100,000,000 trials, seed 42, and seven rotated rounds, all 21 hit counts equaled **78,539,082**. Seconds for the timed counting interval were:

| Threads | Median | Minimum | Maximum |
| --- | ---: | ---: | ---: |
| 1 | 0.44284 | 0.34172 | 0.97519 |
| 2 | 0.21200 | 0.18132 | 2.28861 |
| 4 | 0.25535 | 0.17883 | 0.69575 |

The two-thread outlier and spread across all counts show that host interference can dominate this short workload. These measurements describe one virtual machine session and **are not a stable speedup result**. The recorded source revision was `08c781452cd22c1581fa8e7b49bd9b0fb299c5ab`; the script was under development (`source_dirty=true`). Benchmark CSV and manifest remain local scratch outputs, not a committed performance claim. To publish a comparison, rerun on specified hardware with the final clean revision, preserve every raw repeat, and report dispersion and environment alongside any ratios.

## A concrete next maintainer exercise

On your own Mac, run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`. If the configure output says OpenMP was found, run `python3 scripts/benchmark.py --binary build/cpu_lab --trials 100000000 --repeats 7 --threads 1 2 4 --output results/mac-first.csv`; otherwise use `--threads 1` and document which toolchain you used. Record your Mac model, compiler identity and flags, whether other work was running, and what surprised you in the raw rows. Explain an unfavorable measurement instead of discarding it. Then make and explain one change that you choose yourself; that contribution will be stronger evidence of your independent implementation ability than this AI-assisted cleanup alone.
