#include "apue.h"
#include "print.h"

#include <cytpe.h>
#include <sys/select.h>

#define MAXCFGLINE 512
#define MAXKWLEN 16
#define MAXFMTLEN 16


int getaddrlist(const char *host, const char *service, struct addrinfo **ailistpp) {
	int err;
	struct addrinfo hint;

	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = 0;
	hint.ai_addrlen = 0;
	hint.ai_canonname = NULL;
	hint.ai_addr = NULL;
	hint.ai_next = NULL;
	err = getaddrinfo(host, service, &hint, ailistpp);
	return (err);
}

/**
 * �ڴ�ӡ�����ļ�������ָ���Ĺؼ���
 */
static char *
scan_configfile(char* keyword) {
	int n, match;
	FILE *fp;
	char keybuf[MAXFMTLEN], pattern[MAXFMTLEN];
	char line[MAXCFGLINE];
	static char valbuf[MAXCFGLINE];

	if ((fp = fopen(CONFIG_FILE, "r")) == NULL) {
		log_sys("can't open %s", CONFIG_FILE);
	}

	sprintf(pattern, "%%%ds %%%ds", MAXKWLEN-1, MAXCFGLINE-1);

	match = 0;

	while(fgets(line, MAXCFGLINE, fp) != NULL) {
		n = sscanf(line, pattern, keybuf, valbuf);
		if (n == 2 && strcmp(keyword, keybuf) == 0) {
			match = 1;
			break;
		}
	}

	fclose(fp);

	if (match != 0) {
		return (valbuf);
	}else{
		return NULL;
	}
}

/** 
 * �ҵ����д�ӡ���ѻ��ػ����̵ļ����ϵͳ������
*/
char* get_printserver(void) {
	return (scan_configfile("printserver"));
}

/**
 * ��ȡ�����ӡ���ĵ�ַ
*/
struct addrinfo* get_printaddr(void) {
	int err;
	char *p;
	struct addrinfo *ailist;

	if ((p = scan_configfile("printer")) != NULL) {
		if ((err = getaddrlist(p, "ipp", ailist)) != 0) {
			log_msg("no address information for %s", p);
			return (NULL);
		}

		return (ailist);
	}

	log_msg("no printer address specified");
	return NULL;
}

/**
 ��ȡָ�����ֽ������ڷ�����ǰ��������timeout��
 */
ssize_t tread(int fd, void *buf, size_t nbytes, unsigned int timout){
	int nfds;
	fd_set readfds;

	struct timeval tv;
	tv.tv_sec = timout;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	/*ʹ��select ����ȷ��һ�����߶���׽��ֵ�״̬���ȴ�ָ�����ļ��������ɶ�*/
	nfds = select(fd+1, &readfds, NULL, NULL, &tv);
	if (nfds <= 0) {
		if (nfds == 0){
			errno = ETIME;//��ʱ
		}

		return -1;
	}

	return (read(fd, buf, nbytes));
}

/**
 *�ṩtread�ı���, ����treadn, ��ֻ���ö�ȡ������ֽ���
 */
ssize_t
treadn(int fd, void *buf, size_t nbytes, unsigned int timeout) {
	size_t nleft;
	ssize_t nread;

	nleft = nbytes;
	while (nleft > 0) {
		if (nread = tread(fd, buf, nleft, timeout)) < 0) {
			if (nleft == nbytes) {
				return -1;
			}else{
				break;
			}
		}else if (nread == 0) {
			break;
		}

		nleft -= nread;
		buf += nread;
	}

	return (nbytes - nleft);
}

