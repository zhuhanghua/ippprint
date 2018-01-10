#ifndef __my_log_h
#define __my_log_h
#include <errno.h>  
#include <stdarg.h>  
#include <syslog.h>  
#include "ourhdr.h"  

static void log_doit(int, int, const char*, va_list ap);

int debug;

void log_open(const char *ident, int option, int facility){
	if (debug == 0)
		openlog(ident, option, facility);
}

void log_ret(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	log_doit(1, LOG_ERR, fmt, ap);
	va_end(ap);
	return;
}

void log_sys(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	log_doit(1, LOG_ERR, fmt, ap);
	va_end(ap);
	exit(2);
}

void log_msg(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	log_doit(0, LOG_ERR, fmt, ap);
	va_end(ap);
	return;
}

void log_quit(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	log_doit(0, LOG_ERR, fmt, ap);
	va_end(ap);
	exit(2);
}

static void log_doit(int errnoflag, int priority, const char *fmt, va_list ap){
	int errno_save;
	char buf[MAXLINE];

	errno_save = errno;
	vsprintf(buf, fmt, ap);
	if (errnoflag)
		sprintf(buf + strlen(buf), ": %s", strerror(errno_save));
	strcat(buf, "\n");
	if (debug){
		fflush(stdout);
		fputs(buf, stderr);
		fflush(stderr);
		
	}
	else
		syslog(priority, buf);
	return;
}

#endif