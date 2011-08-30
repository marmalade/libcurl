//-----------------------------------------------------------------------------

#include "libcurl-example.h"
#include <string>
#include <set>
#include <map>
#include <list>
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
	kNone,
	kStarting,
	kDownloading,
	kOK,
	kError,
};

const char *HTTPStatusName[] = {
	"None",
	"Starting",
	"Downloading",
	"OK",
	"Error",
	0
};

class Request {
	CURL *curl;
	HTTPStatus state;
	CURLcode errcode;
	std::string url;
	std::string content;
	std::string errmsg;
	bool canceling;
public:
	Request(const char *a_url)
		: curl(0),state(kNone),errcode(CURLE_OK),url(a_url),canceling(false)
	{
	}
	virtual ~Request()
	{
		cleanup();
	}
	CURL *start(CURLSH *curlsh) {
		state = kStarting;
		curl = curl_easy_init();	
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Request::GotData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-airplay-agent/1.0");
		curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT, 15);
		curl_easy_setopt(curl,CURLOPT_TIMEOUT, 30);
		curl_easy_setopt(curl,CURLOPT_NOPROGRESS, 0);
		curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl,CURLOPT_PROGRESSFUNCTION, Request::GotProgressStatic);
		curl_easy_setopt(curl,CURLOPT_PROGRESSDATA, (void *)this);
		if( curlsh )
			curl_easy_setopt(curl,CURLOPT_SHARE,curlsh);
		return curl;
	}
	void cleanup() {
		if( curl ) {
			curl_easy_cleanup(curl);
		}
		curl = 0;
		errmsg = "";
		content = "";
		url = "";
		state = kNone;
		canceling = false;
	}
	void got_started() {
		state = kDownloading;
	}
	void got_error(CURLcode code,const char *msg) {
		errmsg = msg;
		errcode = code;
		curl_easy_cleanup(curl);
		curl = 0;
		state = kError;
		canceling = false;
	}
	void got_done() {
		errmsg = "";
		errcode = CURLE_OK;
		curl_easy_cleanup(curl);
		curl = 0;
		state = kOK;
		canceling = false;
	}
	void cancel() {
		canceling = true;
	}
	CURL *get_curl() const { return curl; }
	std::string get_url() const { return url; }
	std::string get_content() const { return content; }
	size_t get_content_length() const { return content.size(); }
	std::string get_errmsg() const { return errmsg; }
	CURLcode get_errcode() const { return errcode; }
	HTTPStatus get_state() const { return state; }
	bool get_canceling() const { return canceling; }
private:
	static size_t GotData(void *ptr, size_t size, size_t nmemb, void *data)
	{
	  size_t realsize = size * nmemb;
	  Request *mem = (Request *)data;
	  return mem->GotContent(ptr,realsize);
	}
	size_t GotContent(void *ptr,size_t size) {
		content += std::string((char *)ptr,size);
		return size;
	}
	static int GotProgressStatic(void *clientp,double dltotal,double dlnow,double ultotal,double ulnow)
	{
		Request *mem = (Request *)clientp;
		return mem->GotProgress(dltotal,dlnow,ultotal,ulnow);
	}
	int GotProgress(double dltotal,double dlnow,double ultotal,double ulnow)
	{
		return canceling ? 1:0;
	}
};

class RequestManager {
	std::map<CURL *,Request *> request_map;
	std::list<Request *> queue;
	CURLM *curlm;
	CURLSH *curlsh;
	size_t max_handles;
public:
	RequestManager(size_t a_max_handles)
		: curlm(0),curlsh(0),max_handles(a_max_handles)
	{
	}
	size_t get_max_handles() const { return max_handles; }
	CURLM *start()
	{
		curlsh = curl_share_init();
		curl_share_setopt(curlsh,CURLSHOPT_SHARE,CURL_LOCK_DATA_COOKIE);
		curl_share_setopt(curlsh,CURLSHOPT_SHARE,CURL_LOCK_DATA_DNS);
		return curlm = curl_multi_init();
	}
	CURLM *get_curlm() const {
		return curlm;
	}
	void get(const char *url) {
		Request *r = new Request(url);
		queue.push_back(r);
	}
	void step() {
		if( !curlm ) {
			printf("CURLM SHOULD BE INITIALIZED, call start() before!!!\n");
			return;
		}
		int handles = 0;
		CURLMcode code = CURLM_OK;
		while( (code = curl_multi_perform(curlm, &handles)) == CURLM_CALL_MULTI_PERFORM )
			;
		if( code != CURLM_OK ) {
			printf("SOMETHING BAD HAPPENS: %d!!!\n",code);
			return;
		}

		CURLMsg *msg; /* for picking up messages with the transfer status */
		int msgs_left; /* how many messages are left */

		while ((msg = curl_multi_info_read(curlm, &msgs_left))) {
			if (msg->msg == CURLMSG_DONE) {
				CURLcode result = msg->data.result;
				CURL *handle = msg->easy_handle;
				std::map<CURL *,Request *>::iterator e = request_map.end();
				std::map<CURL *,Request *>::iterator f = request_map.find(handle);
				if( f == e ) {
					printf("REQUEST NOT FOUND FOR HANDLE %p\n",handle);
					continue;
				}
				curl_multi_remove_handle(curlm, handle);
				if( result != CURLE_OK ) {
					f->second->got_error(result,curl_easy_strerror(result));
				} else {
					f->second->got_done();
				}
				request_map.erase(f);
			}
		}

		if( (size_t)handles >= max_handles ) {
			return;
		}

		// select a new request to start
		std::list<Request *>::iterator e = queue.end();
		std::list<Request *>::iterator i = queue.begin();

		for( ; i != e; i++ ) {
			if( (*i)->get_state() == kNone && !(*i)->get_canceling() )
				break;
		}
		// start found request
		if( i != e && (*i)->get_state() == kNone && !(*i)->get_canceling() ) {
			// new request found
			CURL *handle = (*i)->start(curlsh);
			curl_multi_add_handle(curlm, handle);
			request_map[handle] = *i;
			(*i)->got_started();
		}
	}
	const std::list<Request *> &get_queue() const { return queue; }
	bool clean(Request *r) {
		switch(r->get_state()) {
		case kStarting:
		case kDownloading:
			r->cancel();
			return false; // not in this state - cancel request before
		case kNone:
		case kOK:
		case kError:
			{
				std::list<Request *>::iterator e = queue.end();
				std::list<Request *>::iterator i = queue.begin();
				for( ; i != e; i++) {
					if( (*i) == r ) {
						break;
					}
				}
				if( i == e ) {
					printf("REQUEST LIST BAD FOR REQUEST %p WHILE CLEAN\n",r);
					break;
				}
				delete r;
				queue.erase(i);
			}
			break;
		}
		return true;
	}
	size_t active_requests() {
		return request_map.size();
	}
	void stop() {
		{
			std::list<Request *>::iterator e = queue.end();
			std::list<Request *>::iterator i = queue.begin();
			for( ; i != e; i++ ) {
				(*i)->cancel();
			}
		}
		while( active_requests() ) {
			step();
		}
		curl_multi_cleanup(curlm);
		curlm = 0;
	}
};

