#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define TASK 1
#define DATA 2
#define RESULT 3
#define STOP 4

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0)
            printf("Потрібно мінімум 2 процеси!\n");

        MPI_Finalize();
        return 1;
    }

    long long n = 100000000LL;

    if (rank == 0 && argc > 1)
        n = atoll(argv[1]);

    MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    int workers = size - 1;
    long long max_chunk = (n / 2 + workers - 1) / workers;
    if (max_chunk < 1)
        max_chunk = 1;

    if (rank != 0) {
        int *first = malloc(max_chunk * sizeof(int));
        int *second = malloc(max_chunk * sizeof(int));
        int *result = malloc(max_chunk * sizeof(int));

        long long hdr[3];

        while (1) {
            MPI_Status status;

            MPI_Recv(hdr, 3, MPI_LONG_LONG, 0,
                     MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == STOP)
                break;

            long long i0 = hdr[0];
            long long count = hdr[1];
            long long L = hdr[2];

            MPI_Recv(first, (int)count, MPI_INT, 0, DATA,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            MPI_Recv(second, (int)count, MPI_INT, 0, DATA,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            long long second_start = L - i0 - count;

            for (long long k = 0; k < count; k++) {
                long long i = i0 + k;
                long long j = L - 1 - i;
                result[k] = first[k] + second[j - second_start];
            }

            MPI_Send(result, (int)count, MPI_INT, 0, RESULT,
                     MPI_COMM_WORLD);
        }

        free(first);
        free(second);
        free(result);
    }
    else {
        int *a = malloc(n * sizeof(int));

        if (!a) {
            printf("Помилка виділення пам'яті!\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (long long i = 0; i < n; i++)
            a[i] = 1;

        printf("\nArray size: %lld\n", n);
        printf("Processes: %d\n\n", size);

        double start = MPI_Wtime();

        for (long long L = n; L > 1; L = (L + 1) / 2) {

            long long pairs = L / 2;
            long long per = (pairs + workers - 1) / workers;
            long long next = 0;

            long long *positions = calloc(size, sizeof(long long));
            long long *sent = calloc(size, sizeof(long long));

            for (int w = 1; w < size && next < pairs; w++) {

                long long count = (pairs - next < per) ? pairs - next : per;

                long long hdr[3] = { next, count, L };

                MPI_Send(hdr, 3, MPI_LONG_LONG, w, TASK,
                         MPI_COMM_WORLD);

                MPI_Send(a + next, (int)count, MPI_INT, w, DATA,
                         MPI_COMM_WORLD);

                MPI_Send(a + L - next - count, (int)count, MPI_INT, w, DATA,
                         MPI_COMM_WORLD);

                positions[w] = next;
                sent[w] = count;
                next += count;
            }

            for (int w = 1; w < size; w++) {

                if (sent[w] == 0)
                    continue;

                MPI_Recv(a + positions[w], (int)sent[w], MPI_INT, w,
                         RESULT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            free(positions);
            free(sent);
        }

        double elapsed = (MPI_Wtime() - start) * 1000.0;

        long long hdr[3] = {0, 0, 0};

        for (int w = 1; w < size; w++)
            MPI_Send(hdr, 3, MPI_LONG_LONG, w, STOP,
                     MPI_COMM_WORLD);

        printf("MPI:\n");
        printf("Sum = %d\n", a[0]);
        printf("Time = %.2f ms\n", elapsed);
        printf("Check = %s\n", a[0] == n ? "OK" : "ERROR");

        free(a);
    }

    MPI_Finalize();
    return 0;
}