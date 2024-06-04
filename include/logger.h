#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>

void log_request(const char *log_file, const char *request, const char *client_ip, pthread_t tid);

#endif /* LOGGER_H */
