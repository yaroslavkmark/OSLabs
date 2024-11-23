#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define MAX_THREADS 100

typedef struct {
    int **matrix;
    int **result;
    int rows;
    int cols;
    int window_size;
    int iterations;
    int thread_id;
    int max_threads;
} ThreadData;

int compare(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void *thread_function(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int start_row = data->thread_id * (data->rows / data->max_threads);
    int end_row = (data->thread_id + 1) * (data->rows / data->max_threads);

    for (int iter = 0; iter < data->iterations; iter++) {
        for (int i = start_row; i < end_row; i++) {
            for (int j = 0; j < data->cols; j++) {
                int count = 0;
                int half_window = data->window_size / 2;
                int *window = (int *)malloc(data->window_size * data->window_size * sizeof(int));

                for (int ki = -half_window; ki <= half_window; ki++) {
                    for (int kj = -half_window; kj <= half_window; kj++) {
                        int ni = i + ki;
                        int nj = j + kj;
                        if (ni >= 0 && ni < data->rows && nj >= 0 && nj < data->cols) {
                            window[count++] = data->matrix[ni][nj];
                        }
                    }
                }
                qsort(window, count, sizeof(int), compare);
                data->result[i][j] = window[count / 2];
                free(window);
            }
        }

        // Swap matrices for the next iteration
        int **temp = data->matrix;
        data->matrix = data->result;
        data->result = temp;
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <max_threads> <window_size> <iterations> <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    int max_threads = atoi(argv[1]);
    int window_size = atoi(argv[2]);
    int iterations = atoi(argv[3]);
    char *input_file = argv[4];
    char *output_file = argv[5];

    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        perror("Failed to open input file");
        return 1;
    }

    int rows, cols;
    fscanf(fp, "%d %d", &rows, &cols);

    int **matrix = (int **)malloc(rows * sizeof(int *));
    int **result = (int **)malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++) {
        matrix[i] = (int *)malloc(cols * sizeof(int));
        result[i] = (int *)malloc(cols * sizeof(int));
        for (int j = 0; j < cols; j++) {
            fscanf(fp, "%d", &matrix[i][j]);
        }
    }
    fclose(fp);

    pthread_t threads[MAX_THREADS];
    ThreadData thread_data[MAX_THREADS];

    for (int i = 0; i < max_threads; i++) {
        thread_data[i].matrix = matrix;
        thread_data[i].result = result;
        thread_data[i].rows = rows;
        thread_data[i].cols = cols;
        thread_data[i].window_size = window_size;
        thread_data[i].iterations = iterations;
        thread_data[i].thread_id = i;
        thread_data[i].max_threads = max_threads;

        pthread_create(&threads[i], NULL, thread_function, &thread_data[i]);
    }

    for (int i = 0; i < max_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    fp = fopen(output_file, "w");
    if (!fp) {
        perror("Failed to open output file");
        return 1;
    }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            fprintf(fp, "%d ", result[i][j]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);

    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
        free(result[i]);
    }
    free(matrix);
    free(result);

    return 0;
}