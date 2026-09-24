#include <mpi.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// Signal y[i] = sin(2*pi*3*i/n) + 0.5*sin(2*pi*7*i/n).
// Project onto the 3-cycle basis. This is a sine projection, not an FFT.
static double sample(int i, int n) {
    constexpr double pi = 3.14159265358979323846;
    return std::sin(2 * pi * 3 * i / n) + 0.5 * std::sin(2 * pi * 7 * i / n);
}
static double term(int i, int n, double value) {
    constexpr double pi = 3.14159265358979323846;
    return value * std::sin(2 * pi * 3 * i / n);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank = 0, ranks = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);
    int n = 0, valid = 1;
    if (rank == 0) {
        try {
            if (argc != 2 || argv[1][0] == '-' || argv[1][0] == '+') throw std::invalid_argument("usage: mpi_signal <positive-samples>");
            size_t end = 0;
            const unsigned long parsed = std::stoul(argv[1], &end);
            if (argv[1][end] || parsed == 0 || parsed > static_cast<unsigned long>(std::numeric_limits<int>::max()))
                throw std::invalid_argument("invalid sample count");
            n = static_cast<int>(parsed);
        } catch (const std::exception& e) { std::cerr << e.what() << '\n'; valid = 0; }
    }
    MPI_Bcast(&valid, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!valid) { MPI_Finalize(); return 2; }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::vector<int> counts(ranks), offsets(ranks);
    for (int r = 0, offset = 0; r < ranks; ++r) {
        counts[r] = n / ranks + (r < n % ranks);
        offsets[r] = offset;
        offset += counts[r];
    }
    std::vector<double> all;
    if (rank == 0) {
        all.resize(n);
        for (int i = 0; i < n; ++i) all[i] = sample(i, n);
    }
    std::vector<double> local(counts[rank]);
    MPI_Barrier(MPI_COMM_WORLD);
    const double start = MPI_Wtime();
    MPI_Scatterv(rank == 0 ? all.data() : nullptr, counts.data(), offsets.data(), MPI_DOUBLE,
                 local.data(), counts[rank], MPI_DOUBLE, 0, MPI_COMM_WORLD);
    double partial = 0;
    for (int j = 0; j < counts[rank]; ++j) partial += term(offsets[rank] + j, n, local[j]);
    double total = 0;
    MPI_Reduce(&partial, &total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    const double seconds = MPI_Wtime() - start;
    int correct = 1;
    if (rank == 0) {
        double reference = 0;
        for (int i = 0; i < n; ++i) reference += term(i, n, all[i]);
        const double tolerance = 1e-10 * std::max(1.0, std::abs(reference));
        correct = std::abs(total - reference) <= tolerance;
        std::cout << "samples,ranks,projection,serial_reference,seconds,correct\n"
                  << n << ',' << ranks << ',' << total << ',' << reference << ',' << seconds << ',' << correct << '\n';
    }
    MPI_Bcast(&correct, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Finalize();
    return correct ? 0 : 1;
}
