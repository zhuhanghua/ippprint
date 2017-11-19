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






