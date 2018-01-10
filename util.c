#include "apue.h"
#include "print.h"
#include "ipp.h"
//#include "my_err.h"
//#include "my_log.h"
#include "ourhdr.h"
#include <sys/select.h>
#include <unistd.h>
#define MAXCFGLINE 512
#define MAXKWLEN 16
#define MAXFMTLEN 16


int getaddrlist(const char *host, const char *service, struct addrinfo **ailistpp) {
    int err;
    struct addrinfo hint;

    hint.ai_flags = AI_CANONNAME;
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    hint.ai_addrlen = 0;
    hint.ai_canonname = NULL;
    hint.ai_addr = NULL;
    hint.ai_next = NULL;
    err = getaddrinfo(host, service, &hint, ailistpp);
    return (err);
}

/**
 * 在打印配置文件中搜索指定的关键字
 */
static char *
scan_configfile(char* keyword) {
    int n, match;
    FILE *fp;
    char keybuf[MAXFMTLEN], pattern[MAXFMTLEN];
    char line[MAXCFGLINE];
    static char valbuf[MAXCFGLINE];

    if ((fp = fopen(CONFIG_FILE, "r")) == NULL) {
        log_sys("can't open %s", CONFIG_FILE);
    }

    sprintf(pattern, "%%%ds %%%ds", MAXKWLEN-1, MAXCFGLINE-1);

    match = 0;

    while(fgets(line, MAXCFGLINE, fp) != NULL) {
        n = sscanf(line, pattern, keybuf, valbuf);
        if (n == 2 && strcmp(keyword, keybuf) == 0) {
            match = 1;
            break;
        }
    }

    fclose(fp);

    if (match != 0) {
        return (valbuf);
    }else{
        return NULL;
    }
}

/** 
 * 找到运行打印假脱机守护进程的计算机系统的名字
*/
char* get_printserver(void) {
    return (scan_configfile((char *)"printserver"));
}

/**
 * 获取网络打印机的地址
*/
struct addrinfo* get_printaddr(void) {
    int err;
    char *p;
    struct addrinfo **ailist;

    if ((p = scan_configfile((char *)"printer")) != NULL) {
        if ((err = getaddrlist(p, (char *)"ipp", ailist)) != 0) {

            log_msg("no address information for %s", p);
            return (NULL);
        }

        return (*ailist);
    }

    log_msg("no printer address specified");
    return NULL;
}

/**
 读取指定的字节数，在放弃以前至多阻塞timeout秒
 */
ssize_t tread(int fd, void *buf, size_t nbytes, unsigned int timout){
    int nfds;
    fd_set readfds;

    struct timeval tv;
    tv.tv_sec = timout;
    tv.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    /*使用select 用于确定一个或者多个套接字的状态，等待指定的文件描述符可读*/
    nfds = select(fd+1, &readfds, NULL, NULL, &tv);
    if (nfds <= 0) {

        if (nfds == 0){
            errno = ETIME;//超时
        }

        return -1;
    }

    return (read(fd, buf, nbytes));
}

/**
 *提供tread的变体, 叫做treadn, 它只正好读取请求的字节数(可以防拆包)
 */
ssize_t
treadn(int fd, void *buf, size_t nbytes, unsigned int timeout) {
    size_t nleft;
    ssize_t nread;

    nleft = nbytes;
    while (nleft > 0) {
		if (nread = tread(fd, buf, nleft, timeout) < 0) {
			if (nleft == nbytes) {
			    return -1;
			}else{
			    break;
			}
		}else if (nread == 0) {
		    break;
		}

        nleft -= nread;
        buf = (char*)buf + nread;
    }

    return (nbytes - nleft);
}

ssize_t writen(int fd, const void *buf, size_t num){  
	ssize_t res;  
	size_t n;  
	const char *ptr;  
  
	n = num;  
	ptr = (const char *)buf;  
	while (n > 0) {  
		/* 开始写*/   
		if ((res = write(fd, ptr, n)) <= 0) {  
			if (errno == EINTR)  
			res = 0;  
			else  
			return (-1);  
		}  


		ptr += res;/* 从剩下的地方继续写     */   
		n -= res;  
	}  
  
	return (num);  
}  

ssize_t readn(int fd, void *buf, size_t num){
	ssize_t res;
	size_t n;
	char *ptr;


	n = num;
	ptr = (char*)buf;
	while (n > 0) {
		if ((res = read(fd, ptr, n)) == -1) {
			if (errno == EINTR)
				res = 0;
			else
				return (-1);
		}
		else if (res == 0)
			break;


		ptr += res;
		n -= res;
	}


	return (num - n);
}


