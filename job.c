#include "print.h"
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


/**
 * 将一个worker_thread结构加入到活动线程列表中。
 */
void add_worker(pthread_t tid, int sockfd) {
	//分配该结构需要内存, 并初始化
	struct worker_thread *wtp;
	if ((wtp = malloc(sizeof(struct worker_thread))) == NULL) {
		pthread_exit((void *)1);
	}
	wtp->tid = tid;
	wtp->sockfd = sockfd;

	pthread_mutex_lock(&workerlock);
	wtp->prev = NULL;
	wtp->next = workers;
	if (workers == NULL) {
		workers = wtp;
	}else {
		workers->prev = wtp;
	}

	pthread_mutex_unlock(&workerlock);
}

void add_job(struct printreq *reqp, long jobid) {
	struct job *jp;
	if ((jp = malloc(sizeof(struct job))) == NULL) {
		log_sys("malloc failed");
	}
	memcpy(&jp->req, reqp, sizeof(struct printreq));
	jp->jobid = jobid;
	jp->next = NULL;
	phtread_mutex_lock(&joblock);
	jp->prev = jobtail;
	if (jobtail == NULL) {
		jobhead = jp;
	}else{
		jobtail->next = jp;
	}

	jobtail = jp;
	pthread_mutex_unlock(&joblock);
	pthread_cond_signal(&jobwait);//信号锁，通知等待打印的线程
}

/**
 * 用于将作业插入到挂起列表头部
 */
void replace_job(struct job* jp) {
	pthread_mutex_lock(&joblock);
	
	jp->prev = NULL;
	jp->next = jobhead;
	if (jobhead == NULL) {
		jobtail = jp;
	} else {
		jobhead->prev = jp;
	}

	jobhead = jp;
	pthread_mutex_unlock(&joblock);
}

/**
 * 将作业从挂起的作业列表中删除
 * 调用者必须持有joblock互斥量
 */
void remove_job(struct job *target) {
	if (target->next != NULL) {
		target->next->prev = target->prev;
	}else {
		jobtail = target->prev;
	}

	if (target->prev != NULL) {
		target->prev->next = target->next;
	}else {
		jobhead = target->next;
	}
}