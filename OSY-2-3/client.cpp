#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

void show_image(const char *filename) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("display", "display", filename, (char *)NULL);
        perror("Failed to open image");
        exit(EXIT_FAILURE);
    } else {
        wait(NULL);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port> [command]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char request[BUFFER_SIZE];

    // Pokud je třetí argument zadán, použije se jako celý příkaz
    if (argc == 4) {
        snprintf(request, sizeof(request), "#img %s\n", argv[3]);
    } else {
        printf("Enter command (#img season): ");
        fflush(stdout);
        if (fgets(request, sizeof(request), stdin) == NULL) {
            fprintf(stderr, "Failed to read input.\n");
            exit(EXIT_FAILURE);
        }
    }


    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    write(sock, request, strlen(request));

    // .img soubror
    char filename[50];
    snprintf(filename, sizeof(filename), "%d.img", getpid());

    int file_fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("File creation failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    int received;
    while ((received = read(sock, buffer, sizeof(buffer))) > 0) {
        write(file_fd, buffer, received);
    }

    close(file_fd);
    close(sock);

    // exec
    show_image(filename);
    return 0;
}
