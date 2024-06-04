#ifndef GLOBAL_H
#define GLOBAL_H

#include <pthread.h>

#define BUFFER_SIZE 2048
#define PID_FILE "/home/bartosz/daemon_server.pid"

extern int server_socket;
extern int stop_flag;
extern char cwd[BUFFER_SIZE];
extern pthread_mutex_t log_mutex;

#endif /* GLOBAL_H */
