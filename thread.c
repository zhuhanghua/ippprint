
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
 *  �ṩtread�ı���, ����treadn, ��ֻ���ö�ȡ������ֽ���
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
//���������󱻽���ʱ��main�߳���������client_thread, 
//�������Ǵӿͻ�print�����н���Ҫ��ӡ���ļ���
//Ϊÿ���ͻ��˴�ӡ����ֱ���һ���������߳�
void * client_thread(void *arg) {
	int n, fd, sockfd, nr, nw, first;
	long jobid;
	pthread_t tid;
	struct printreq req;
	struct printresp res;
	char name[FILENMSZ];
	char buf[IOBUFSZ];

	tid = pthread_self();
	//��װ�߳����������
	pthread_cleanup_push(client_cleanup, (void*)tid);

	sockfd = (int)arg;
	add_worker(tid, sockfd);

 	/**
 	 * �ӿͻ��˶�ȡ����ͷ��
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
 * �������ӡ��ͨ�ŵ��߳�����
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