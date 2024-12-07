#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <stdio.h>

#define BUFFER_SIZE 1024
#define SHM_NAME "/my_shared_memory"
#define SEM_NAME_PARENT "/my_semaphore_parent"
#define SEM_NAME_CHILD "/my_semaphore_child"
static char CLIENT_PROGRAM_NAME[] = "child";

void write_string(int fd, const char *str) {
    write(fd, str, strlen(str));
}

void read_string(int fd, char *buffer, size_t size) {
    read(fd, buffer, size);
}

int main() {
    char filename[BUFFER_SIZE];
    char progpath[1024];

    ssize_t len = readlink("/proc/self/exe", progpath, sizeof(progpath) - 1);
    if (len == -1) {
        const char msg[] = "error: failed to read full program path\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    while (progpath[len] != '/')
        --len;

    progpath[len] = '\0';

    const char *prompt = "Введите имя файла: ";
    write_string(STDOUT_FILENO, prompt);
    read_string(STDIN_FILENO, filename, sizeof(filename));
    filename[strcspn(filename, "\n")] = '\0'; // Удаляем символ новой строки

//    int file_descriptor = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
//    if (file_descriptor == -1) {
//        write_string(STDOUT_FILENO, "Не удалось открыть файл");
//        exit(EXIT_FAILURE);
//    }

    // Создание разделяемой памяти
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        write_string(STDOUT_FILENO, "Не удалось создать разделяемую память");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, BUFFER_SIZE);

    // Отображение разделяемой памяти
    char *shared_memory = (char *)mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        write_string(STDOUT_FILENO, "Не удалось отобразить разделяемую память");
        exit(EXIT_FAILURE);
    }

    // Создание семафоров
    sem_t *sem_parent = sem_open(SEM_NAME_PARENT, O_CREAT, 0666, 0);
    sem_t *sem_child = sem_open(SEM_NAME_CHILD, O_CREAT, 0666, 0);
    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        write_string(STDOUT_FILENO, "Не удалось создать семафоры");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        write_string(STDOUT_FILENO, "Не удалось создать процесс");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        char path[2024];
        snprintf(path, sizeof(path) - 1, "%s/%s", progpath, CLIENT_PROGRAM_NAME);

        char *const args[] = {CLIENT_PROGRAM_NAME, filename, NULL};

        int32_t status = execv(path, args);

        if (status == -1) {
            const char msg[] = "error: failed to exec into new executable image\n";
            write(STDERR_FILENO, msg, sizeof(msg));
            exit(EXIT_FAILURE);
        }
//        // Дочерний процесс
//        char *args[] = {"./child", NULL};
//        execvp(args[0], args);
//        write_string(STDOUT_FILENO, "Не удалось запустить дочерний процесс");
//        exit(EXIT_FAILURE);
    } else {
        // Родительский процесс
        char input[BUFFER_SIZE];
        const char *input_prompt = "Введите строки (CTRL+D для завершения):\n";
        write_string(STDOUT_FILENO, input_prompt);

        while (read(STDIN_FILENO, input, sizeof(input)) > 0) {
            input[strcspn(input, "\n")] = '\0'; // Удаляем символ новой строки

            // Запись в разделяемую память
            strncpy(shared_memory, input, BUFFER_SIZE);

            // Сигнализируем дочернему процессу
            sem_post(sem_child);

            // Ожидаем подтверждения от дочернего процесса
            sem_wait(sem_parent);
        }

        // Сигнализируем о завершении ввода
        strncpy(shared_memory, "", BUFFER_SIZE);
        sem_post(sem_child);

        // Ожидание завершения дочернего процесса
        wait(NULL);

        // Освобождение ресурсов
        munmap(shared_memory, BUFFER_SIZE);
        shm_unlink(SHM_NAME);
        sem_close(sem_parent);
        sem_close(sem_child);
        sem_unlink(SEM_NAME_PARENT);
        sem_unlink(SEM_NAME_CHILD);
        //close(file_descriptor);
    }

    return 0;
}