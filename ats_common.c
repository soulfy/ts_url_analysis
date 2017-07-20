#include "ats_common.h"
static int LKHdrFieldValue(TSMBuffer bufp, TSMLoc hdr_loc, char *name, int name_length, char* **value, int *value_count)
{
	TSMLoc field_loc;
	int value_len;
	field_loc = TSMimeHdrFieldFind(bufp, hdr_loc, name , name_length);
	if(field_loc != TS_NULL_MLOC){
		*value_count = TSMimeHdrFieldValuesCount(bufp, hdr_loc, field_loc);
		DebugLK(">>>find field:%s value count:%d", name, *value_count);
		*value = (char **)malloc(*value_count * sizeof(char*));
		DebugLK(">>>malloc str pointer array");
		for(int i=0; i<*value_count; ++i){
			*value[i] = (char *)TSMimeHdrFieldValueStringGet(bufp, hdr_loc, field_loc, i, &value_len);
			if(!*value[i] || !value_len){
				return TS_ERROR;
			}
			DebugLK(">>>%s>>>>>>>>>>%s>>>>>%d\n", name, *value[i], value_len);
		}
		return TS_SUCCESS;
	}
	
	DebugLK("!!!NOT FOUND FIELD %s!!!!!!!!!!!\n", name);
	return TS_ERROR;
}

TSReturnCode LKCliReqMimeHdrFieldGet(TSHttpTxn txnp, char *name, int name_length, char* **value, int *value_count)
{
	TSMBuffer bufp;
	TSMLoc hdr_loc;

	if(TSHttpTxnClientReqGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS){
		return TS_ERROR;
	}

	return LKHdrFieldValue(bufp, hdr_loc, name, name_length, value, value_count);
}

TSReturnCode LKSerRspMimeHdrFieldGet(TSHttpTxn txnp, char *name, int name_length, char* **value, int *value_count)
{
	TSMBuffer bufp;
	TSMLoc hdr_loc;

	if(TSHttpTxnServerRespGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS) {
		return TS_ERROR;
	}

	return LKHdrFieldValue(bufp, hdr_loc, name, name_length, value, value_count);
}

TSReturnCode LKCliReqHdrMethodGet(TSHttpTxn txnp, char **method)
{
	TSMBuffer bufp;
	TSMLoc hdr_loc;
	int len;
	
	TSReleaseAssert(TSHttpTxnClientReqGet(txnp, &bufp, &hdr_loc) == TS_SUCCESS);

	*method = (char *)TSHttpHdrMethodGet(bufp, hdr_loc, &len);
	TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

	if(!*method || !len){
		return TS_ERROR;
	}

	return TS_SUCCESS;
}

TSReturnCode LKCliReqHdrHostAndPathGet(TSHttpTxn txnp, char **netloc, char **path)
{
	TSMBuffer bufp;
	TSMLoc hdr_loc;
	TSMLoc url_loc;
	int len;
	
	TSReleaseAssert(TSHttpTxnClientReqGet(txnp, &bufp, &hdr_loc) == TS_SUCCESS);

	TSReleaseAssert(TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc) == TS_SUCCESS);

	*netloc = (char *)TSUrlHostGet(bufp, url_loc, &len);
	//if absolute , then get netloc and path from netloc
	//else path+query = netloc
	//     netloc get from headers->Host:
	if(!*netloc || !len){
		return TS_ERROR;
	}
	if(**netloc == '/')
	{
		// relatvie
		char **value;
		int value_len;
		*path = *netloc;
		LKHdrFieldValue(bufp, hdr_loc, "Host", 4, &value, &value_len);
		if(value_len > 0)
		  *netloc = value[0];
	}
	else {
		*path = (char *)TSUrlPathGet(bufp, url_loc, &len);
		if(!*path || !len){
			*path = NULL;
		}
	}

	TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
	TSHandleMLocRelease(bufp, TS_NULL_MLOC, url_loc);
	return TS_SUCCESS;	
}

char *LKGetUrlHost(char *url)
{
	return NULL;
}
char *LKGetParamUrl(char *url)
{
	return NULL;
}
int LKGetUrlType(char* url,int url_len,char* url_type)
{
	return 0;
}
