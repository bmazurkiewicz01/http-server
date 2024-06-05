#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "global.h"
#include "server.h"
#include "util.h"

int server_socket = -1;
int stop_flag = 0;
char cwd[BUFFER_SIZE];

int main(int argc, char *argv[]) {
    int opt;
    int port = 0;
    char *dir = NULL;
    int start_flag = 0;

    while ((opt = getopt(argc, argv, "sqp:d:")) != -1) {
        switch (opt) {
            case 's':
                start_flag = 1;
                break;
            case 'q':
                stop_server();
                exit(EXIT_SUCCESS);
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

        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("Error getting current working directory");
            exit(EXIT_FAILURE);
        }

        daemonize();
        start_server(port, dir);
    } else {
        fprintf(stderr, "Usage: %s -s -p port -d dir\n", argv[0]);
        fprintf(stderr, "       %s -q\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return 0;
}
