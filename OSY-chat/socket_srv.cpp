#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <vector>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <string>
#include <fcntl.h>
#include <poll.h>
#include <ctime>
#include <sstream>

#define STR_CLOSE "close"
#define LOG_ERROR 0
#define LOG_INFO  1
#define LOG_DEBUG 2

int g_debug = LOG_INFO;
std::unordered_map<int, std::string> client_nicks;
std::mutex client_mutex;
int pipe_fd[2];

void broadcast_message(const std::string& sender, std::string message, bool include_sender = false) {
    std::lock_guard<std::mutex> lock(client_mutex);

    // Odebereme koncový '\n', pokud je přítomen, abychom předešli zdvojení řádků
    if (!message.empty() && message.back() == '\n') {
        message.pop_back();
    }

    std::string formatted_message = sender + ": " + message + "\n";  // Přidáme '\n' na konec zprávy
    for (const auto& [sock, nick] : client_nicks) {
        if (!include_sender && sender == nick) continue;  // Neodesílat zpět odesílateli
        write(sock, formatted_message.c_str(), formatted_message.size());
    }
}




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

// Funkce pro obsluhu klienta
void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[256];
    std::string client_nick;
    bool nick_set = false;

    while (1) {
        int length = read(client_socket, buffer, sizeof(buffer) - 1);
        if (length <= 0) break;
        buffer[length] = '\0';

        if (!nick_set) {
            if (strncmp(buffer, "#nick ", 6) == 0) {
                client_nick = std::string(buffer + 6, length - 6);
                nick_set = true;

                {
                    std::lock_guard<std::mutex> lock(client_mutex);
                    client_nicks[client_socket] = client_nick;
                }
                broadcast_message(client_nick, " has joined the chat.", true);
            } else {
                log_msg(LOG_INFO, "Client ignored without nickname.");
                continue;
            }
        } else if (strcmp(buffer, "#list\n") == 0) {
            std::string list_message = "Connected users:\n";
            {
                std::lock_guard<std::mutex> lock(client_mutex);
                for (const auto& [sock, nick] : client_nicks) {
                    list_message += " - " + nick + "\n";
                }
            }
            write(client_socket, list_message.c_str(), list_message.size());
        } else {
            broadcast_message(client_nick, buffer, false);
        }
    }

    {
        std::lock_guard<std::mutex> lock(client_mutex);
        client_nicks.erase(client_socket);
    }
    broadcast_message(client_nick, " has left the chat.", true);

    write(pipe_fd[1], &client_socket, sizeof(client_socket));
    close(client_socket);
    return NULL;
}


void help(const char *program_name) {
    printf("Usage: %s [-d] <port>\n", program_name);
    exit(0);
}

int main(int argc, char **argv) {
    if (argc < 2) help(argv[0]);

    int server_port = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) g_debug = LOG_DEBUG;
        else server_port = atoi(argv[i]);
    }

    if (pipe(pipe_fd) < 0) {
        perror("Pipe creation failed");
        exit(1);
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

    pollfd poll_fds[2];
    poll_fds[0].fd = listening_socket;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = pipe_fd[0];
    poll_fds[1].events = POLLIN;

    while (1) {
        int poll_result = poll(poll_fds, 2, -1);

        if (poll_result < 0) {
            log_msg(LOG_ERROR, "Poll error.");
            break;
        }

        if (poll_fds[0].revents & POLLIN) {
            struct sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            int client_socket = accept(listening_socket, (struct sockaddr *)&client_address, &client_len);

            if (client_socket == -1) {
                log_msg(LOG_ERROR, "Accept failed.");
                continue;
            }

            pthread_t client_thread;
            int *new_sock = (int *)malloc(sizeof(int));
            *new_sock = client_socket;

            if (pthread_create(&client_thread, NULL, client_handler, (void *)new_sock) < 0) {
                log_msg(LOG_ERROR, "Could not create thread for client.");
                close(client_socket);
                continue;
            }

            pthread_detach(client_thread);
        }

        if (poll_fds[1].revents & POLLIN) {
            int client_socket;
            read(pipe_fd[0], &client_socket, sizeof(client_socket));

            std::lock_guard<std::mutex> lock(client_mutex);
            client_nicks.erase(client_socket);
        }
    }

    close(listening_socket);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return 0;
}
