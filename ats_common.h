#ifndef ATS_COMMON
#define ATS_COMMON
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ts/ts.h"
#include "ts/ink_defs.h"

#define TS_NULL_MUTEX NULL
#define PLUGIN_NAME "url_analysis"
#define DebugLK(fmt, ...) do {\
	fprintf(stdout, "%s\t", PLUGIN_NAME);\
	fprintf(stdout, fmt, ##__VA_ARGS__);\
	fprintf(stdout, "%c", '\n');\
	}while(0);

#define FUNC_ENTERING DebugLK("Entering %s()", __func__)

TSReturnCode LKCliReqMimeHdrFieldGet(TSHttpTxn txnp, char *name, int name_length, char* **value, int *value_len);
TSReturnCode LKSerRspMimeHdrFieldGet(TSHttpTxn txnp, char *name, int name_length, char* **value, int *value_len);
TSReturnCode LKCliReqHdrMethodGet(TSHttpTxn txnp, char **method);
TSReturnCode LKCliReqHdrHostAndPathGet(TSHttpTxn txnp, char **url, char **path);

char *LKGetUrlHost(char *url);
char *LKGetParamUrl(char *url);
int LKGetUrlType(char* url,int url_len,char* url_type);

#endif
