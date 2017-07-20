#include <stdlib.h>
#include <string.h>
#include "ats_common.h"

typedef enum {
	LK_STATE_BUFFER_DATA,
	LK_STATE_OUTPUT_DATA
} LKState;

typedef struct {
	TSVIO incoming_vio;
	TSIOBuffer incoming_buffer;
	TSIOBufferReader incoming_reader;
	LKState state;
} IncomingData;

static IncomingData * incoming_data_alloc() 
{
	IncomingData *data;
	data = (IncomingData *)TSmalloc(sizeof(IncomingData));
	data->incoming_vio = NULL;
	data->incoming_buffer = NULL;
	data->incoming_reader = NULL;
	data->state = LK_STATE_BUFFER_DATA;

	return data;
}

static void print_TSIOBuffer(TSIOBuffer bufp)
{
	TSAssert(bufp != NULL);
	TSIOBufferReader buf_reader = TSIOBufferReaderAlloc(bufp);
	int64_t avail = TSIOBufferReaderAvail(buf_reader);
	TSIOBufferBlock block = TSIOBufferReaderStart(buf_reader);
	int64_t blk_avail;
	char *print_buf;
	int64_t print_len = 0;

	if(avail <= 0){
		return;
	}

	print_buf = (char*)TSmalloc(avail+1);
	for(; block && avail > 0; block = TSIOBufferBlockNext(block))
	{
		const char* const tmp  = TSIOBufferBlockReadStart(block, buf_reader, &blk_avail);
		memcpy(print_buf+print_len, tmp, (int)blk_avail);
		print_len += blk_avail;
		avail -= blk_avail;
	}
	
	memset(print_buf+print_len, '\0', 1);
	fprintf(stdout, "TSIOBuffer: %s\n", print_buf);
	TSfree(print_buf);
}

static void incoming_data_destroy(IncomingData *data) 
{
	if(data)
	{
		if(data->incoming_buffer)
		{
			TSIOBufferDestroy(data->incoming_buffer);
			TSfree(data);
		}
	}
}

static int handle_buffering(TSCont contp, IncomingData* data)
{
	TSVIO write_vio;
	int towrite;
	int avail;

	write_vio = TSVConnWriteVIOGet(contp);

	if(!data->incoming_buffer)
	{
		data->incoming_buffer = TSIOBufferCreate();
		TSAssert(data->incoming_buffer);
		data->incoming_reader = TSIOBufferReaderAlloc(data->incoming_buffer);
		TSAssert(data->incoming_reader);
	}

	if(!TSVIOBufferGet(write_vio))
	{
		data->state = LK_STATE_OUTPUT_DATA;
		return 0;
	}

	towrite = TSVIONTodoGet(write_vio);
	if(towrite > 0)
	{
		avail = TSIOBufferReaderAvail(TSVIOReaderGet(write_vio));
		if(towrite > avail)
		  towrite = avail;
		
		if(towrite > 0)
		{
			TSIOBufferCopy(data->incoming_buffer, TSVIOReaderGet(write_vio), towrite, 0);
			TSIOBufferReaderConsume(TSVIOReaderGet(write_vio), towrite);
			TSVIONDoneSet(write_vio, TSVIONDoneGet(write_vio)+towrite);
		}
	}

	/* Now we check the write VIO to see if there is data left to read. */
	if (TSVIONTodoGet(write_vio) > 0) {
		if (towrite > 0)
		{
			/* Call back the write VIO continuation to let it know that we
			 * are ready for more data. */
			TSContCall(TSVIOContGet(write_vio), TS_EVENT_VCONN_WRITE_READY, write_vio);
		}
    } else {
		data->state = LK_STATE_OUTPUT_DATA;
		print_TSIOBuffer(data->incoming_buffer);
		TSContCall(TSVIOContGet(write_vio), TS_EVENT_VCONN_WRITE_COMPLETE, write_vio);
	}

	return 1;

}

static int handle_output(TSCont contp, IncomingData *data)
{
	if(!data->incoming_vio){
		TSVConn incoming_conn;
		incoming_conn = TSTransformOutputVConnGet(contp);
		data->incoming_vio = TSVConnWrite(incoming_conn, contp, data->incoming_reader, TSIOBufferReaderAvail(data->incoming_reader));
		TSAssert(data->incoming_vio);
	}
	return 1;
}

