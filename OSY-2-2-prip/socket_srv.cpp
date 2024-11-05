#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define STR_CLOSE "close"
#define LOG_ERROR 0
#define LOG_INFO  1
#define LOG_DEBUG 2

int g_debug = LOG_INFO;

void log_msg(int log_level, const char *format, ...) {
    const char *prefix[] = { "ERR: ", "INF: ", "DEB: " };
    if (log_level > g_debug) return;

    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    fprintf(log_level == LOG_ERROR ? stderr : stdout, "%s%s\n", prefix[log_level], buffer);
}

int calculator(const char *expression, char *response) {
    int num1, num2, result;
    char op;

    if (sscanf(expression, "%d %c %d", &num1, &op, &num2) != 3) {
        snprintf(response, 256, "Invalid expression format. Use format: <number> <operator> <number>\n");
        return -1;
    }

    switch (op) {
        case '+': result = num1 + num2; break;
        case '-': result = num1 - num2; break;
        case '*': result = num1 * num2; break;
        case '/':
            if (num2 == 0) {
                snprintf(response, 256, "Error: Division by zero.\n");
                return -1;
            }
            result = num1 / num2;
            break;
        default:
            snprintf(response, 256, "Invalid operator. Use +, -, *, or /.\n");
            return -1;
    }

    snprintf(response, 256, "%d %c %d = %d\n", num1, op, num2, result);
    return 0;
}

void handle_client(int client_socket) {
    char buffer[256], response[256];
    while (1) {
        int length = read(client_socket, buffer, sizeof(buffer) - 1);
        if (length <= 0) break;

        buffer[length] = '\0';
        log_msg(LOG_INFO, "Received from client %d: %s", client_socket, buffer);

        if (strncmp(buffer, STR_CLOSE, strlen(STR_CLOSE)) == 0) break;

        // Check buffer is ending '\n'
        if (buffer[length - 1] != '\n') {
            snprintf(response, sizeof(response), "Error: Expression must end with newline '\\n'\n");
        } else {
            // Evaluating expression
            if (calculator(buffer, response) != 0) {
                snprintf(response, sizeof(response), "Invalid expression format or operator.\n");
            }
        }

        // Sending response to client
        write(client_socket, response, strlen(response));
    }

    close(client_socket);
    exit(0);
}

void help(const char *program_name) {
    printf("Usage: %s [-d] <port>\n", program_name);
    printf("Options:\n");
    printf("  -d  Enable debug mode\n");
    printf("  -h  Show help\n");
    exit(0);
}

int main(int argc, char **argv) {
    if (argc < 2) help(argv[0]);

    int server_port = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) g_debug = LOG_DEBUG;
        else if (strcmp(argv[i], "-h") == 0) help(argv[0]);
        else server_port = atoi(argv[i]);
    }

    if (server_port <= 0) {
        log_msg(LOG_ERROR, "Invalid or missing port number.");
        help(argv[0]);
    }

    int listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket == -1) {
        log_msg(LOG_ERROR, "Socket creation failed.");
        exit(1);
    }

    int reuse_option = 1;
    setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_option, sizeof(reuse_option));

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(listening_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        log_msg(LOG_ERROR, "Bind failed.");
        close(listening_socket);
        exit(1);
    }

    if (listen(listening_socket, 10) < 0) {
        log_msg(LOG_ERROR, "Listen failed.");
        close(listening_socket);
        exit(1);
    }

    log_msg(LOG_INFO, "Server listening on port %d", server_port);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int client_socket = accept(listening_socket, (struct sockaddr *)&client_address, &client_len);

        if (client_socket == -1) {
            log_msg(LOG_ERROR, "Accept failed.");
            continue;
        }

        log_msg(LOG_INFO, "Connected: %s:%d",
                inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        pid_t pid = fork();
        if (pid < 0) {
            log_msg(LOG_ERROR, "Fork failed.");
            close(client_socket);
        } else if (pid == 0) { // Child process
            close(listening_socket);
            handle_client(client_socket);
        } else {
            close(client_socket); // Parent process closes client socket
        }
    }

    close(listening_socket);
    return 0;
}