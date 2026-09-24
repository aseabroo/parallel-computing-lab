# Parallel Computing Lab

Small, reproducible CPU and optional MPI experiments, independently implemented from workload descriptions. This repository is **not** a dump of CS475 submissions. Earlier coursework motivated the workloads; no starter code, historical benchmark results or third-party datasets are included.

## Build and verify

On a system with CMake 3.16+, a C++17 compiler and optionally OpenMP/MPI:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/cpu_lab 1000000 42 4 5
```

If OpenMP is unavailable, use one thread. MPI is optional; if found, run `mpiexec -n 4 ./build/mpi_signal 10007`. The MPI tests also cover counts that do not divide evenly and more ranks than samples.

`cpu_lab` estimates π from a stateless seeded two-dimensional sample. Every thread count uses the same samples; each run is checked against a serial hit count before a CSV row is emitted. `mpi_signal` creates a synthetic signal on rank zero, distributes all samples with `MPI_Scatterv`, projects onto a known sine component, reduces partial sums and compares with a serial double-precision reference. Its measured MPI interval includes barrier, scatter, projection and reduction, but excludes signal generation and output. The CPU measured interval covers counting only; it excludes serial validation, process startup and CSV output.

Keep all raw repeats and record CPU, compiler, flags, OpenMP/MPI versions, host load and run time with any published results. The commands above are examples, not measured speedups. Tiny sizes are correctness fixtures; meaningful performance comparisons need larger workloads and repeated runs on named hardware. The Monte Carlo estimate has sampling error; exact reproducibility of hit counts is a correctness check, not evidence that the estimate equals π.

## Scope

The existing course archive contains other experiments whose source or provenance is incomplete. CUDA, OpenCL, ecosystem simulation, K-means and SIMD are not implemented or claimed here. This lab has no connection to AiQuant trading performance.
