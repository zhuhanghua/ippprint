#include "print.h"
#include "print.c"
#include "apue.h"

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