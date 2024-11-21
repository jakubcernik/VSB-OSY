#include <unordered_map>
#include <string>
#include <mutex>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

struct ImageData {
    sem_t semaphore;
    char *img_data;
    int size;
};

// Mapa pro obdobi
std::unordered_map<std::string, std::string> files = {
    {"jaro", "jaro.jpg"},
    {"leto", "leto.png"},
    {"podzim", "podzim.jpg"},
    {"zima", "zima.png"},
    {"error", "error.png"}
};

std::unordered_map<std::string, ImageData> images;
std::mutex images_mutex;

void load_image(const char* filename, ImageData &image_data) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open image");
        exit(EXIT_FAILURE);
    }
    
    fseek(file, 0, SEEK_END);
    image_data.size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    image_data.img_data = (char*) malloc(image_data.size);
    if (!image_data.img_data) {
        perror("Memory error");
        exit(EXIT_FAILURE);
    }
    
    fread(image_data.img_data, 1, image_data.size, file);
    fclose(file);
}

void init_images() {
    for (const auto &pair : files) {
        ImageData &img_data = images[pair.first];
        sem_init(&img_data.semaphore, 0, 1);
        img_data.img_data = nullptr;
        img_data.size = 0;
    }
}

void send_image(int client_socket, ImageData &image) {
    int sent = 0;
    int delay = 10000000 / (image.size / BUFFER_SIZE);

    while (sent < image.size) {
        int chunk = (image.size - sent > BUFFER_SIZE) ? BUFFER_SIZE : (image.size - sent);
        write(client_socket, image.img_data + sent, chunk);
        sent += chunk;
        usleep(delay);
    }
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int len = read(client_socket, buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        close(client_socket);
        return NULL;
    }
    
    buffer[len] = '\0';
    std::string request(buffer);
    std::string season = request.substr(5, request.size() - 6);

    ImageData *image_data;
    std::string filename;
    {
        std::lock_guard<std::mutex> lock(images_mutex);
        if (images.find(season) != images.end()) {
            image_data = &images[season];
            filename = files[season];  // map
        } else {
            image_data = &images["error"];
            filename = files["error"];
        }

        if (image_data->img_data == nullptr) {
            load_image(filename.c_str(), *image_data); 
        }
    }

    // Lock the image with the semaphore
    sem_wait(&image_data->semaphore);
    send_image(client_socket, *image_data);
    sem_post(&image_data->semaphore);

    close(client_socket);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    init_images();

    int server_port = atoi(argv[1]);
    int listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(listening_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(listening_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(listening_socket, 10) < 0) {
        perror("Listen failed");
        close(listening_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", server_port);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int client_socket = accept(listening_socket, (struct sockaddr *)&client_address, &client_len);

        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        int *new_sock = (int *)malloc(sizeof(int));
        *new_sock = client_socket;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_handler, (void *)new_sock) < 0) {
            perror("Could not create thread for client");
            close(client_socket);
            free(new_sock);
            continue;
        }

        pthread_detach(client_thread);
    }

    close(listening_socket);
    return 0;
}
