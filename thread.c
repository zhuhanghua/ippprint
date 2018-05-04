#include "print.h"
#include "ipp.h"
#include "apue.h"
//#include "my_err.h"
//#include "my_log.h"
#include "ourhdr.h"
//当连接请求被接受时，main线程中派生出client_thread, 
//其作用是从客户print命令中接收要打印的文件。
//为每个客户端打印请求分别建立一个独立的线程
extern int log_to_stderr;

extern struct addrinfo *printer;//保存打印机的网络地址
extern char  *printer_name;//保存打印机的主机名字
extern pthread_mutex_t  configlock;//用于保护对reread变量的访问
extern int  reread;

extern struct worker_thread *workers;
extern pthread_mutex_t  workerlock;
extern sigset_t mask;
extern struct job  *jobhead, *jobtail;
extern int  jobfd;	//jobfd是作业文件的文件描述符

extern long nextjob;
extern pthread_mutex_t  joblock;
extern pthread_cond_t  jobwait;


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

    sockfd = *(int*)arg;
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

        strncpy(res.msg, strerror(res.retcode), MSGLEN_MAX);//读取失败，返回错误码
        writen(sockfd, &res, sizeof(struct printresp));
        pthread_exit((void *)1);
    }

    req.size = ntohl(req.size);
    req.flags = ntohl(req.flags);

    jobid = get_newjobno();
    sprintf(name, "%s/%s/%ld", SPOOLDIR, DATADIR, jobid);

    if ((fd = creat(name, FILEPERM)) < 0) {
    }

    /**
     * 读取并存储数据文件到spool目录
    */
    first = 1;
    while((nr = tread(sockfd, buf, IOBUFSZ, 20)) > 0) {
        if (first) {
            first = 0;
            if (strncmp(buf, "%!PS", 4) != 0) {
                req.flags |= PR_TEXT;
            }
        }

        nw = write(fd, buf, nr);
        if (nw != nr) {
            if (nw < 0) {
                res.retcode = htonl(errno);
            }else {
                res.retcode = htonl(EIO);
            }
        }
    }
    close(fd);

    /**
     * 创建控制文件，记住打印请求
     */
    sprintf(name, "%s/%s/%ld", SPOOLDIR, REQDIR, jobid);
    fd = creat(name, FILEPERM);
    if (fd < 0) {
    }

    nw = write(fd, &req, sizeof(struct printreq));
    if (nw != sizeof(struct printreq)) {
    }

    close(fd);

    /**
     * 发送响应给客户端
     */
    res.retcode = 0;
    res.jobid = htonl(jobid);
    sprintf(res.msg, strerror(res.retcode), sizeof(struct printresp));
    write(sockfd, &res, sizeof(struct printresp));

    /**
     * 唤醒打印线程，清理，并退出
     */
     log_msg("adding job %ld to queue", jobid);
     add_job(&req, jobid);
     pthread_cleanup_pop(1);
     return ((void*)0);
}

/**
 * 与网络打印机通信的线程运行
 */
void* printer_thread(void *arg) {
    struct job *jp;
    int hlen, ilen, sockfd, fd, nr, nw;
    struct stat sbuf;
	struct ipvec iov[2];
	char ibuf[IBUFSZ];
	char hbuf[HBUFSZ];
    char name[FILENMSZ];
    char buf[IOBUFSZ];

    for (;;) {
        /**
        * Get a job to print
        */
        pthread_mutex_lock(&joblock);
        while(jobhead == NULL){
            log_msg("printer thread wating...");
            pthread_cond_wait(&jobwait, &joblock);
        }
        remove_job(jp = jobhead);
        log_msg("printer_thread: picked up job %ld", jp->jobid);
        pthread_mutex_unlock(&joblock);

        update_jobno();

        pthread_mutex_lock(&configlock);
        if (reread) {
			freeaddrinfo(printer);
			printer = NULL;
			printer_name = NULL;
			reread = 0;
			pthread_mutex_lock(&configlock);
			init_printer();
        }else{
        	pthread_mutex_unlock(&configlock);
		}

		sprintf(name, "%s/%s/%ld", SPOOLDIR, DATADIR, jp->jobid);
		if ((fd = open(name, O_RDONLY)) < 0) {
			log_msg("job %ld canceled - can't open %s: %s", 
			jp->jobid, name, strerror(errno));
			free(jp);
			continue;
		}

		if ((fstat(fd, &sbuf)) < 0) {
			log_msg("job %ld canceled - can't fstat %s %s",
			jp->jobid, name, strerror(errno));
			free(jp);
			close(fd);
			continue;
		}
		/*SOCK_STREAM 流服务(TCP)， SOCK_UGRAM 数据报服务（UDP）*/
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			log_msg("job %ld deferred - can't create socket: %s",
			jp->jobid, strerror(errno));
			goto defer;
		}

		log_msg("send file to printer for print");
		
		/**
		 * Set up the IPP header
		 */
		setup_ipp_header(jp, iov, ibuf);
		/**
		 * Set up the HTTP header
		 */
		setup_http_header(jp, iov, hbuf, &sbuf);

		hlen = iov[0].iov_len;
		ilen = iov[1].iov_len;

		if ((nw = writev(sockfd, (const struct iovec*)iov, 2)) != hlen + ilen) {
			log_ret("can't write to printer");
			goto defer;
		}

		//读取数据文件并发送给打印机
		while((nr = read(fd, buf, IOBUFSZ)) > 0) {
			if ((nw = write(sockfd, buf, nr)) != nr) {
				if (nw < 0) {
					log_ret("can't write to printer");
				}else {
					log_msg("short write (%d/%d) to printer", nw, nr);
				}
				goto defer;
			}
		}

		if (nr < 0) {
			log_ret("can't read %s", name);
			goto defer;
		}

		/**
		 * 获取打印机响应，如果成功，则删除文件，释放jp对应的资源
		 */
		if (printer_status(sockfd, jp)){
			unlink(name);
			sprintf(name, "%s/%s/%ld", SPOOLDIR, REQDIR, jp->jobid);
			unlink(name);
			free(jp);
			jp = NULL;
		}
    }

 defer:
 	close(fd);
 	if (sockfd >= 0) {
		close(sockfd);
 	}

 	if (jp != NULL) {
		replace_job(jp);
		sleep(60);
 	}
}

void * signal_thread(void *arg) {
    int err, signo;
    for (;;) {
    	err = sigwait(&mask, &signo);
    	if (err != 0) {
			log_quit("sigwait failed: %s", strerror(err));
    	}

		switch (signo){
		case SIGHUP://重新刷新配置文件
			pthread_mutex_lock(&configlock);
			reread = 1;
			pthread_mutex_unlock(&configlock);
			break;
		case SIGTERM:
			kill_workers();
			log_msg("terminate with signal %s", strsignal(signo));
			exit(0);
		default:
			kill_workers();
			log_quit("unexpected signal %d", signo);
		}
    }
}
