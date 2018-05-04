
#include "print.h"
#include "apue.h"
//#include "my_err.h"
//#include "my_log.h"
#include "ourhdr.h"

int log_to_stderr = 0;
extern int debug;

struct addrinfo *printer;//保存打印机的网络地址
char  *printer_name;//保存打印机的主机名字
pthread_mutex_t  configlock = PTHREAD_MUTEX_INITIALIZER;//用于保护对reread变量的访问
int  reread;

struct worker_thread *workers;
pthread_mutex_t  workerlock = PTHREAD_MUTEX_INITIALIZER;
sigset_t mask;
struct job  *jobhead, *jobtail;//工作队列
int  jobfd;	//jobfd是作业文件的文件描述符

long nextjob;
pthread_mutex_t  joblock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  jobwait = PTHREAD_COND_INITIALIZER;


void init_printer(void) {
	printer = get_printaddr();
	if (printer == NULL) {
		log_msg("no printer device registered");
		exit(-1);
	}

	printer_name = printer->ai_canonname;
	if (printer_name == NULL) {
		printer_name = (char *)"printer";
	}
	log_msg("printer is %s", printer_name);
}


