#include "print.h"
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
	pthread_cond_signal(&jobwait);//�ź�����֪ͨ�ȴ���ӡ���߳�
}

/**
 * ���ڽ���ҵ���뵽�����б�ͷ��
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
 * ����ҵ�ӹ������ҵ�б���ɾ��
 * �����߱������joblock������
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