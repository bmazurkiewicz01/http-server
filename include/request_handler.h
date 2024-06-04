#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

typedef struct {
    int client_socket;
    const char *dir;
} HandleRequestArgs;

void *handle_request_thread(void *arg);
void handle_client(int client_socket, const char *dir);

#endif /* REQUEST_HANDLER_H */
