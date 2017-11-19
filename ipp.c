#include "apue.h"
#include "print.h"
#include "ipp.h"

#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <pthread.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/uio.h>

#define HTTP_INFO(x)  ((x)>=100 && (x) <= 199)
#define HTTP_SUCCESS(x)  ((x)>=200 && (x) <= 299)



/**
 * 添加一个选项到IPP 头部
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

void setup_ipp_header(struct job *jp, struct ipvec** iov, char* ibuf){
    char *icp;
    struct ipp_hdr *hp;
   
    char str[64];

	icp = ibuf;
	hp = (struct ipp_hdr *)icp;
	hp->major_version = 1;
	hp->minor_version = 1;
	hp->operation = htons(OP_PRINT_JOB);
	hp->request_id = htonl(jp->jobid);
	icp += offsetof(struct ipp_hdr, attr_group);
	*icp++ = TAG_OPERATION_ATTR;
	icp = add_option(icp, TAG_CHARSET, "attributes-charset", "utf-8");
	icp = add_option(icp, TAG_NATULANG, "attributes-natural-language", "en-us");
	sprintf(str, "http://%s:%d", printer_name, IPP_PORT);
	icp = add_option(icp, TAG_URI, "printer-uri", str);

	*icp ++ = TAG_END_OF_ATTR;
	iov[1].iov_base = ibuf;
	iov[1].iov_len = icp - ibuf;
}

void setup_http_header(struct job *jp, struct ipvec** iov, char* hbuf, struct stat* sbuf) {
	char *hcp;
	hcp = hbuf;

	sprintf(hcp, "POST /%s/ipp HTTP/1.1 \r\n", printer_name);
	hcp += strlen(hcp);
	sprintf(hcp, "Content-Lenght: %ld\r\n", (long)sbuf.st_size + iov[1].ilen);
	hcp += strlen(hcp);
	sprintf(hcp, "Host: %s:%d\r\n", printer_name, IPP_PORT);
	hcp += strlen(hcp);
	*hcp ++ = '\r';
	*hcp ++ = '\n';
	iov[0].iov_base = hbuf;
	iov[0].iov_len = hcp - hbuf;
}

ssize_t readmore(int sockfd, char * *bpp, int off, int *bszp){
	ssize_t nr;
	char *bp = *bpp;
	int bsz = *bszp;

	if (off >= bsz) {
		bsz += IOBUFSZ;
		if ((bp = realloc(*bpp, bsz)) == NULL) {
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
	ssize_t nr;
	char *statcode, *reason, *cp, *contentlen;
	struct ipp_hdr *hp;
	char *bp;

	success = 0;
	bufsz = IOBUFSZ;
	if ((bp = malloc(IOBUFSZ)) == NULL) {
		log_sys("printer_status: can't allocate read buffer");
	}

	/**
	 * 分配一个缓冲区并读取来自打印机的数据，期望5秒之内有可用的响应
	 */
	while((nr = tread(sockfd, bp, IOBUFSZ, 5)) > 0){
	    /**
	     *跳过http 1.1
	     */
		cp = bp + 8;
		while(isspace((int)*cp) {
			cp ++;
		}
		
		statcode = cp;
		while(isdigit((int)*cp)) {
			cp ++;
		}

		if (cp == statcode) {
			log_msg(bp);
		}else {
			*cp ++ = '\0';
			reason = cp;
			while(*cp != '\r' && *cp != '\0') {
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
			
			i = cp - bp;
			for (;;){
				/**
				  * 搜索Content-Length 属性: 先检查字符是否为‘C’或者‘c’.如果是，则与Content-Lenght比较
				  */
				while(*cp != 'C' && *cp != 'c' && i < nr) {
					cp ++;
					i ++;
				}

				if (i > nr && ((nr = readmore(sockfd, &bp, i, &bufsz)) < 0)) {
					goto out;
				}

				cp = &bp[i];
				if (strncasecmp(cp, "Content-Length:", 15) == 0) {
					cp += 15;
					while(isspace((int)*cp)) {
						cp ++;
					}
					contentlen = cp;
					while(isdigit((int)*cp)) {
						cp ++;
					}
					cp ++ = '\0';
					i = cp - bp;
					len = atoi(contentlen);
					break;
				}else{
					cp ++;
					i ++;
				}
			}
		}
	}
out:
	free(bp);
	if (nr < 0) {
		log_msg("jobid %ld: error reading printer response: %s", jobid, strerror(error));
	}

	return (success);
}
