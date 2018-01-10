#include "print.h"
//#include "print.c"
#include "apue.h"
#include "ourhdr.h"
//#include "my_err.h"
//#include "my_log.h"

extern int log_to_stderr;
extern int debug;
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


void update_jobno(void) {
	char buf[32];

	lseek(jobfd, 0, SEEK_SET);
	sprintf(buf, "%ld", nextjob);
	if (write(jobfd, buf, strlen(buf)) <0) {
		log_sys("can't update job file");
	}
}

long get_newjobno(void) {
	long jobid;

	pthread_mutex_lock(&joblock);
	jobid = nextjob ++;
	if (nextjob <= 0) {
		nextjob = 1;
	}

	pthread_mutex_unlock(&joblock);

	return jobid;
}

void add_job(struct printreq *reqp, long jobid) {
	struct job *jp;
	if ((jp = (struct job*)malloc(sizeof(struct job))) == NULL) {
		log_sys("malloc failed");
	}
	memcpy(&jp->req, reqp, sizeof(struct printreq));
	jp->jobid = jobid;
	jp->next = NULL;
	pthread_mutex_lock(&joblock);
	jp->prev = jobtail;
	if (jobtail == NULL) {
		jobhead = jp;
	}else{
		jobtail->next = jp;
	}

	jobtail = jp;
	pthread_mutex_unlock(&joblock);
	pthread_cond_signal(&jobwait);//淇″彿閿侊紝閫氱煡绛夊緟鎵撳嵃鐨勭嚎锟?
}

/**
 * 鐢ㄤ簬灏嗕綔涓氭彃鍏ュ埌鎸傝捣鍒楄〃澶撮儴
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
 * 灏嗕綔涓氫粠鎸傝捣鐨勪綔涓氬垪琛ㄤ腑鍒犻櫎
 * 璋冪敤鑰呭繀椤绘寔鏈塲oblock浜掓枼锟?
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