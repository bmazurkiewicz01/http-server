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
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 2048
#define PID_FILE "/home/bartosz/daemon_server.pid"

typedef struct {
    int client_socket;
    const char *dir;
} HandleRequestArgs;

int server_socket = -1;
int stop_flag = 0;
char cwd[BUFFER_SIZE];
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void start_server(int port, const char *dir);
void stop_server();
void daemonize();
void handle_client(int client_socket, const char *dir);
void send_404_response(int client_socket);
void send_501_response(int client_socket);
void send_file_response(int client_socket, const char *path, const char *mime_type);
void handle_request(int client_socket, const char *dir, const char *url_path);
void * handle_request_thread(void *arg);
void signal_handler(int sig);
char *current_date_time();
void log_request(const char *log_file, const char *request, const char *client_ip, pthread_t tid);

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

void send_501_response(int client_socket) {
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

void send_404_response(int client_socket) {
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
             "Connection: close\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             mime_type, content_length);

    send(client_socket, response, strlen(response), 0);
    send(client_socket, content, content_length, 0);

    free(content);
}

void signal_handler(int sig) {
    if (sig == SIGTERM) {
        stop_flag = 1;
    }
}
