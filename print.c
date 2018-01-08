#include <pthread.h>
#include "print.h"

struct job {
	struct job *next;
	struct job *prev;
	long  jobid;
	struct printreq req;
};

struct worker_thread {
	struct worker_thread *next;
	struct worker_thread *prev;
	pthread_t  tid;
	int  sockfd;
};

int log_to_stderr = 0;

struct addrinfo *printer;//保存打印机的网络地址
char  *printer_name;//保存打印机的主机名字
pthread_mutex_t  configlock = PTHREAD_MUTEX_INITIALIZER;//用于保护对reread变量的访问
int  reread;

struct worker_thread *workers;
pthread_mutex_t  workerlock = PTHREAD_MUTEX_INITIALIZER;
sigset_t mask;
struct job  *jobhead, *jobtail;
int  jobfd;	//jobfd是作业文件的文件描述符

long nextjob;
pthread_mutex_t  joblock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  jobwait = PTHREAD_COND_INITIALIZER;

void init_request(void);
void init_printer(void);
void update_jobno(void);
long get_newjobno(void);
void add_job(struct printreq *, long);
void replace_job(struct job *);
void remove_job(struct job *);
void build_qonstart(void);
void *client_thread(void *);
void *printer_thread(void *);

void *signal_thread(void *);

void add_worker(pthread_t, int);
void kill_workers(void);
void client_cleanup(void *);

ssize_t readmore(int, char**, int, int *);
int printer_status(int, struct job*);