static void handle_transform(TSCont contp)
{
	IncomingData *data;
	int done;

	data = TSContDataGet(contp);
	if(!data)
	{
		data = incoming_data_alloc();
		TSContDataSet(contp, data);
	}

	do{
		switch(data->state){
			case LK_STATE_BUFFER_DATA:
				done = handle_buffering(contp, data);
				break;
			case LK_STATE_OUTPUT_DATA:
				done = handle_output(contp, data);
				break;
			default:
				done = 1;
				break;
		}
	}while(!done);
}

static int analysis_transform(TSCont contp, TSEvent event, void *edata ATS_UNUSED)
{
	FUNC_ENTERING;
	DebugLK("%s() Get event ---> %d", __func__, event);

	/* Check to see if the transformation has been closed by a call to
	 * TSVConnClose.
	 */
	if(TSVConnClosedGet(contp))
	{
		DebugLK("VConn is closed");
		incoming_data_destroy(TSContDataGet(contp));
		TSContDestroy(contp);
		return TS_SUCCESS;
	}
	switch(event){
		case TS_EVENT_ERROR:
			DebugLK("%s() Get event: TS_EVENT_ERROR", __func__);
			
			TSVIO write_vio;
			write_vio = TSVConnWriteVIOGet(contp);
			TSContCall(TSVIOContGet(write_vio), TS_EVENT_ERROR, write_vio);
			break;
		case TS_EVENT_VCONN_WRITE_COMPLETE:
			DebugLK("%s() Get event: TS_EVENT_VCONN_WRITE_COMPLETE", __func__);
			TSVConnShutdown(TSTransformOutputVConnGet(contp), 0, 1);
			break;
		case TS_EVENT_VCONN_WRITE_READY:
		default:
			DebugLK("%s() Get event: TS_EVENT_VCONN_WRITE_READY", __func__);
			handle_transform(contp);
			break; 
	}

	return TS_SUCCESS;
}

static void analysis_add(TSHttpTxn txnp)
{
	FUNC_ENTERING;

	TSVConn connp;
	connp = TSTransformCreate(analysis_transform, txnp);
	TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, connp);
}

/*我们是否对这个url感兴趣*/
static int interested(TSHttpTxn txnp)
{
	return 1;
}

static int url_analysis_plugin(TSCont contp, TSEvent event, void *edata)
{
	FUNC_ENTERING;

	TSHttpTxn txnp = (TSHttpTxn)edata;
	DebugLK("Event: %d ", event);

	//char* method;
	//char* netloc;
	//char* path;

	switch(event){
	case TS_EVENT_HTTP_READ_RESPONSE_HDR:
		if(!interested(txnp)){
			TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
			break;
		}
		analysis_add(txnp);
/*
		//METHOD
		//if METHOD == 'CONNECT', skip
		//
		if(TS_SUCCESS != LKCliReqHdrMethodGet(txnp, &method)) {
			DebugLK("request METHOD get failed!");
			break;
		}
		DebugLK("-->METHOD: %s", method);
		
		//HOST and PATH
		if(TS_SUCCESS != LKCliReqHdrHostAndPathGet(txnp, &netloc, &path)) {
			DebugLK("request METHOD get failed!\n");
			break;
		}
		if( path == NULL ){
			path = malloc(2);
			memset(path, '/', 1);
			*(path+1) = '\0';
		}
		DebugLK("-->NETLOC: %s", netloc);
		DebugLK("-->PATH: %s", path);
*/
		TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
		break;
	default:
		break;
	}

	return TS_SUCCESS;
}

void TSPluginInit(int argc, const char* argv[])
{
	TSPluginRegistrationInfo info;
	info.plugin_name = (char*)"url_anaylysis";
	info.vendor_name = (char*)"LK";
	info.support_email = (char*)"soulfy@163.com";

	if(TSPluginRegister(&info) != TS_SUCCESS){
		return;
	}
	TSMutex mutex = TS_NULL_MUTEX;
	TSCont cont = TSContCreate(url_analysis_plugin, mutex);
	TSHttpHookAdd(TS_HTTP_READ_RESPONSE_HDR_HOOK, cont);
}
