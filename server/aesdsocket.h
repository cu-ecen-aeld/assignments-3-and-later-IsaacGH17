#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>

#define PORT 9000
#define BACKLOG 10
#define MAXDATASIZE 100
#define PATHFILE "/var/tmp/aesdsocketdata"
