

void init_request(void) {
	int n;
	char name[FILENMSZ];

	sprintf(name, "%s/%s", SPOOLDIR, JOBFILE);
	//初始化文件描述符
	jobfd = open(name, O_CREATE|O_RDWR, S_IRUSR|S_IWUSR);

	//放一个记录锁
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
	//创建tcp套接字
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);//tcp 协议: 面向连接
    if(sockfd < 0){
        perror("socket");
        return -1;
    }

	//设置地址为可重复使用
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == -1) {
		err = errno;
		goto errout;
	}
    
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);//设置本机地址
	serveraddr.sin_port = htons(8080);//将整型变量从主机字节顺序转变成网络字节顺序,网络字节顺序采用big-endian排序方式
	memset(serveraddr.sin_zero, 0, sizeof(serveraddr.sin_zero)); 
	
	//绑定套接字与地址信息
	if (bind(sockfd, (struct sockaddr_in*)&serveraddr, sizeof(serveraddr)) == -1) {
		err = errno;
		goto errout;
	}

	//监听，宣告可以接受连接请求
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
 * 当守护进程程序启动时，调用build_qonstart从存储在/var/spool/printer/reqs中的磁盘文件建立
 * 一个内存中的打印作业列表。如果不能打开该目录，表示没有打印作业要处理
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

		//读取保存在文件中的printreq结构
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
		//将文件名转化为作业ID
		jobid = atol(entp->d_name);
		log_msg("adding job %ld to queue", jobid);
		//将请求加入到挂起的打印作业列表 
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

	//初始化守护进程程序
	if (argc ！= 1) {
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

	//获取网络打印机地址列表
	if ((err = getaddrlist(host, "print", &ailist)) != 0) {
		log_quit("getaddrinfo error: %s", gai_strerror(err));
		exit(1);
	}

	FD_ZERO(&rendezvous);

	maxfd = -1;
	for (aip = ailist; aip != NULL; aip = aip->ai_next){
		//建立服务端socket
		if ((sockfd = initserver(SOCK_STREAM, aip->ai_addr, aip->ai_addrlen, QLEN)) >= 0) {
			FD_SET(sockfd, &rendezvous);//将其文件描述符加入fd_set
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

	//创建一个处理打印机通信的线程
	pthread_create(&tid, NULL, printer_thread, NULL);
	//创建一个处理信号的线程
	pthread_create(&tid, NULL, signal_thread, NULL);
	build_qonstart();

	log_msg("daemon initialized");

	//处理来自客户端的连接请求
	//实现服务器响应多个客户端的连接，就可以使用select函数，且是非阻塞的，可以查询是哪个客户端的响应。
	for (;;) {
		rset = rendezvous;
		//无阻塞连接，等待其中一个文件描述符变为可读
		if (select(maxfd+1, &rset, NULL, NULL, NULL) < 0) {
			log_sys("select failed");
		}

		for (i = 0; i < maxfd; i ++) {
			//一个可读的文件描述符就意味着一个连接请求需要处理
			if (FD_ISSET(i, &rset)) {
				//使用accept接受该连接请求
				sockfd = accept(i, NULL, NULL);
				if (sockfd < 0) {
					log_ret("accept failed");//如果失败，则记录日志继续检查更多的可读文件描述符
				}

				pthread_create(&tid, NULL, client_thread, (void*)sockfd);//创建一个线程来处理客户端的请求
			}
		}
	}
	
	//主线程main一直循环，将请求发送到其他线程处理，永远到达不了exit语句
	exit(1);
}

