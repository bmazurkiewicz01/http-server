#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFFER_SIZE 2048

void start_server(int port, const char *dir);
void stop_server();
void daemonize();
void handle_client(int client_socket, const char *dir);
void send_404_response(int client_socket);
void send_501_response(int client_socket);
void send_file_response(int client_socket, const char *path, const char *mime_type);
void handle_request(int client_socket, const char *dir, const char *url_path);

int main(int argc, char *argv[]) {
    int opt;
    int port = 0;
    char *dir = NULL;
    int start_flag = 0;
    int stop_flag = 0;

    while ((opt = getopt(argc, argv, "sqp:d:")) != -1) {
        switch (opt) {
            case 's':
                start_flag = 1;
                break;
            case 'q':
                stop_flag = 1;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'd':
                dir = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -s -p port -d dir\n", argv[0]);
                fprintf(stderr, "       %s -q\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (start_flag) {
        if (port == 0 || dir == NULL) {
            fprintf(stderr, "Port and directory must be specified\n");
            exit(EXIT_FAILURE);
        }
        // daemonize();
        start_server(port, dir);
    } else if (stop_flag) {
        // stop_server();
    } else {
        fprintf(stderr, "Usage: %s -s -p port -d dir\n", argv[0]);
        fprintf(stderr, "       %s -q\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return 0;
}

void start_server(int port, const char *dir) {
    int sfds = socket(AF_INET, SOCK_STREAM, 0);
    if (sfds < 0) {
        perror("Error creating server socket");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    socklen_t optlen = sizeof(optval);
    setsockopt(sfds, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sfds, (struct sockaddr *)&saddr, sizeof(saddr))) {
        perror("Error binding server socket");
        close(sfds);
        exit(EXIT_FAILURE);
    }

    if (listen(sfds, 10) < 0) {
        perror("Error listening on socket");
        close(sfds);
        exit(EXIT_FAILURE);
    }

    printf("Server started on http://localhost:%d, serving directory %s\n", port, dir);

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        client_socket = accept(sfds, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error accepting connection");
            continue;
        }

        handle_client(client_socket, dir);
    }
}

void handle_request(int client_socket, const char *dir, const char *url_path) {
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
    char buffer[BUFFER_SIZE];
    int bytes_read;

    bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read < 0) {
        perror("Error reading from socket");
        close(client_socket);
        return;
    }

    buffer[bytes_read] = '\0';
    printf("Received request: %s\n", buffer);

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
}

void send_501_response(int client_socket) {
    const char *content = "<html><body><h1>501 Not Implemented</h1></body></html>";
    int content_length = strlen(content);
    char response[BUFFER_SIZE];

    snprintf(response, sizeof(response),
             "HTTP/1.1 501 Not Implemented\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "\r\n"
             "%s",
             content_length, content);

    send(client_socket, response, strlen(response), 0);
}

void send_404_response(int client_socket) {
    const char *content = "<html><body><h1>404 Not Found</h1></body></html>";
    int content_length = strlen(content);
    char response[BUFFER_SIZE];

    snprintf(response, sizeof(response),
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "\r\n"
             "%s",
             content_length, content);

    send(client_socket, response, strlen(response), 0);
}

void send_file_response(int client_socket, const char *path, const char *mime_type) {
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
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             mime_type, content_length);

    send(client_socket, response, strlen(response), 0);
    send(client_socket, content, content_length, 0);

    free(content);
}

