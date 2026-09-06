#include "aesdsocket.h"
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>

volatile sig_atomic_t caught_signal = 0;

void signal_handler(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) {
        caught_signal = 1;
    }
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
void handle_connection(int client_fd) {
    int fd;
    char recv_buf[MAXDATASIZE];
    char *packet_buf = NULL;
    size_t packet_size = 0;
    ssize_t bytes_received;
    char *newline_ptr = NULL;
    fd = open(PATHFILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("Couldn't open file");
        close(client_fd);
        return;
    }
    while ((bytes_received = recv(client_fd, recv_buf, sizeof(recv_buf), 0)) > 0) {
        char *new_buf = realloc(packet_buf, packet_size + bytes_received);
        if (!new_buf) {
            perror("Failed to allocate memory");
            free(packet_buf);
            close(fd);
            close(client_fd);
            return;
        }
        packet_buf = new_buf;
        memcpy(packet_buf + packet_size, recv_buf, bytes_received);
        packet_size += bytes_received;
        newline_ptr = memchr(packet_buf, '\n', packet_size);
        if (newline_ptr != NULL) {
            break;
        }
    }
    if (bytes_received < 0) {
        perror("recv");
        free(packet_buf);
        close(fd);
        close(client_fd);
        return;
    }
    if (packet_buf == NULL) {
        close(fd);
        close(client_fd);
        return;
    }
    size_t len = (newline_ptr != NULL) ? (size_t)(newline_ptr - packet_buf) + 1
                                       : packet_size;
    ssize_t bytes_written = write(fd, packet_buf, len);
    if (bytes_written != (ssize_t)len) {
        close(fd);
        syslog(LOG_ERR, "Failed to write to file");
        free(packet_buf);
        return;
    }
    free(packet_buf);
    close(fd);
    fd = open(PATHFILE, O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read;
        char read_buf[MAXDATASIZE];
        while ((bytes_read = read(fd, read_buf, sizeof(read_buf))) > 0) {
            if (send(client_fd, read_buf, bytes_read, 0) < 0) {
                perror("send to client");
                break;
            }
        }
        close(fd);
    } else {
        perror("Erro while opening read file");
    }
    close(client_fd);
}
int main(int argc, char *argv[]){
    int daemonize = 0;
    int c;
    while ((c = getopt(argc, argv, "d")) != -1) {
        if (c == 'd') daemonize = 1;
        else { 
            return -1; 
        }
    }
    if (daemonize) {
        pid_t pid = fork();
        if (pid < 0) { 
            perror("fork"); 
            return -1; 
        }
        if (pid > 0) 
            return 0;
        if (setsid() < 0) { 
            perror("setsid"); 
            return -1; 
        }
        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd >= 0) {
            dup2(nullfd, STDIN_FILENO);
            dup2(nullfd, STDOUT_FILENO);
            dup2(nullfd, STDERR_FILENO);
            if (nullfd > STDERR_FILENO) close(nullfd);
        }
    }
   int sock_fd, client_fd = -1, optval = 1;
   struct sockaddr_in server_addr;
   struct sockaddr_storage claddr;
   socklen_t len;
   char claddrStr[INET_ADDRSTRLEN];
   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_flags = 0;
   sa.sa_handler = signal_handler;
   sigemptyset(&sa.sa_mask);
   openlog("aesdsocket", LOG_PID, LOG_USER);
   if(sigaction(SIGINT, &sa, NULL) != 0){
       perror("Error registrating SIGINT");
       return -1;
   } 
   if(sigaction(SIGTERM, &sa, NULL) != 0){
       perror("Error registrating SIGINT");
       return -1;
   }
   
   sock_fd = socket(AF_INET, SOCK_STREAM, 0);
   if(sock_fd < 0){
       perror("Failed to open socket");
       return -1;   
   }
   memset(&server_addr , 0, sizeof(struct sockaddr_in));
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(PORT);   
   server_addr.sin_addr.s_addr = INADDR_ANY;
   if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0){
       close(sock_fd);    
       perror("setsockopt");
           return -1;
   }
   if(bind(sock_fd, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in)) < 0){
       close(sock_fd); 
       perror("Couldn't bind socket to port");
        return -1;
   }
   if(listen(sock_fd, BACKLOG) < 0){
       close(sock_fd);
       perror("Error initializing socket in listen mode");
       return -1;
   }
   while(!caught_signal){
       len = sizeof(struct sockaddr_storage);
       client_fd = accept(sock_fd, (struct sockaddr *) &claddr, &len);
       if(client_fd < 0){
            perror("Couldn't connect to client");
            continue;
       }
       inet_ntop(claddr.ss_family, get_in_addr((struct sockaddr *) &claddr), claddrStr, sizeof(claddrStr));
       syslog(LOG_INFO, "Accepted connection from %s", claddrStr);
       handle_connection(client_fd);
       syslog(LOG_INFO, "Closed connection from %s", claddrStr);
    }
    syslog(LOG_INFO, "Caught signal, exiting");
    if(sock_fd != -1)
        close(sock_fd);
    if (client_fd != -1)
        close(client_fd);
    remove(PATHFILE);
    closelog();
}

