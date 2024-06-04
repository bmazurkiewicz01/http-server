#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "global.h"
#include "util.h"

void log_request(const char *log_file, const char *request, const char *client_ip, pthread_t tid) {
    char log_path[BUFFER_SIZE]; 
    snprintf(log_path, sizeof(log_path), "%s/%s", cwd, log_file);

    pthread_mutex_lock(&log_mutex);

    FILE *f = fopen(log_path, "a");
    if (f) {
        char *datetime = current_date_time();
        fprintf(f, "Date/Time: %s\n", datetime);
        fprintf(f, "Client IP: %s\n", client_ip);
        fprintf(f, "Thread ID: %ld\n", (long)tid);
        fprintf(f, "Request: %s\n", request);
        fclose(f);
        free(datetime);
    } else {
        perror("Error opening log file for writing");
    }

    pthread_mutex_unlock(&log_mutex);
}
