

void init_request(void) {
	int n;
	char name[FILENMSZ];

	sprintf(name, "%s/%s", SPOOLDIR, JOBFILE);
	//��ʼ���ļ�������
	jobfd = open(name, O_CREATE|O_RDWR, S_IRUSR|S_IWUSR);

	//��һ����¼��
	if (write_lock(jobfd, 0, SEEK_SET, 0)< 0){
		log_quit("daemon already running");
	}

	if ((n = read(jobfd, name, FILENMSZ)) < 0){
		log_sys("can't read job file");
	}

	if (n == 0){
		nextjob = 1;
	}else{
		nextjob = atol(name);
	}
}

void init_printer(void) {
	printer = get_printaddr();
	if (printer == NULL) {
		log_msg("no printer device registered");
		exit(-1);
	}

	printer_name = printer->ai_canonname;
	if (printer_name == NULL) {
		printer_name = "printer";
	}
	log_msg("printer is %s", printer_name);
}

int initserver() {
    struct sockaddr_in serveraddr;
    socklen_t sockaddrlen;
	int err;
    int reuse = 1;
	//����tcp�׽���
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);//tcp Э��: ��������
    if(sockfd < 0){
        perror("socket");
        return -1;
    }

	//���õ�ַΪ���ظ�ʹ��
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == -1) {
		err = errno;
		goto errout;
	}
    
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);//���ñ�����ַ
	serveraddr.sin_port = htons(8080);//�����ͱ����������ֽ�˳��ת��������ֽ�˳��,�����ֽ�˳�����big-endian����ʽ
	memset(serveraddr.sin_zero, 0, sizeof(serveraddr.sin_zero)); 
	
	//���׽������ַ��Ϣ
	if (bind(sockfd, (struct sockaddr_in*)&serveraddr, sizeof(serveraddr)) == -1) {
		err = errno;
		goto errout;
	}

	//������������Խ�����������
	if (listen(serveraddr, 128) == -1) {
		err = errno;
		goto errout;
	}

	return sockfd;
errout: 
	close(fd);
	errno = err;
	return -1;
}

/**
 * ���ػ����̳�������ʱ������build_qonstart�Ӵ洢��/var/spool/printer/reqs�еĴ����ļ�����
 * һ���ڴ��еĴ�ӡ��ҵ�б�������ܴ򿪸�Ŀ¼����ʾû�д�ӡ��ҵҪ����
 */
void build_qonstart(void) {
	int fd, err, nr;
	long jobid;
	DIR *dirp;
	struct dirent *entp;
	struct printreq req;
	char dname[FILENMSZ], fname[FILENMSZ];

	sprintf(dname, "%s/%s", SPOOLDIR, REQDIR);
	if ((dirp = opendir(dname)) == NULL)
		return;
	
	while((entp = readdir(dirp, ".")) != NULL) {
		if (strcmp(entp->d_name, ".") == 0 || strcmp(entp->d_name, "..") == 0) {
			continue;
		}

		sprintf(fname, "%s/%s/%s", SPOOLDIR, REQDIR, entp->d_name);
		if ((fd = open(fname, O_RDONLY)) < 0) {
			continue;
		}

		//��ȡ�������ļ��е�printreq�ṹ
		nr = read(fd, &req, sizeof(struct printreq));
		if (nr != sizeof(struct printreq)) {
			if (nr < 0) {
				err = errno;
			}else {
				err = EIO;
			}

			close(fd);
			log_msg("build_qonstart: can't read %s: %s", fname, strerror(err));
			unlink(fname);
			sprintf(fname, "%s/%s/%s", SPOOLDIR, DATADIR, entp->d_name);
			unlink(fname);
			continue;
		}
		//���ļ���ת��Ϊ��ҵID
		jobid = atol(entp->d_name);
		log_msg("adding job %ld to queue", jobid);
		//��������뵽����Ĵ�ӡ��ҵ�б� 
		add_job(&req, jobid);
	}		
}

int main(int argc, char *argv[]) {
	pthread_t tid;
	struct addrinfo *ailist, *aip;
	int sockfd, err, i, n, maxfd;
	char *host;
	fd_set rendezvous, rset;
	struct sigaction sa;
	struct passwd *pwdp;

	//��ʼ���ػ����̳���
	if (argc ��= 1) {
		err_quit("usage: printd");
	}

	daemonize("printd");
	sigemptyset(&sa.sa_mask);
	ss.sa_flags = 0;
	ss.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) < 0) {
		log_sys("sigaction failed");
	}

	sigemptyset(&mask);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGTERM);

	if ((err = pthread_sigmask(SIG_BLOCK, &mask, NULL)) != 0) {
		log_sys("pthread_sigmask failed");
	}

	init_request();
	init_printer();

	#ifdef _SC_HOST_NAME_MAX
	n = sysconf(_SC_HOST_NAME_MAX);
	if (n < 0)
	#endif
	n = HOST_NAME_MAX;

	if ((host = malloc(n)) == NULL) {
		log_sys("malloc error");
	}

	if (gethostname(host, n) < 0) {
		log_sys("gethostname error");
	}

	//��ȡ�����ӡ����ַ�б�
	if ((err = getaddrlist(host, "print", &ailist)) != 0) {
		log_quit("getaddrinfo error: %s", gai_strerror(err));
		exit(1);
	}

	FD_ZERO(&rendezvous);

	maxfd = -1;
	for (aip = ailist; aip != NULL; aip = aip->ai_next){
		//���������socket
		if ((sockfd = initserver(SOCK_STREAM, aip->ai_addr, aip->ai_addrlen, QLEN)) >= 0) {
			FD_SET(sockfd, &rendezvous);//�����ļ�����������fd_set
			if (sockfd > maxfd) {
				maxfd = sockfd;
			}
		}
	}

	if (maxfd == -1) {
		log_quit("service not enabled");
	}

	pwdp = getpwnam("lp");
	if (pwdp == NULL) {
		log_sys("can't find user lp");
	}
	if (pwdp->pw_uid == 0) {
		log_quit("user lp is privileged");
	}

	if (setuid(pwdp->pw_uid) < 0) {
		log_sys("can't change IDs to user lp");
	}

	//����һ�������ӡ��ͨ�ŵ��߳�
	pthread_create(&tid, NULL, printer_thread, NULL);
	//����һ�������źŵ��߳�
	pthread_create(&tid, NULL, signal_thread, NULL);
	build_qonstart();

	log_msg("daemon initialized");

	//�������Կͻ��˵���������
	//ʵ�ַ�������Ӧ����ͻ��˵����ӣ��Ϳ���ʹ��select���������Ƿ������ģ����Բ�ѯ���ĸ��ͻ��˵���Ӧ��
	for (;;) {
		rset = rendezvous;
		//���������ӣ��ȴ�����һ���ļ���������Ϊ�ɶ�
		if (select(maxfd+1, &rset, NULL, NULL, NULL) < 0) {
			log_sys("select failed");
		}

		for (i = 0; i < maxfd; i ++) {
			//һ���ɶ����ļ�����������ζ��һ������������Ҫ����
			if (FD_ISSET(i, &rset)) {
				//ʹ��accept���ܸ���������
				sockfd = accept(i, NULL, NULL);
				if (sockfd < 0) {
					log_ret("accept failed");//���ʧ�ܣ����¼��־����������Ŀɶ��ļ�������
				}

				pthread_create(&tid, NULL, client_thread, (void*)sockfd);//����һ���߳�������ͻ��˵�����
			}
		}
	}
	
	//���߳�mainһֱѭ�����������͵������̴߳�����Զ���ﲻ��exit���
	exit(1);
}

