#include "request_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/stat.h>

#include "logger.h"
#include "util.h"
#include "global.h"

static void send_501_response(int client_socket) {
    const char *content = "<html><body><h1>501 Not Implemented</h1></body></html>";
    int content_length = strlen(content);
    char response[BUFFER_SIZE];

    snprintf(response, sizeof(response),
             "HTTP/1.1 501 Not Implemented\r\n"
             "Connection: close\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "\r\n"
             "%s",
             content_length, content);

    send(client_socket, response, strlen(response), 0);
}

static void send_404_response(int client_socket) {
    const char *content = "<html><body><h1>404 Not Found</h1></body></html>";
    int content_length = strlen(content);
    char response[BUFFER_SIZE];

    snprintf(response, sizeof(response),
             "HTTP/1.1 404 Not Found\r\n"
             "Connection: close\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "\r\n"
             "%s",
             content_length, content);

    send(client_socket, response, strlen(response), 0);
}

static void send_file_response(int client_socket, const char *path, const char *mime_type) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        send_404_response(client_socket);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat failed");
        close(fd);
        send_404_response(client_socket);
        return;
    }

    int content_length = st.st_size;
    char *content = malloc(content_length);
    if (!content) {
        perror("malloc failed");
        close(fd);
        send_404_response(client_socket);
        return;
    }

    if (read(fd, content, content_length) < 0) {
        perror("read failed");
        free(content);
        close(fd);
        send_404_response(client_socket);
        return;
    }

    close(fd);

    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Connection: close\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             mime_type, content_length);

    send(client_socket, response, strlen(response), 0);
    send(client_socket, content, content_length, 0);

    free(content);
}

static void handle_request(int client_socket, const char *dir, const char *url_path) {
    char path[BUFFER_SIZE];

    if (url_path == NULL || strcmp(url_path, "/") == 0) {
        snprintf(path, sizeof(path), "%s/index.html", dir);
    } else {
        snprintf(path, sizeof(path), "%s%s", dir, url_path);
    }

    struct stat st;
    if (stat(path, &st) < 0 || S_ISDIR(st.st_mode)) {
        send_404_response(client_socket);
        return;
    }

    const char *mime_type = "text/html";

    if (strstr(path, ".html")) {
        mime_type = "text/html";
    } else if (strstr(path, ".jpg")) {
        mime_type = "image/jpeg";
    } else if (strstr(path, ".png")) {
        mime_type = "image/png";
    } else if (strstr(path, ".css")) {
        mime_type = "text/css";
    } else if (strstr(path, ".js")) {
        mime_type = "application/javascript";
    } else {
        mime_type = "application/octet-stream";
    }

    send_file_response(client_socket, path, mime_type);
}



void handle_client(int client_socket, const char *dir) {
    pthread_t thread;
    HandleRequestArgs *args = malloc(sizeof(HandleRequestArgs));
    if (args == NULL) {
        perror("Error allocating memory for handle request arguments");
        close(client_socket);
        return;
    }

    args->client_socket = client_socket;
    args->dir = dir;

    if (pthread_create(&thread, NULL, handle_request_thread, (void *)args) != 0) {
        perror("Error creating request handling thread");
        close(client_socket);
        free(args);
        return;
    }

    pthread_detach(thread);
}

void *handle_request_thread(void *arg) {
    HandleRequestArgs *args = (HandleRequestArgs *)arg;
    int client_socket = args->client_socket;
    const char *dir = args->dir;
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_read;

    bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read < 0) {
        perror("Error reading from socket");
        close(client_socket);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    printf("Received request: %s\n", buffer);

    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    if (getpeername(client_socket, (struct sockaddr *)&client_addr, &client_len) == 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    } else {
        strcpy(client_ip, "Unknown");
    }

    pthread_t tid = pthread_self();

    log_request("server.log", buffer, client_ip, tid);

    if (strncmp(buffer, "GET ", 4) == 0) {
        char *url_path = strtok(buffer + 4, " ");
        if (url_path == NULL) {
            send_404_response(client_socket);
        } else {
            handle_request(client_socket, dir, url_path);
        }
    } else {
        send_501_response(client_socket);
    }

    close(client_socket);
    return NULL;
}