class TileMatrix
{
	int x_min,y_min;
	int x_max,y_max;
	int z,x,y;
public:
	TileMatrix(int ax_min,int ay_min,int ax_max,int ay_max,int az)
		: x_min(ax_min),y_min(ay_min),
		x_max(ax_max),y_max(ay_max),
		z(az),x(ax_min),y(ay_min)
	{
	}
	int get_x() const { return x; }
	int get_y() const { return y; }
	int get_z() const { return z; }
	void step() {
		if( ++x >= x_max ) {
			x = x_min;
			if( ++y >= y_max ) {
				y = y_min;
			}
		}
	}
};

//#define HTTP_TILES "http://tile.openstreetmap.org/%d/%d/%d.png" // tile z,x,y
//#define HTTP_TILES "http://78.40.184.246/jams/%d/%d/%d.png" // tile z,x,y
//#define HTTP_TILES "http://jams1.doroga.tv/jams/%d/%d/%d.png" // tile z,x,y
#define HTTP_TILES "http://jams.doroga.tv/jams/%d/%d/%d.png" // tile z,x,y

RequestManager manager(3);
TileMatrix matrix(10189,5076,10192,5080,14);

//-----------------------------------------------------------------------------
void ExampleInit()
{
    IwGxInit();
	curl_global_init(CURL_GLOBAL_ALL);
}

//-----------------------------------------------------------------------------
void ExampleShutDown()
{
	manager.stop();
	curl_global_cleanup();
	IwGxTerminate();
}

int ExampleStep = 0;

bool ExampleUpdate()
{
	if( ExampleStep >= 30 ) {
		manager.stop();
		while( manager.get_queue().begin() != manager.get_queue().end() )
			manager.clean(*manager.get_queue().begin());
		ExampleStep = 0;
	}

	if( !ExampleStep ) {
		manager.start();
	}

	manager.step();
	ExampleStep += 1;
	if( manager.get_queue().size() < 20 ) {
		char buf[256];
		sprintf(buf,HTTP_TILES,matrix.get_z(),matrix.get_x(),matrix.get_y());
		manager.get(buf);
		matrix.step();
	}
	if( manager.active_requests() < manager.get_max_handles() ) {
		std::list<Request *>::const_iterator e = manager.get_queue().end();
		std::list<Request *>::const_iterator i = manager.get_queue().begin();
		for( ; i != e; i++ ) {
			if( (*i)->get_state() == kOK || (*i)->get_state() == kError ) {
				manager.clean(*i);
				break;
			}
		}
	}
	if( s3ePointerGetState(S3E_POINTER_BUTTON_SELECT) & S3E_POINTER_STATE_PRESSED ) {
		// random cancel
		std::list<Request *>::const_iterator e = manager.get_queue().end();
		std::list<Request *>::const_iterator i = manager.get_queue().begin();
		for( ; i != e; i++ ) {
			if( (*i)->get_state() == kDownloading ) {
				(*i)->cancel();
				break;
			}
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
void ExampleRender()
{
	// Clear screen
	IwGxClear( IW_GX_COLOUR_BUFFER_F | IW_GX_DEPTH_BUFFER_F );
	// Render text

	int sx = 10;
	int sy = 40;

	std::list<Request *>::const_iterator e = manager.get_queue().end();
	std::list<Request *>::const_iterator i = manager.get_queue().begin();
	int count = 0;
	for( ; i != e; i++ ) {
		char buf[1024];
		const char *name = HTTPStatusName[(*i)->get_state()];
		snprintf(buf, 1023, "%d) %s: %s/%d (%s)",count,(*i)->get_url().c_str(),name,(*i)->get_content_length(),(*i)->get_errmsg().c_str());
	    IwGxPrintString(sx, sy, buf, true);
		sy += 20;
		count++;
	}
	// Swap buffers
	IwGxFlush();
	IwGxSwapBuffers();
}
