#include "print.h"
#include "apue.h"
//#include "my_err.h"
//#include "my_log.h"

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

/**
 * 将一个worker_thread结构加入到活动线程列表中。
 */
void add_worker(pthread_t tid, int sockfd) {
	//分配该结构需要内存, 并初始化
	struct worker_thread *wtp;
	if ((wtp = (struct worker_thread *)malloc(sizeof(struct worker_thread))) == NULL) {
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

void kill_workers(void) {
	struct worker_thread *wtp;
	pthread_mutex_lock(&workerlock);

	for(wtp = workers; wtp != NULL; wtp = wtp->next) {
		pthread_cancel(wtp->tid);
	}

	pthread_mutex_unlock(&workerlock);
}

 /**
 * 线程清理程序, 用于可预见或者不可预见的当前线程退出时执行
 */
void client_cleanup(void * arg) {
    struct worker_thread *wtp;
    pthread_t tid;

    tid = (pthread_t)arg;
    pthread_mutex_lock(&workerlock);
    for (wtp = workers; wtp != NULL; wtp = wtp->next) {
        if (wtp->tid == tid) {
            if (wtp->next != NULL) {
                wtp->next->prev = wtp->prev;
            }

            if (wtp->prev != NULL) {
                wtp->prev->next = wtp->next;
            }else {
                workers = wtp->next;
            }

            break;
        }
    }

    pthread_mutex_unlock(&workerlock);
    if (wtp != NULL) {
        close(wtp->sockfd);
        free(wtp);
    }
}