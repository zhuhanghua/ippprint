#include "print.h"
#include "ipp.h"

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

        strncpy(res.msg, strerror(res.retcode), MSGLEN_MAX);//��ȡʧ�ܣ����ش�����
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
     * ��ȡ���洢�����ļ���spoolĿ¼
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
     * ���������ļ�����ס��ӡ����
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
     * ������Ӧ���ͻ���
     */
    res.retcode = 0;
    res.jobid = htonl(jobid);
    sprintf(res.msg, &res, sizeof(struct printreq));
    write(sockfd, &res, sizeof(struct printresp));

    /**
     * ���Ѵ�ӡ�̣߳��������˳�
     */
     log_msg("adding job %ld to queue", jobid);
     add_job(&req, jobid);
     pthread_cleanup_pop(1);
     return ((void*)0);
}

/**
 * �������ӡ��ͨ�ŵ��߳�����
 */
void printer_thread(void *arg) {
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
			printer_mutex_unlock(&configlock);
			init_printer();
        }else{
        	pthread_mutex_unlock(&configlock);
		}

		sprintf(name, "%s/%s/%ld", SPOOLDIR, DATADIR, jp->jobid);
		if ((fd = open(name, O_RDONLY)) < 0) {
			log_msg("job %ld canceled - can't open %s: %s", 
			job->jobid, name, strerror(errno));
			free(jp);
			continue;
		}

		if ((fstat(fd, &sbuf)) < 0) {
			log_msg("job %ld canceled - can't fstat %s %s",
			job->jobid, name, strerror(errno));
			free(jp);
			close(fd);
			continue;
		}

		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			log_msg("job %ld deferred - can't create socket: %s",
			jp->jobid, strerror(errno));
			goto defer;
		}

		log_msg("send file to printer for print");
		setup_ipp_header(jp, iov, ibuf);
		setup_http_header(jp, iov, hbuf, sbuf);

		if ((nw = writev(sockfd, iov, 2)) != hlen + ilen) {
			log_ret("can't write to printer");
			goto defer;
		}

		//��ȡ�����ļ������͸���ӡ��
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
		 * ��ȡ��ӡ����Ӧ������ɹ�����ɾ���ļ����ͷ�jp
		 */
    }

 defer:
 	closed(fd);
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
		case SIGHUP://����ˢ�������ļ�
			phtread_mutex_lock(&configlock);
			reread = 1;
			phtread_mutex_unlock(&configlock);
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
