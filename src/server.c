#include "server.h"

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
#include <signal.h>

#include "request_handler.h"
#include "logger.h"
#include "util.h"
#include "global.h"

void start_server(int port, const char *dir) {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating server socket");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    socklen_t optlen = sizeof(optval);
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&saddr, sizeof(saddr))) {
        perror("Error binding server socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Error listening on socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server started on http://localhost:%d, serving directory %s\n", port, dir);

    printf("Current working directory: %s\n", cwd);

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (!stop_flag) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            if (stop_flag) {
                break;
            }
            perror("Error accepting connection");
            continue;
        }

        handle_client(client_socket, dir);
    }

    close(server_socket);
    unlink(PID_FILE);
}

void stop_server() {
    FILE *f = fopen(PID_FILE, "r");
    if (f) {
        int pid;
        fscanf(f, "%d", &pid);
        fclose(f);
        if (pid > 0) {
            kill(pid, SIGTERM);
        }
        unlink(PID_FILE);
    } else {
        fprintf(stderr, "Server is not running\n");
    }
}
