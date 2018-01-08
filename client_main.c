#include "apue.h"
#include "print.h"

extern int optind, opterr, optopt;
extern char *optarg;

int log_to_stderr = 1;

void submit_file(int, int, const char*, size_t, int);

#define MAXSLEEP 128

int connect_retry(int sockfd, const struct sockaddr *addr, socklen_t alen)
{
    int nsec;

    /*
    * Try to connect with exponential backoff.
    */
    for(nsec = 1; nsec <= MAXSLEEP; nsec <<= 1)
    {
        if(connect(sockfd, addr, alen) == 0)
        {
            /*
            * Connection accepted. 
            */
            return(0);
        }
        
        /*
        * Delay before trying again.
        */
        if(nsec <= MAXSLEEP/2)
            sleep(nsec);
    }
    return(-1);
}

int main(int argc, char *argv[]) {
	int fd, sockfd, err, text, c;
	struct stat sbuf;
	char *host;
	struct addrinfo *ailist, *aip;

	err = 0;
	text = 0;
	while ((c = getopt(argc, argv, "t")) != -1) {
		switch(c) {
		case 't': 
			text = 1;//按照文本方式打印
			break;
		case '?':
			err = 1;
			break;
		}
	}

	if (err || (optind != argc -1)) {
		err_quit("usage: print [-t] filename");
	}

	if ((fd = open(argv[optind], O_RDONLY)) < 0) {
		err_sys("print: can't open %s", argv[1]);
	}

	if (fstat(fd, &sbuf) < 0) {
		err_quit("print: can't open %s\n", argv[1]);
	}

	//文件是一个普通文件
	if (!(S_ISREG(sbuf.st_mode))) {
		err_quit("print: %s must be a regular file\n", argv[1]);
	}

	if ((host = get_printserver()) == NULL) {
		err_quit("print: no print server defined");
	}

	if ((err = getaddrlist(host, "print", &ailist))) {
		err_quit("print: getaddrinfo error: %s\n", gai_strerror(err));
	}

	for (aip = ailist; aip != NULL; aip = ailist->ai_next) {
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			err = errno;
		}else if(connect_retry(sockfd, aip, aip->ai_addrlen) < 0) {
			err = errno;
		}else{
			submit_file(fd, sockfd, argv[1], sbuf.st_size, text);
			exit(0);
		}
	}

	errno = err;
	err_ret("print: can't contact %s", host);
	exit(1);
}

/**
 * 将文件传送到打印假脱机守护进程
 */
void submit_file(int fd, int sockfd, const char *fname, size_t nbytes, int text){
	int nr, nw, len;
	struct passwd *pwd;
	struct printreq req;
	struct printresp res;
	char buf[IOBUFSZ];

	/**
	 * 首先建立请求首部
	 */
	if ((pwd = getpwuid(geteuid())) == NULL) {
		strcpy(req.usernm, "unknown");
	}else {
		strcpy(req.usernm, pwd->pw_name);
	}

	req.size = htonl(nbytes);
	if (text) {
		req.flags = htonl(PR_TEXT);//纯文本格式
	}else {
		req.flags = 0;
	}

	strcpy(req.jobjnm, fname);

 	/**
 	 * 发送文件头到守护进程
 	 */
	nw = writen(sockfd, &req, sizeof(struct printreq));
	if (nw != sizeof(struct printreq)) {
		if (nw < 0) {
			err_sys("can't write to print server");
		} else {
			err_quit("short write (%d/%d) to print server", nw,  sizeof(struct printreq));
		}
	}

 	/**
 	 * 发送文件到守护进程
 	 */
	while( (nr = read(fd, buf, IOBUFSZ)) != 0) {
		nw = writen(sockfd, buf, nr);
		if (nw != nr) {
			if (nw < 0) {
				err_sys("can't write to print server");
			}else{
				err_quit("short write (%d/%d) to print server", nw, nr);
			}
		}
	}

	/**
 	 * 获取响应
 	 */
 	if ((nr = readn(sockfd, &res, sizeof(struct printresp))) != sizeof(struct printresp)){
		err_sys("cant't read response from server");
 	}

 	if (res.retcode != 0) {
		printf("rejected: %s\n", res.msg);
 	}else {
		printf("job Id %d", ntonl(res.jobid));
 	}

 	exit(0);
}
