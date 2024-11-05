#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define STR_CLOSE "close"
#define TIMEOUT_MS 150000  // Timeout 150 sekund
#define NICK_TIMEOUT 200000  // Timeout pro zadání přezdívky (200 sekund)

#define LOG_ERROR 0
#define LOG_INFO  1
#define LOG_DEBUG 2

int g_debug = LOG_INFO;
char nickname[50];  // Přezdívka klienta
char last_message[128];  // Uchová poslední odeslanou zprávu


void log_msg(int log_level, const char *format, ...) {
    const char *out_fmt[] = {
        "ERR: %s\n",
        "INF: %s\n",
        "DEB: %s\n"
    };

    if (log_level > g_debug) return;

    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    fprintf(log_level == LOG_ERROR ? stderr : stdout, out_fmt[log_level], buffer);
}

void help(const char *program_name) {
    printf(
        "\nSocket client example.\n\n"
        "Usage: %s [-h -d] ip_or_name port_number\n\n"
        "  -d  debug mode\n"
        "  -h  this help\n\n", program_name
    );
    exit(0);
}

int main(int argc, char **argv) {
    if (argc <= 2) help(argv[0]);

    int server_port = 0;
    char *server_host = nullptr;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d")) g_debug = LOG_DEBUG;
        else if (!strcmp(argv[i], "-h")) help(argv[0]);
        else if (*argv[i] != '-' && !server_host) server_host = argv[i];
        else if (*argv[i] != '-' && !server_port) server_port = atoi(argv[i]);
    }

    if (!server_host || !server_port) {
        log_msg(LOG_INFO, "Host or port is missing!");
        help(argv[0]);
        exit(1);
    }

    log_msg(LOG_INFO, "Connecting to '%s':%d.", server_host, server_port);

    addrinfo address_info_request, *address_info_answer;
    memset(&address_info_request, 0, sizeof(address_info_request));
    address_info_request.ai_family = AF_INET;
    address_info_request.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server_host, nullptr, &address_info_request, &address_info_answer) != 0) {
        log_msg(LOG_ERROR, "Unknown host name!");
        exit(1);
    }

    sockaddr_in client_address = *(sockaddr_in *) address_info_answer->ai_addr;
    client_address.sin_port = htons(server_port);
    freeaddrinfo(address_info_answer);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        log_msg(LOG_ERROR, "Unable to create socket.");
        exit(1);
    }

    if (connect(server_socket, (sockaddr *)&client_address, sizeof(client_address)) < 0) {
        log_msg(LOG_ERROR, "Unable to connect to server.");
        close(server_socket);
        exit(1);
    }

    log_msg(LOG_INFO, "Connected to server.");

    // Vyžádání přezdívky od uživatele s timeoutem 20 sekund
    printf("Please enter your nickname (format: #nick <name>): ");
    fflush(stdout);

    pollfd nick_poll = {STDIN_FILENO, POLLIN, 0};
    int poll_result = poll(&nick_poll, 1, NICK_TIMEOUT);
    if (poll_result == 0) {
        log_msg(LOG_INFO, "Nickname entry timed out. Disconnecting...");
        close(server_socket);
        return 0;
    } else if (poll_result < 0) {
        log_msg(LOG_ERROR, "Error while waiting for nickname.");
        close(server_socket);
        return 1;
    }

    int length = read(STDIN_FILENO, nickname, sizeof(nickname) - 1);
    if (length <= 0 || strncmp(nickname, "#nick ", 6) != 0) {
        log_msg(LOG_INFO, "Invalid nickname format. Disconnecting...");
        close(server_socket);
        return 0;
    }

    nickname[length] = '\0';
    write(server_socket, nickname, length);

    pollfd poll_fds[2];
    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = server_socket;
    poll_fds[1].events = POLLIN;

    srand(time(NULL));

    while (1) {
        char buffer[128];
        
        int poll_result = poll(poll_fds, 2, TIMEOUT_MS);

        if (poll_result < 0) {
            log_msg(LOG_ERROR, "Poll error.");
            break;
        }

        if (poll_result == 0) {
            snprintf(buffer, sizeof(buffer), "sleeping...\n");
            write(server_socket, buffer, strlen(buffer));
            continue;
        }

        if (poll_fds[0].revents & POLLIN) {
            int data_length = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            if (data_length <= 0) {
                log_msg(LOG_ERROR, "Unable to read from stdin.");
                break;
            }

            buffer[data_length] = '\0';

            // Odebereme koncové zalomení řádku
            if (buffer[data_length - 1] == '\n') {
                buffer[data_length - 1] = '\0';
            }

            strncpy(last_message, buffer, sizeof(last_message));  // Uložíme čistý text zprávy
            write(server_socket, buffer, strlen(buffer));

            printf("%s\n", buffer);  // Výpis odeslané zprávy bez jména
        }

        if (poll_fds[1].revents & POLLIN) {
            int data_length = read(server_socket, buffer, sizeof(buffer) - 1);
            if (data_length <= 0) {
                log_msg(LOG_INFO, "Server closed the connection.");
                break;
            }

            buffer[data_length] = '\0';

            // Kontrola, jestli přijatá zpráva není totožná s poslední odeslanou zprávou
            if (strcmp(buffer, last_message) != 0) {
                printf("%s", buffer);  // Zobrazí zprávu bez dodatečného '\n'
            }
        }

    }

    close(server_socket);
    return 0;
}
