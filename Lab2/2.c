#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>


typedef struct {
    int **matrix;
    int window_size;
    int matrix_size;
    int line_start;
    int line_end;
    int column_start;
    int column_end;
    int K;
    int id;
} ThreadData;

pthread_mutex_t output_mutex;

int compare(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void write_string(int fd, const char *str) {
    write(fd, str, strlen(str));
}

bool is_number(const char *str) {
    if (*str == '\0') return false;
    if (*str == '-') return false;

    while (*str) {
        if (*str == '.' || !isdigit(*str))
            return false;

        str++;
    }

    return true;
}

void apply_median_filter(int **input, int **output, ThreadData* data){
    int line = data->line_start;
    int col = data->column_start;
    for (int l = line; l < data->line_end; ++l){
        for (int i = col; i < data->column_end; ++i){
            //++q;
            int tmp[data->window_size * data->window_size];
            int index = 0;

            for (int fx = 0; fx < 2; ++fx){
                for (int fy = 0; fy < 2; ++fy){
                    if (fx == 0 && fy == 0){
                        tmp[index++] = input[l + fx][i + fy];

                    } else if (fx == 1 && fy == 1) {
                        tmp[index++] = input[l + fx][i + fy];
                        tmp[index++] = input[l - fx][i - fy];
                        tmp[index++] = input[l - fx][i + fy];
                        tmp[index++] = input[l + fx][i - fy];
                    } else {
                        tmp[index++] = input[l + fx][i + fy];
                        tmp[index++] = input[l - fx][i - fy];
                    }
                }
            }
            qsort(tmp, index, sizeof(int), compare);

            output[l][i] = tmp[index / 2];
        }
    }

}

void *threadFunction(void *arg){
    ThreadData *data = (ThreadData*)arg;

    int size = data->matrix_size;
    int **localOutput = (int **)calloc(size, sizeof(int *));
    for (int i = 0; i <size; i++) {
        localOutput[i] = (int *)calloc(size, sizeof(int));
    }

    apply_median_filter(data->matrix, localOutput, data);
    pthread_mutex_lock(&output_mutex);

    data->matrix = localOutput;

    pthread_mutex_unlock(&output_mutex);


    return NULL;
}

void printMatrix(int **matrix, int width, int height) {
    printf("Матрица:\n");
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            printf("%3d ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

int64_t millis(){
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return ((int64_t) now.tv_sec) * 1000 + ((int64_t) now.tv_nsec) / 1000000;
}

int main(int argc, char *argv[]){
    if (argc != 5){
        write_string(STDOUT_FILENO, "Usage: ./program <window_size> <matrix_size> <K> <num_threads>");
        return 1;
    }

    if (!is_number(argv[1]) || !is_number(argv[2]) || !is_number(argv[3])){
        write_string(STDOUT_FILENO, "Wrong input");
        return 2;
    }

    int window_size = atoi(argv[1]);
    int matrix_size = atoi(argv[2]);
    int K = atoi(argv[3]);
    int num_threads = atoi(argv[4]);

    if (num_threads != window_size){
        num_threads = 1;
    }

    if (window_size % 2 == 0 || matrix_size % 2 == 0){
        write_string(STDOUT_FILENO, "Must be odd");
        return 1;
    }

    if (matrix_size <= 1){
        write_string(STDOUT_FILENO, "Must be >= 2");
        return 2;
    }

    if (window_size <= 1 || window_size > matrix_size){
        write_string(STDOUT_FILENO, "Must be 3 <= window size <= matrix size");
        return 3;
    }


    srand(time(NULL));
    int **matrix = (int **)malloc(matrix_size * sizeof(int *));
    for (int i = 0; i < matrix_size; i++) {
        matrix[i] = (int *)malloc(matrix_size * sizeof(int));
        for (int j = 0; j < matrix_size; j++) {
            matrix[i][j] = i * 5 + (j + 1);
        }
    }

    printMatrix(matrix, matrix_size, matrix_size);
    if (window_size == matrix_size){
        int idx = 0;
        int data[matrix_size * matrix_size];
        for (int i = 0; i < matrix_size; ++i){
            for (int j = 0; j < matrix_size; ++j){
                data[idx++] = matrix[i][j];
            }
        }
        qsort(data, idx, sizeof(int), compare);
        int output = data[(matrix_size * matrix_size) / 2];
        for (int i = 0; i < matrix_size; ++i){
            for (int j = 0; j < matrix_size; ++j){
                if (i == matrix_size / 2 && j == matrix_size / 2){
                    printf("%d  ", output);
                } else{
                    printf("0  ");
                }
            }
            printf("\n");
        }
        return 0;
    }

    pthread_mutex_init(&output_mutex, NULL);

    pthread_t threads[num_threads];
    ThreadData threadData[num_threads];

    for (int i = 0; i < num_threads; ++i){
        threadData[i].matrix = matrix;
        threadData[i].window_size = window_size;
        threadData[i].matrix_size = matrix_size;
        threadData[i].K = K;
        threadData[i].column_start = (matrix_size - window_size) / 2; // 1
        threadData[i].column_end = matrix_size - ((matrix_size - window_size) / 2); // 8
        threadData[i].line_start = (window_size / num_threads == 1) ? ((matrix_size - window_size) / 2) + i : (matrix_size - window_size) / 2; // 3
        threadData[i].line_end = (window_size / num_threads == 1) ? ((matrix_size - window_size) / 2) + i + 1 : matrix_size - ((matrix_size - window_size) / 2);
        threadData[i].id = i + 1;
    }


//    long long res = 0;
    for (int k = 0; k < K; ++k){
        //long long q = millis();
        for (int i = 0; i < num_threads; ++i){
            if (pthread_create(&threads[i], NULL, threadFunction, &threadData[i])){
                write_string(STDERR_FILENO, "Error thread creation");
                return 4;
            }
        }
        // Ожидание завершения всех потоков
        for (int i = 0; i < num_threads; i++) {
            if (pthread_join(threads[i], NULL) != 0) {
                fprintf(stderr, "Error waiting thread\n");
                return 1;
            }
        }
//        long long q1 = millis();
//        res += (q1 - q);

        int **matrix1 = (int **)calloc(matrix_size,  sizeof(int *));
        for (int i = 0; i < matrix_size; i++) {
            matrix1[i] = (int *)calloc(matrix_size, sizeof(int));
        }
        for (int i = 0; i < num_threads; ++i){
            for (int l = threadData[i].line_start; l <= threadData[i].column_end; ++l){
                for (int j = threadData[i].column_start; j < threadData[i].column_end; ++j){
                    matrix1[l][j] = threadData[i].matrix[l][j];
                }
            }
        }

        for (int i = 0; i < num_threads; ++i){
            for (int j = 0; j < matrix_size; ++j){
                free(threadData[i].matrix[j]);
            }
            free(threadData[i].matrix);
            threadData[i].matrix = matrix1;
        }
    }

//
//    printf("TiMe: %lld\n", res);


    printMatrix(threadData[num_threads - 1].matrix, matrix_size, matrix_size);

    for (int i = 0; i < matrix_size; i++) {
        free(matrix[i]);
    }
    free(matrix);
    pthread_mutex_destroy(&output_mutex);

    return 0;
}