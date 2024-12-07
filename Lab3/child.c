#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

#define BUFFER_SIZE 1024
#define SHM_NAME "/my_shared_memory"
#define SEM_NAME_PARENT "/my_semaphore_parent"
#define SEM_NAME_CHILD "/my_semaphore_child"

int main(int argc, char *argv[]) {
    // Открытие разделяемой памяти
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        write(STDOUT_FILENO, "Не удалось открыть разделяемую память", sizeof("Не удалось открыть разделяемую память"));
        exit(EXIT_FAILURE);
    }

    // Отображение разделяемой памяти
    char *shared_memory = (char *)mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        write(STDOUT_FILENO, "Не удалось отобразить разделяемую память", sizeof("Не удалось отобразить разделяемую память"));
        exit(EXIT_FAILURE);
    }

    // Открытие семафоров
    sem_t *sem_parent = sem_open(SEM_NAME_PARENT, 0);
    sem_t *sem_child = sem_open(SEM_NAME_CHILD, 0);
    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        write(STDOUT_FILENO, "Не удалось открыть семафоры", sizeof("Не удалось открыть семафоры"));
        exit(EXIT_FAILURE);
    }

    int file_descriptor = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (file_descriptor == -1) {
        write(STDOUT_FILENO, "Не удалось открыть файл", sizeof("Не удалось открыть файл"));
        exit(EXIT_FAILURE);
    }

    char input[BUFFER_SIZE];
    char error_msg[BUFFER_SIZE];

    while (1) {
        // Ожидание сигнала от родительского процесса
        sem_wait(sem_child);

        // Чтение из разделяемой памяти
        strncpy(input, shared_memory, BUFFER_SIZE);

        if (strlen(input) == 0) {
            break; // Конец ввода
        }

        if (isupper(input[0])) {
            char tmp[1100];
            int ret = snprintf(tmp, sizeof(tmp), "Валидная строка: %s\n", input);
            if (ret < 0) {
                abort();
            }
            write(STDOUT_FILENO, tmp, strlen(tmp));
            write(file_descriptor, tmp, strlen(tmp));
        } else {
            if (strlen(input)) {
                int ret = snprintf(error_msg, sizeof(error_msg), "Не валидная строка: %s\n", input);
                if (ret < 0) {
                    abort();
                }
                write(STDOUT_FILENO, error_msg, strlen(error_msg));
                write(file_descriptor, error_msg, strlen(error_msg));
            }
        }

        // Сигнализируем родительскому процессу
        sem_post(sem_parent);
    }

    // Освобождение ресурсов
    munmap(shared_memory, BUFFER_SIZE);
    close(shm_fd);
    sem_close(sem_parent);
    sem_close(sem_child);
    close(file_descriptor);
    return 0;
}