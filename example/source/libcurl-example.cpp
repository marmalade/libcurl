//-----------------------------------------------------------------------------

#include "libcurl-example.h"
#include <string>
#include "s3eMemory.h"
#include "ExamplesMain.h"
#include "IwGx.h"
#include "IwGxPrint.h"
#include <curl_config.h>
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

enum HTTPStatus
{
	kStarting,
	kNone,
	kDownloading,
	kFinalizing,
	kOK,
	kError,
	kNext,
	kNextError,
};

struct MemoryStruct {
  char *memory;
  size_t size;
};

#define HTTP_URI "http://http-test.ideaworks3d.net/example.txt"
#define HTTP_URI2 "http://www.doroga.tv/robots.txt"

CURL *curl[] = { 0, 0 }; // we are testing the only two connetions, but it might be much more
CURLM *curlm = 0;
const char *errmsg = "";
uint32 len = 0;
HTTPStatus status = kStarting;
struct MemoryStruct chunk = { 0, 0 }; // the real application needs more accurate code
struct MemoryStruct chunk2 = { 0, 0 }; // the real application needs more accurate code
int still_running=-1; /* keep number of running handles */

static size_t
GotData(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)data;
  mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    printf("not enough memory (realloc returned NULL)\n");
    exit(EXIT_FAILURE);
  }
 
  memcpy(&(mem->memory[mem->size]), ptr, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

int ExampleReset(void *,void *);
int ExampleStart(void *,void *)
{
	curl_global_init(CURL_GLOBAL_ALL);
	return ExampleReset(0,0);
}
//-----------------------------------------------------------------------------
void ExampleInit()
{
    IwGxInit();
	s3eTimerSetTimer(5000,ExampleStart,0);
}

//-----------------------------------------------------------------------------
void ExampleShutDown()
{
	if (curl[0]) 
	{
		curl_easy_cleanup(curl[0]);
		curl_easy_cleanup(curl[1]);
	}
    if (curlm) 
		curl_multi_cleanup(curlm);

	free(chunk.memory);
	free(chunk2.memory);
	curl_global_cleanup();
	IwGxTerminate();
}
//-----------------------------------------------------------------------------
int ExampleReset(void *,void *)
{
    if (curlm) 
		curl_multi_cleanup(curlm);

	curlm = NULL;

	if (curl[0]) curl_easy_cleanup(curl[0]);
	if (curl[1]) curl_easy_cleanup(curl[1]);
	curl[0] = curl[1] = NULL;

	if(chunk.memory) free(chunk.memory);
	if(chunk2.memory) free(chunk2.memory);
	memset(&chunk,0,sizeof(chunk));
	memset(&chunk2,0,sizeof(chunk));
	status = kNone;
	still_running=-1;
	return 0;
}

bool ExampleUpdate()
{
	if(status == kNone)
	{
		// The starting point, transfer initialization

		chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */ 
		chunk.memory[0] = 0;
		chunk.size = 0;    /* no data at this point */ 

		chunk2.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */ 
		chunk2.memory[0] = 0;
		chunk2.size = 0;    /* no data at this point */ 

		curl[0] = curl_easy_init();	
		curl_easy_setopt(curl[0], CURLOPT_URL, HTTP_URI);
		curl_easy_setopt(curl[0], CURLOPT_WRITEFUNCTION, GotData);
		curl_easy_setopt(curl[0], CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl[0], CURLOPT_USERAGENT, "libcurl-airplay-agent/1.0");
		curl_easy_setopt(curl[0],CURLOPT_CONNECTTIMEOUT, 15);
		curl_easy_setopt(curl[0],CURLOPT_TIMEOUT, 30);

		curl[1] = curl_easy_init();	
		curl_easy_setopt(curl[1], CURLOPT_URL, HTTP_URI2);
		curl_easy_setopt(curl[1], CURLOPT_WRITEFUNCTION, GotData);
		curl_easy_setopt(curl[1], CURLOPT_WRITEDATA, (void *)&chunk2);
		curl_easy_setopt(curl[1], CURLOPT_USERAGENT, "libcurl-airplay-agent/1.0");
		curl_easy_setopt(curl[1],CURLOPT_CONNECTTIMEOUT, 15);
		curl_easy_setopt(curl[1],CURLOPT_TIMEOUT, 30);

		curlm =  curl_multi_init();
		curl_multi_add_handle(curlm, curl[0]);
		curl_multi_add_handle(curlm, curl[1]);

		// first start
		curl_multi_perform(curlm, &still_running);
	    status = kDownloading;
		if(!still_running)
			status = kFinalizing;
	}
	if(status == kDownloading)
	{
		curl_multi_perform(curlm, &still_running);
		if(!still_running)
			status = kFinalizing;
	}
    else if(status == kFinalizing)
    {
	  CURLMsg *msg; /* for picking up messages with the transfer status */
	  int msgs_left; /* how many messages are left */
	  status = kOK;
	  while ((msg = curl_multi_info_read(curlm, &msgs_left))) {
		if (msg->msg == CURLMSG_DONE) {
			if( msg->data.result != CURLE_OK ) {
				status = kError;
				errmsg = curl_easy_strerror(msg->data.result);
				break;
			}
		}
	  }
    }
    else if(status == kOK )
    {
		status = kNext;
		s3eTimerSetTimer(2000,ExampleReset,0);
    }
    else if(status == kError )
    {
		status = kNextError;
		s3eTimerSetTimer(2000,ExampleReset,0);
    }
    
	return true;
}

//-----------------------------------------------------------------------------
void ExampleRender()
{
	// Clear screen
	IwGxClear( IW_GX_COLOUR_BUFFER_F | IW_GX_DEPTH_BUFFER_F );
	// Render text

	if( status == kOK || status == kNext )
	{
	    char buf[512];
	    
        if (chunk.memory && chunk.size > 1)
        {
			snprintf(buf, 500, "1) -> %s", chunk.memory);
		    IwGxPrintString(10, 40, buf, 1);
		}
        if (chunk2.memory && chunk2.size > 1)
        {
			snprintf(buf, 500, "2) -> %s", chunk2.memory);
		    IwGxPrintString(10, 50, buf, 1);
		}
	}
	
	if(status == kStarting)
	{
        IwGxPrintString(10, 20, "Starting ...", 1);
    }
	else if(status == kError)
	{
        IwGxPrintString(10, 20, "Error!", 1);
        IwGxPrintString(10, 30, errmsg, 1);
    }
	else if(status == kNextError)
	{
        IwGxPrintString(10, 20, "Error! - waiting next ...", 1);
        IwGxPrintString(10, 30, errmsg, 1);
    }
	else if(status == kFinalizing)
	{
        IwGxPrintString(10, 20, "Finalizing...", 1);
    }
	else if(status == kOK)
	{
        IwGxPrintString(10, 20, "OK!", 1);
    }
	else if(status == kNext)
	{
        IwGxPrintString(10, 20, "OK! - waiting next ...", 1);
    }
	else
	{
	    IwGxPrintString(10, 20, "Connecting...", 1);
	}

	// Swap buffers
	IwGxFlush();
	IwGxSwapBuffers();
}
