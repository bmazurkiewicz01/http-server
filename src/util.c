#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include "global.h"

char *current_date_time() {
    time_t rawtime;
    struct tm *timeinfo;
    char *buffer = (char *)malloc(80 * sizeof(char));
    if (buffer == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);

    return buffer;
}

void daemonize() {
    pid_t pid, sid;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        FILE *f = fopen(PID_FILE, "w");
        if (f) {
            fprintf(f, "%d\n", pid);
            fclose(f);
        }
        exit(EXIT_SUCCESS);
    }

    signal(SIGTERM, signal_handler);
}

void signal_handler(int sig) {
    if (sig == SIGTERM) {
        stop_flag = 1;
        exit(EXIT_SUCCESS);
    }
}
