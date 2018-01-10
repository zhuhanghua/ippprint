 
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "my_err.h"
#include "my_log.h"


#define log_sys print
#define log_info print
#define log_error print