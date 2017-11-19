
/**
 *
 */
 ssize_t tread(int fd, void *buf, size_t nbytes, unsigned int timeout){
	int nfds;
	fd_set readfds;
	struct timeval tv;
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	nfds = select(fd+1, &readfds, NULL, NULL, &tv);
	if (nfds <= 0) {
		if (nfds == 0) {
			errno = ETIME;
		}
		return -1;
	}

	return (read(fd, buf, nbytes));
 }

/**
 *  提供tread的变体, 叫做treadn, 它只正好读取请求的字节数
 */
 ssize_t treadn(int fd, void *buf, size_t nbtytes, unsigned int timeout) {
	size_t nleft;
	size_t nread;

	nleft = nbtytes;
	while(nleft > 0) {
		if ((nread = tread(fd, buf, nbtytes, timeout)) < 0) {
			if (nleft == nbtytes) {
				return -1;
			}else {
				break;
			}
		}else if (nread == 0) {
			break; /* EOF */
		}
			nleft -= nread;
			buf += nread;
	}

	return (nbtytes - nleft);
 }
//当连接请求被接受时，main线程中派生出client_thread, 
//其作用是从客户print命令中接收要打印的文件。
//为每个客户端打印请求分别建立一个独立的线程
void * client_thread(void *arg) {
	int n, fd, sockfd, nr, nw, first;
	long jobid;
	pthread_t tid;
	struct printreq req;
	struct printresp res;
	char name[FILENMSZ];
	char buf[IOBUFSZ];

	tid = pthread_self();
	//安装线程清理处理程序
	pthread_cleanup_push(client_cleanup, (void*)tid);

	sockfd = (int)arg;
	add_worker(tid, sockfd);

 	/**
 	 * 从客户端读取请求头部
 	 */
	if ((n = treadn(sockfd, &req, sizeof(struct printreq), 10)) != sizeof(struct printreq)) {
		res.jobid = 0;
		if(n < 0) {
			res.retcode = htonl(errno);
		} else {
			res.retcode = htonl(EIO);
		}

		strncpy(res.msg, strerror(res.retcode), MSGLEN_MAX);
	}
}

/**
 * 与网络打印机通信的线程运行
 */
void printer_thread(void *arg) {
	struct job *jp;
	int hlen, ilen, sockfd, fd, nr, nw;
	char *icp, *hcp;
	struct ipp_hdr *hp;
	struct stat sbuf;
	struct ipvec iov[2];
	char name[FILENMSZ];
	char hbuf[HBUFSZ];
	char ibuf[IBUFSZ];
	char buf[IOBUFSZ];
	char str[64];

	for (;;) {
	}
}