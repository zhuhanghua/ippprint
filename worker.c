#include "print.h"
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