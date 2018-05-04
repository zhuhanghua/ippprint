#ifndef _PRINT_H_
#define _PRINT_H_

#include <sys/socket.h>
#include <arpa/inet.h>
#if defined(BSD) || defined(MACOS)
#include <netinet/in.h>
#endif

#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>


#define CONFIG_FILE "etc/printer.conf"
#define SPOOLDIR "/var/spool/printer"
#define JOBFILE "jobno"
#define DATADIR "data"
#define REQDIR "reqs"

#define FILENMSZ 64

#define USERNM_MAX 64
#define JOBNM_MAX 256
#define MSGLEN_MAX 512

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

#define IPP_PORT 631
#define QLEN 10
#define IBUFSZ 512     /* IPP 头部大小 */
#define HBUFSZ 512     /* HTTP 头部大小 */
#define IOBUFSZ 8192  /* 传输数据缓冲区大小 */

#define FILEPERM (S_IRUSR|S_IWUSR)

extern int getaddrlist(const char *, const char *, struct addrinfo **);
extern char* get_printserver(void);
extern struct addrinfo *get_printaddr(void);
extern ssize_t tread(int, void *, size_t, unsigned int);
extern ssize_t treadn(int, void*, size_t, unsigned int);
extern int connect_retry(int, const struct sockaddr*,  socklen_t);
extern int initserver();
extern ssize_t writen(int fd, const void *buf, size_t num);
extern ssize_t readn(int fd, void *buf, size_t num);

struct printreq {
	long size;
	long flags;
	char usernm[USERNM_MAX];
	char jobjnm[JOBNM_MAX];
};

#define PR_TEXT 0x01 //纯文本格式

struct printresp {
	long retcode;
	long jobid;
	char msg[MSGLEN_MAX];
};

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



#endif
