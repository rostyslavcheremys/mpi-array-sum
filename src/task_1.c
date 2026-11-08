#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    long long n = 100000000LL;

    if (rank == 0 && argc > 1)
        n = atoll(argv[1]);

    MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    if (n < 1 || n > 2147483647LL) {
        if (rank == 0)
            printf("Некоректний розмір масиву!\n");

        MPI_Finalize();
        return 1;
    }

    int *array = NULL;

    if (rank == 0) {
        array = malloc((size_t)n * sizeof(int));

        if (!array) {
            printf("Помилка виділення пам'яті!\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (long long i = 0; i < n; i++)
            array[i] = 1;
    }

    int *counts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));

    int base = (int)(n / size);
    int rem = (int)(n % size);

    int offset = 0;

    for (int i = 0; i < size; i++) {
        counts[i] = base + (i < rem ? 1 : 0);
        displs[i] = offset;
        offset += counts[i];
    }

    int *local = malloc((size_t)counts[rank] * sizeof(int));

    if (!local) {
        printf("Помилка виділення локальної пам'яті!\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    long long seq_sum = 0;
    double seq_time = 0;

    if (rank == 0) {
        double start = MPI_Wtime();

        for (long long i = 0; i < n; i++)
            seq_sum += array[i];

        seq_time = (MPI_Wtime() - start) * 1000.0;

        printf("\nArray size: %lld\n", n);
        printf("Processes: %d\n\n", size);

        printf("Sequential:\n");
        printf("Sum = %lld\n", seq_sum);
        printf("Time = %.2f ms\n\n", seq_time);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double start = MPI_Wtime();

    MPI_Scatterv(array, counts, displs, MPI_INT,
                 local, counts[rank], MPI_INT,
                 0, MPI_COMM_WORLD);

    long long local_sum = 0;

    for (int i = 0; i < counts[rank]; i++)
        local_sum += local[i];

    long long total = 0;

    MPI_Reduce(&local_sum, &total, 1, MPI_LONG_LONG,
               MPI_SUM, 0, MPI_COMM_WORLD);

    double mpi_time = (MPI_Wtime() - start) * 1000.0;

    if (rank == 0) {
        printf("MPI:\n");
        printf("Sum = %lld\n", total);
        printf("Time = %.2f ms\n", mpi_time);
        printf("Check = %s\n", total == seq_sum ? "OK" : "ERROR");

        free(array);
    }

    free(local);
    free(counts);
    free(displs);

    MPI_Finalize();

    return 0;
}