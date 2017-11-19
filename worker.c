
/**
 * ��һ��worker_thread�ṹ���뵽��߳��б��С�
 */
void add_worker(pthread_t tid, int sockfd) {
	//����ýṹ��Ҫ�ڴ�, ����ʼ��
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
 * �߳��������, ���ڿ�Ԥ�����߲���Ԥ���ĵ�ǰ�߳��˳�ʱִ��
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