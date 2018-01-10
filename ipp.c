//#include "apue.h"
#include "print.h"
//#include "print.c"
#include "ipp.h"
#include "apue.h"
//#include "my_err.h"
//#include "my_log.h"
#include "ourhdr.h"
#include <sys/select.h>
#include <sys/uio.h>
#include <arpa/inet.h>

extern 
char  *printer_name;//�����ӡ������������

#define HTTP_INFO(x)  ((x)>=100 && (x) <= 199)
#define HTTP_SUCCESS(x)  ((x)>=200 && (x) <= 299)

/**
 * ���һ��ѡ�IPP ͷ��
 */
char* add_option(char* cp, int tag, char* optname, char *optval) {
	int n;
	union {
		int16_t s;
		char c[2];
	}u;

	*cp ++ = tag;
	n = strlen(optname);
	u.s = htons(n);
	*cp ++= u.c[0];
	*cp ++ = u.c[1];
	strcpy(cp, optname);
	cp += n;
	n = strlen(optval);
	u.s = htons(n);
	*cp ++ = u.c[0];
	*cp ++ = u.c[1];
	strcpy(cp, optval);
	return (cp + n);
}

void setup_ipp_header(struct job *jp, struct ipvec iov[], char* ibuf){
    char *icp;
    struct ipp_hdr *hp;
   
    char str[64];

	icp = ibuf;
	hp = (struct ipp_hdr *)icp;
	hp->major_version = 1;/*1.1�汾Э��*/
	hp->minor_version = 1;
	hp->u.op = htons(OP_PRINT_JOB);
	hp->request_id = htonl(jp->jobid);
	icp += offsetof(struct ipp_hdr, attr_group);
	*icp++ = TAG_OPERATION_ATTR;
	icp = add_option(icp, TAG_CHARSET, (char *)"attributes-charset", (char *)"utf-8");
	icp = add_option(icp, TAG_NATULANG, (char *)"attributes-natural-language", (char *)"en-us");
	sprintf(str, "http://%s:%d", printer_name, IPP_PORT);
	icp = add_option(icp, TAG_URI, (char *)"printer-uri", str);/*��ӡ����ͳһ��Դ��ʶ��*/

	*icp ++ = TAG_END_OF_ATTR;
	iov[1].iov_base = ibuf;
	iov[1].iov_len = icp - ibuf;
}

void setup_http_header(struct job *jp, struct ipvec iov[], char* hbuf, struct stat* sbuf) {
	char *hcp;
	hcp = hbuf;

	sprintf(hcp, "POST /%s/ipp HTTP/1.1 \r\n", printer_name);
	hcp += strlen(hcp);
	sprintf(hcp, "Content-Length: %ld\r\n", (long)sbuf->st_size + iov[1].iov_len);
	hcp += strlen(hcp);
	strcpy(hcp, "Content-Type: application/ipp\r\n");
	hcp += strlen(hcp);
	sprintf(hcp, "Host: %s:%d\r\n", printer_name, IPP_PORT);
	hcp += strlen(hcp);
	*hcp ++ = '\r';
	*hcp ++ = '\n';
	iov[0].iov_base = hbuf;
	iov[0].iov_len = hcp - hbuf;
}

ssize_t readmore(int sockfd, char **bpp, int off, int *bszp){
	ssize_t nr;
	char *bp = *bpp;
	int bsz = *bszp;

	if (off >= bsz) {
		bsz = bsz + IOBUFSZ;
		if ((bp = (char*)realloc(*bpp, bsz)) == NULL) {
			log_sys("readmore: can't allocate bigger read buffer");
		}

		*bszp = bsz;
		*bpp = bp;
	}

	if ((nr = tread(sockfd, &bp[off], bsz - off, 1)) > 0) {
		return (off + nr);
	}else {
		return (-1);
	}
}


