#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>

#define PORT "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUF_SIZE 4096

static volatile sig_atomic_t caught_signal = 0;
static int server_fd = -1;

static void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        caught_signal = 1;
        if (server_fd != -1) {
            shutdown(server_fd, SHUT_RDWR);
        }
    }
}

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') daemon_mode = true;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    if (sigaction(SIGINT, &sa, NULL) != 0 || sigaction(SIGTERM, &sa, NULL) != 0) {
        closelog();
        return -1;
    }

    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) {
        closelog();
        return -1;
    }

    int yes = 1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_fd == -1) continue;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_fd);
            server_fd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);

    if (p == NULL || server_fd == -1) {
        closelog();
        return -1;
    }

    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            close(server_fd);
            closelog();
            return -1;
        }
        if (pid > 0) exit(EXIT_SUCCESS);
        setsid();
        if (chdir("/") != 0) {}
        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > 2) close(devnull);
        }
    }

    if (listen(server_fd, 10) == -1) {
        close(server_fd);
        closelog();
        return -1;
    }

    while (!caught_signal) {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            if (caught_signal) break;
            continue;
        }

        char client_ip[INET6_ADDRSTRLEN] = {0};
        if (client_addr.ss_family == AF_INET) {
            inet_ntop(AF_INET, &(((struct sockaddr_in *)&client_addr)->sin_addr), client_ip, sizeof(client_ip));
        } else {
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&client_addr)->sin6_addr), client_ip, sizeof(client_ip));
        }
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char *packet_buf = NULL;
        size_t packet_len = 0;
        char recv_buf[BUF_SIZE];
        ssize_t bytes_read;

        while ((bytes_read = recv(client_fd, recv_buf, sizeof(recv_buf), 0)) > 0) {
            char *new_buf = realloc(packet_buf, packet_len + bytes_read);
            if (!new_buf) {
                free(packet_buf);
                packet_buf = NULL;
                packet_len = 0;
                break;
            }
            packet_buf = new_buf;
            memcpy(packet_buf + packet_len, recv_buf, bytes_read);
            packet_len += bytes_read;

            char *newline_ptr;
            while ((newline_ptr = memchr(packet_buf, '\n', packet_len)) != NULL) {
                size_t line_len = (newline_ptr - packet_buf) + 1;
                int data_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (data_fd != -1) {
                    if (write(data_fd, packet_buf, line_len) < 0) {}
                    close(data_fd);
                }

                int read_fd = open(DATA_FILE, O_RDONLY);
                if (read_fd != -1) {
                    char file_buf[BUF_SIZE];
                    ssize_t r_bytes;
                    while ((r_bytes = read(read_fd, file_buf, sizeof(file_buf))) > 0) {
                        ssize_t total_sent = 0;
                        while (total_sent < r_bytes) {
                            ssize_t s = send(client_fd, file_buf + total_sent, r_bytes - total_sent, 0);
                            if (s <= 0) break;
                            total_sent += s;
                        }
                    }
                    close(read_fd);
                }

                size_t remaining = packet_len - line_len;
                memmove(packet_buf, newline_ptr + 1, remaining);
                packet_len = remaining;
            }
        }
        free(packet_buf);
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    if (caught_signal) {
        syslog(LOG_INFO, "Caught signal, exiting");
    }
    if (server_fd != -1) close(server_fd);
    remove(DATA_FILE);
    closelog();
    return 0;
}
