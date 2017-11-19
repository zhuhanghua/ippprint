#ifndef _IPP_H_
#define _IPP_H_

#define STATCLASS_OK(x) ((x) >= 0x0000 && (x) <= 00ff)
#define STATCLASS_INFO(x) ((x) >= 0x0100 && (x) <= 01ff)
#define STATCLASS_REDIR(x) ((x) >= 0x0200 && (x) <= 02ff)
#define STATCLASS_CLIERR(x) ((x) >= 0x0400 && (x) <= 04ff)
#define STATCLASS_SRVERR(x) ((x) >= 0x500 && (x) <= 05ff)

#endif