int printer_status(int sockfd, struct job * jp){
	int i, success, code, len, found, bufsz;
	long jobid;
	ssize_t nr;//��ȡ��ӡ�����ݻ������󷵻صĴ�С
	char *statcode, *reason, *contentlen;
	struct ipp_hdr *hp;
	char *bp;//Ŀ�껺�����׵�ַ
	char *cp;//Ŀ�껺������λָ��

	success = 0;
	bufsz = IOBUFSZ;
	if ((bp = (char *)malloc(IOBUFSZ)) == NULL) {
		log_sys("printer_status: can't allocate read buffer");
	}

	/**
	 * ����һ������������ȡ���Դ�ӡ�������ݣ�����5��֮���п��õ���Ӧ
	 */
	while((nr = tread(sockfd, bp, IOBUFSZ, 5)) > 0){
	    /**
	     *�����ַ���"http 1.1"
	     */
		cp = bp + 8;
		while(isspace((int)*cp)) {
			cp ++;
		}

		/** 
		 * ��ȡ״̬��
		 */
		statcode = cp;
		while(isdigit((int)*cp)) {
			cp ++;
		}
		
		if (cp == statcode) {
			/* ���Ӧ�������ֵ�״̬�룬������ǣ�����־�м�¼���ĵ����� */
			log_msg(bp);
		}else {
			*cp ++ = '\0';
			reason = cp; /* ��¼�������Ϣԭ�� */
			while(*cp != '\r' && *cp != '\n') {
				cp ++;
			}
			*cp = '\0';
			code = atoi(statcode);
			if (HTTP_INFO(code)) {
				continue;
			}

			if (!HTTP_SUCCESS(code)) {
				bp[nr] = '\0';
				log_msg("error: %s", reason);
				break;
			}

			/**
			 * ����Content-Length ����: �ȼ���ַ��Ƿ�Ϊ��C�����ߡ�c��.����ǣ�����Content-Lenght�Ƚ�
		     */			
			i = cp - bp;
			for (;;){
				while(*cp != 'C' && *cp != 'c' && i < nr) {
					cp ++;
					i ++;
				}

				if (i > nr && ((nr = readmore(sockfd, &bp, i, &bufsz)) < 0)) {
					goto out;
				}

				cp = &bp[i];
				if (strncasecmp(cp, (char *)"Content-Length:", 15) == 0) {
					cp += 15;
					while(isspace((int)*cp)) {
						cp ++;
					}
					contentlen = cp;
					while(isdigit((int)*cp)) {
						cp ++;
					}
					*cp ++ = '\0';
					i = cp - bp;
					len = atoi(contentlen);
					break;
				}else{
					cp ++;
					i ++;
				}
			}

			if (i >= nr && (nr = readmore(sockfd, &bp, i, &bufsz)) < 0) {
				goto out;
			}

			cp = &bp[i];

			//����HTTP�ײ��������ֵ�һ���հ���
			found = 0;
			while (!found) {
				while(i < nr -2) {
					if (*cp == '\n' && *(cp + 1) == '\r' && *(cp + 2) == '\n') {
						found = 1;
						cp += 3;
						i += 3;
						break;
					}
					cp ++;
					i ++;
				}
				if (i >= nr && (nr = readmore(sockfd, &bp, i, &bufsz))){
					goto out;
				}
				cp = &bp[i];
			}

			//��������HTTP�ײ��Ľ�β
			if (nr - i < len && ((nr = readmore(sockfd, &bp, i, &bufsz)) < 0)) {
				goto out;
			}

			hp = (struct ipp_hdr*)cp;
			i = ntohs(hp->u.st);
			jobid = htonl(hp->request_id);
			if (jobid != jp->jobid) {
				log_msg("jobid %ld status code %d", jobid, i);
				break;
			}

			if (STATCLASS_OK(i)) {
				success = -1;
			}
			break;
		}
	}
	
out:
	free(bp);
	if (nr < 0) {
		log_msg("jobid %ld: error reading printer response: %s", 
			jobid, strerror(errno));
	}

	return (success);
}
