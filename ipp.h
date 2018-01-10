/**
 * 包含IPP定义的头文件
 */
#ifndef _IPP_H_
#define _IPP_H_

#include <stdint.h>

#define STATCLASS_OK(x) ((x)>=0x0000 && (x)<=0x00ff)
#define STATCLASS_INFO(x) ((x) >= 0x0100 && (x) <= 0x01ff)
#define STATCLASS_REDIR(x) ((x) >= 0x0200 && (x) <= 0x02ff)
#define STATCLASS_CLIERR(x) ((x) >= 0x0400 && (x) <= 0x04ff)
#define STATCLASS_SRVERR(x) ((x) >= 0x0500 && (x) <= 0x05ff)

/*
 * Attribute Tags.
 */
#define TAG_OPERATION_ATTR   0x01	/* operation attributes tag */
#define TAG_JOB_ATTR         0x02	/* job attributes tag */
#define TAG_END_OF_ATTR      0x03	/* end of attributes tag */
#define TAG_PRINTER_ATTR     0x04	/* printer attributes tag */
#define TAG_UNSUPP_ATTR      0x05	/* unsupported attributes tag */

/**
 * Operation IDs 操作码
 */
#define OP_PRINT_JOB 0x02

/*
 * Value Tags.
 */
#define TAG_UNSUPPORTED      0x10	/* unsupported value */
#define TAG_UNKNOWN          0x12	/* unknown value */
#define TAG_NONE             0x13	/* no value */
#define TAG_INTEGER          0x21	/* integer */
#define TAG_BOOLEAN          0x22	/* boolean */
#define TAG_ENUM             0x23	/* enumeration */
#define TAG_OCTSTR           0x30	/* octetString */
#define TAG_DATETIME         0x31	/* dateTime */
#define TAG_RESOLUTION       0x32	/* resolution */
#define TAG_INTRANGE         0x33	/* rangeOfInteger */
#define TAG_TEXTWLANG        0x35	/* textWithLanguage */
#define TAG_NAMEWLANG        0x36	/* nameWithLanguage */
#define TAG_TEXTWOLANG       0x41	/* textWithoutLanguage */
#define TAG_NAMEWOLANG       0x42	/* nameWithoutLanguage */
#define TAG_KEYWORD          0x44	/* keyword */
#define TAG_URI              0x45	/* URI */
#define TAG_URISCHEME        0x46	/* uriScheme */
#define TAG_CHARSET          0x47	/* charset */
#define TAG_NATULANG         0x48	/* naturalLanguage */
#define TAG_MIMETYPE         0x49	/* mimeMediaType */


struct ipp_hdr{
	int8_t major_version;/*版本号*/
	int8_t minor_version;
	union {
		int16_t op;/*操作码*/
		int16_t st;/*状态码*/
	}u;

	int32_t request_id;//请求ID
	char attr_group[1];//定义结构体的最后一个标志位作为分界
};

struct ipvec{
	char* iov_base;
	int iov_len;
};

void setup_ipp_header(struct job *jp, struct ipvec iov[], char* ibuf);
void setup_http_header(struct job *jp, struct ipvec iov[], char* hbuf, struct stat* sbuf);



#endif
