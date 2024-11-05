#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main() {
    int pipe1[2], pipe2[2];

    if (pipe(pipe1) == -1) {
        perror("pipe1");
        exit(1);
    }

    if (pipe(pipe2) == -1) {
        perror("pipe2");
        exit(1);
    }

    // First child (sort)
    if (fork() == 0) {
        // Redirection of output to pipe1
        dup2(pipe1[1], STDOUT_FILENO);
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        int input = open("names.txt", O_RDONLY);
        if (input == -1) {
            perror("open");
            exit(1);
        }
        

        dup2(input, STDIN_FILENO);
        close(input);

        // Executing sort command
        execlp("sort", "sort", (char *)NULL);
        perror("execlp sort");
        exit(1);
    }

    // Second child (a -> A)
    if (fork() == 0) {
        // Redirecting input to pipe1 and output to pipe2
        dup2(pipe1[0], STDIN_FILENO);
        dup2(pipe2[1], STDOUT_FILENO);
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        // Executing "tr" command
        execlp("tr", "tr", "[a-z]", "[A-Z]", (char *)NULL);
        perror("execlp tr");
        exit(1);
    }

    // Third child (line numbers)
    if (fork() == 0) {
        // Redirecting input to pipe2
        dup2(pipe2[0], STDIN_FILENO);
        close(pipe2[0]);
        close(pipe2[1]);
        close(pipe1[0]);
        close(pipe1[1]);

        // Executing "nl" command
        execlp("nl", "nl", "-s", ". ", (char *)NULL);
        perror("execlp nl");
        exit(1);
    }

    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);

    for (int i = 0; i < 3; i++) {
        wait(NULL);
    }

    return 0;
}