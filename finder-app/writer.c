#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("Insufficient arguments, Consider usage: %s <filename> <writestr>\n", argv[0]);
        exit(1);
    }
    const char *filename = argv[1];
    const char *writestr = argv[2];
    openlog("writer", LOG_PID, LOG_USER);
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file %s: %s", filename, strerror(errno));
        closelog();
        exit(1);
    }
    syslog(LOG_DEBUG, "Writing %s into %s", writestr, filename);
    size_t len = strlen(writestr);
    ssize_t bytes_written = write(fd, writestr, len);
    if (bytes_written != (ssize_t)len) {
        syslog(LOG_ERR, "Failed to write to file %s: %s", filename, strerror(errno));
        close(fd);
        closelog();
        exit(1);
    }
    close(fd);
    closelog();
    return 0;
}
