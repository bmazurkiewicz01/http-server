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

#define BUFFER_SIZE 1024

void start_server(int port, const char *dir);
void stop_server();
void daemonize();
void handle_request(int client_socket, const char *dir);

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

    const char *response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "<html><body><h1>Hello, World!</h1></body></html>";

    write(client_socket, response, strlen(response));
    close(client_socket);
}

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