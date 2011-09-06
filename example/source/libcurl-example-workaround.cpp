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
#include <ares.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

enum HTTPStatus
{
	kNone,
	kStarting,
	kResolving,
	kStartDownload,
	kDownloading,
	kFinishingOK,
	kOK,
	kFinishingError,
	kError,
};

const char *HTTPStatusName[] = {
	"None",
	"Starting",
	"Resolving",
	"StartDownload",
	"Downloading",
	"FinishingOK",
	"OK",
	"FinishingError",
	"Error",
	0
};

std::string extract_host(const std::string &url) {
	size_t p1 = url.find("://");
	if( p1 == std::string::npos )
		return "";
	p1 += 3;
	size_t p2 = url.find(":",p1);
	size_t p3 = url.find("/",p1);
	if( p2 != std::string::npos && (p3 == std::string::npos || p2 < p3) ) {
		p3 = p2;
	}
	if( p3 == std::string::npos )
		return url.substr(p1);
	return url.substr(p1,p3 - p1);
}

std::string extract_port(const std::string &url) {
	size_t p1 = url.find("://");
	if( p1 == std::string::npos )
		return "";
	p1 += 3;
	size_t p2 = url.find(":",p1);
	size_t p3 = url.find("/",p1);
	if( p2 == std::string::npos )
		return "";
	return url.substr(p2,p3);
}

std::string extract_path(const std::string &url) {
	size_t p1 = url.find("://");
	if( p1 == std::string::npos )
		return "";
	p1 += 3;
	size_t p3 = url.find("/",p1);
	if( p3 == std::string::npos )
		return "";
	return url.substr(p3);
}

std::string extract_proto(const std::string &url) {
	size_t p1 = url.find("://");
	if( p1 == std::string::npos )
		return "";
	return url.substr(0,p1);
}
class DnsCache
{
	struct DnsCacheEntry {
		int64_t last_update;
		std::string result;
		DnsCacheEntry()
			: last_update(s3eTimerGetUTC())
		{
		}
		DnsCacheEntry(const std::string &a_result)
			: last_update(s3eTimerGetUTC()),
			result(a_result)
		{
		}
		DnsCacheEntry(const std::string &a_result,int64_t a_last_update)
			: last_update(a_last_update),
			result(a_result)
		{
		}
		DnsCacheEntry(const DnsCacheEntry &a)
			: last_update(a.last_update),
			result(a.result)
		{
		}
		DnsCacheEntry &operator=(const DnsCacheEntry &a)
		{
			last_update = a.last_update;
			result = a.result;
			return *this;
		}
	};
	std::map<std::string,DnsCacheEntry> dns_cache;

	// Forbid copying
	DnsCache(const DnsCache &);
	DnsCache &operator=(const DnsCache &);
public:
	DnsCache() {
	}
	virtual ~DnsCache() {
	}

	std::string get_ent(const std::string &hostname) const {
		std::map<std::string,DnsCacheEntry>::const_iterator e = dns_cache.end();
		std::map<std::string,DnsCacheEntry>::const_iterator f = dns_cache.find(hostname);
		if( f == e )
			return "";
#ifdef _DEBUG
		int64_t fresh = 30 * 1000;
#else
		int64_t fresh = 300 * 1000;
#endif
		if( s3eTimerGetUTC() - f->second.last_update >= fresh )
			return "";
		return f->second.result;
	}
	void add_ent(const std::string &hostname, const std::string &ent) {
		dns_cache[hostname].result = ent;
	}
	void add_ent(const std::string &hostname, int32_t result) {
		add_ent(hostname,std::string((char *)&result,4));
	}
	void add_ent(const std::string &hostname, hostent *result) {
		add_ent(hostname,std::string((char *)&result->h_addr,result->h_length));
	}
};

class Request {
	CURL *curl;
	ares_channel ares;
	curl_slist *headers;
	CURLM *curlm;
	CURLSH *curlsh;
	DnsCache *dns_cache;
	HTTPStatus state;
	int errcode;
	std::string url;
	std::string redirect_url;
	std::string direct_ip;
	std::string content;
	std::string errmsg;
	bool canceling;
	bool relocating;
public:
	Request(const char *a_url)
		: curl(0),ares(0),headers(0),curlm(0),curlsh(0),dns_cache(0),
		state(kNone),errcode(CURLE_OK),url(a_url),canceling(false),relocating(false)
	{
	}
	virtual ~Request()
	{
		cleanup();
	}
	void start(CURLM *a_curlm,CURLSH *a_curlsh,DnsCache *a_dns_cache) {
		state = kStarting;
		curlm = a_curlm;
		curlsh = a_curlsh;
		dns_cache = a_dns_cache;
		start_resolve();
	}
	void step() {
		if( state == kResolving ) {
			if( ares ) {
				if( canceling ) {
					ares_cancel(ares);
				}
				int nfds, count;
				fd_set readers, writers;
				struct timeval tv, maxtv, *tvp;
				FD_ZERO(&readers);
				FD_ZERO(&writers);
				nfds = ares_fds(ares, &readers, &writers);
				maxtv.tv_sec = 0;
				maxtv.tv_usec = 1000;
				if (nfds == 0) {
					ares_process(ares, NULL, NULL);
				} else {
					tvp = ares_timeout(ares, &maxtv, &tv);
					count = select(nfds, &readers, &writers, NULL, tvp);
					ares_process(ares, &readers, &writers);
				}
			}
		}
		if( state == kStartDownload ) {
			if( ares )
				ares_destroy( ares );
			ares = 0;
			start_curl();
		}
		if( state == kFinishingOK ) {
			got_done();
		}
		if( state == kFinishingError ) {
			got_error();
		}
		// Nothing to do with other states
	}
	void start_resolve() {
		if( ares ) {
			ares_destroy(ares);
			ares = 0;
		}
		state = kResolving;
		std::string actual_url = redirect_url.size() ? redirect_url:url;
		std::string host = extract_host(actual_url);
		std::string res = dns_cache->get_ent(host);
		if( !res.size() ) {
			ares_init(&ares);
			ares_gethostbyname(ares,host.c_str(),AF_INET,Request::GotResolveStatic,this);
		} else {
			char buf[1024];
			sprintf(buf,"%d.%d.%d.%d",res[0],res[1],res[2],res[3]);
			direct_ip = buf;
			state = kStartDownload;
		}
	}
	void start_curl() {
		state = kDownloading;
		curl = curl_easy_init();
		//assert(direct_ip.size());
		std::string actual_url = redirect_url.size() ? redirect_url:url;
		char buf[1024];
		sprintf(buf,"%s://%s%s%s",
					extract_proto(actual_url).c_str(),
					direct_ip.c_str(),
					extract_port(actual_url).c_str(),
					extract_path(actual_url).c_str());
		curl_easy_setopt(curl, CURLOPT_URL, buf);
		sprintf(buf,"Host: %s%s",extract_host(actual_url).c_str(),extract_port(actual_url).c_str());
		headers = curl_slist_append(headers,buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Request::GotData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-airplay-agent/1.0");
		curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT, 15);
		curl_easy_setopt(curl,CURLOPT_TIMEOUT, 30);
		curl_easy_setopt(curl,CURLOPT_NOPROGRESS, 0);
		curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION, 0);

		curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION, Request::GotHeaderStatic);
		curl_easy_setopt(curl,CURLOPT_HEADERDATA, this);

		curl_easy_setopt(curl,CURLOPT_PROGRESSFUNCTION, Request::GotProgressStatic);
		curl_easy_setopt(curl,CURLOPT_PROGRESSDATA, (void *)this);
		//curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
		if( curlsh )
			curl_easy_setopt(curl,CURLOPT_SHARE,curlsh);
		if( curlm )
			curl_multi_add_handle(curlm, curl);
	}
	void cleanup() {
		if( curl ) {
			curl_easy_cleanup(curl);
		}
		curl = 0;
		if( ares )
			ares_destroy( ares );
		ares = 0;
		if( headers ) {
			curl_slist_free_all(headers);
		}
		headers = 0;
		errmsg = "";
		content = "";
		url = "";
		state = kNone;
		canceling = false;
		relocating = false;
	}
	void got_error() {
		if( curl )
			curl_easy_cleanup(curl);
		curl = 0;
		if( ares )
			ares_destroy( ares );
		ares = 0;
		if( headers ) {
			curl_slist_free_all(headers);
		}
		headers = 0;
		canceling = false;
		relocating = false;
		state = kError;
	}
	void finish_error(int code,const char *msg) {
		errmsg = msg;
		errcode = code;
		state = kFinishingError;
	}
	void got_done() {
		if( curl )
			curl_easy_cleanup(curl);
		curl = 0;
		if( ares )
			ares_destroy( ares );
		ares = 0;
		if( headers ) {
			curl_slist_free_all(headers);
		}
		headers = 0;
		if( relocating ) {
			relocating = false;
			state = kStarting;
			start(curlm,curlsh,dns_cache);
		} else {
			state = kOK;
			canceling = false;
			relocating = false;
		}
	}
	void finish_done() {
		errmsg = "";
		errcode = CURLE_OK;
		state = kFinishingOK;
	}
	void cancel() {
		canceling = true;
	}
	CURL *get_curl() const { return curl; }
	std::string get_url() const { return url; }
	std::string get_content() const { return content; }
	size_t get_content_length() const { return content.size(); }
	std::string get_errmsg() const { return errmsg; }
	int get_errcode() const { return errcode; }
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
		if( !relocating ) {
			content += std::string((char *)ptr,size);
		}
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
	void GotResolve(int status, int timeouts, struct hostent *hostent)
	{
		switch(status) {
			case ARES_SUCCESS:
				{
					char buf[1024];
					sprintf(buf,"%d.%d.%d.%d",hostent->h_addr[0],hostent->h_addr[1],hostent->h_addr[2],hostent->h_addr[3]);
					direct_ip = buf;
					state = kStartDownload;
					std::string actual_url = redirect_url.size() ? redirect_url:url;
					dns_cache->add_ent(extract_host(actual_url),hostent);
			    }
				break;
			case ARES_EDESTRUCTION:
				break;
			case ARES_ECANCELLED:
				finish_error(status,"ARES_ECANCELLED");
				break;
			case ARES_ETIMEOUT:
				finish_error(status,"ARES_ETIMEOUT");
				break;
			case ARES_ENOTIMP:
				finish_error(status,"ARES_ENOTIMP");
				break;
			case ARES_EBADNAME:
				finish_error(status,"ARES_EBADNAME");
				break;
			case ARES_ENOTFOUND:
				finish_error(status,"ARES_ENOTFOUND");
				break;
			case ARES_ENOMEM:
				finish_error(status,"ARES_ENOMEM");
				break;
			default:
				finish_error(status,"ARES - UNKNOWN!");
				break;
		}
	}
	static void GotResolveStatic(void *arg, int status, int timeouts, struct hostent *hostent)
	{
		Request *mem = (Request *)arg;
		mem->GotResolve(status,timeouts,hostent);
	}
	size_t GotHeader(const char *ptr,size_t size)
	{
		if( !memcmp(ptr,"Location:",sizeof("Location:")-1) ) {
			long code = 0;
			curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
			if( code >= 300 && code < 400 ) {
				printf("Relocation happening\n");
				relocating = true;
				redirect_url = std::string(ptr+sizeof("Location:"),size - sizeof("Location:") - 2);
				while( redirect_url[0] == ' ' )
					redirect_url = redirect_url.substr(1);
			}
		}
		return size;
	}
	static size_t GotHeaderStatic( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		Request *mem = (Request *)userdata;
		return mem->GotHeader((const char *)ptr,size*nmemb);
	}
};

class RequestManager {
	std::list<Request *> queue;
	CURLM *curlm;
	CURLSH *curlsh;
	DnsCache dns_cache;
	size_t max_handles;
public:
	RequestManager(size_t a_max_handles)
		: curlm(0),curlsh(0),max_handles(a_max_handles)
	{
	}
	size_t get_max_handles() const { return max_handles; }
	CURLM *start()
	{
		//curlsh = curl_share_init();
		//curl_share_setopt(curlsh,CURLSHOPT_SHARE,CURL_LOCK_DATA_COOKIE);
		//curl_share_setopt(curlsh,CURLSHOPT_SHARE,CURL_LOCK_DATA_DNS);
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
		{
			std::list<Request *>::iterator e= queue.end();
			std::list<Request *>::iterator i= queue.begin();
			for( ; i != e; i++ ) {
				(*i)->step();
			}
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
				std::list<Request *>::iterator e= queue.end();
				std::list<Request *>::iterator f= queue.begin();
				for( ; f != e; f++ ) {
					if( (*f)->get_curl() == handle )
						break;
				}
				if( f == e ) {
					printf("REQUEST NOT FOUND FOR HANDLE %p\n",handle);
					continue;
				}
				curl_multi_remove_handle(curlm, handle);
				if( result != CURLE_OK ) {
					(*f)->finish_error(result,curl_easy_strerror(result));
				} else {
					(*f)->finish_done();
				}
			}
		}

		if( (size_t)handles >= max_handles ) {
			return;
		}
		if( active_requests() >= max_handles ) {
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
			(*i)->start(curlm,curlsh,&dns_cache);
		}
	}
	const std::list<Request *> &get_queue() const { return queue; }
	bool clean(Request *r) {
		switch(r->get_state()) {
		case kStarting:
		case kResolving:
		case kDownloading:
		case kFinishingError:
		case kFinishingOK:
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
		size_t r = 0;
		std::list<Request *>::iterator e = queue.end();
		std::list<Request *>::iterator i = queue.begin();

		for( ; i != e; i++ ) {
			switch( (*i)->get_state() ) {
				case kStarting:
				case kResolving:
				case kStartDownload:
				case kDownloading:
				case kFinishingOK:
				case kFinishingError:
					r++;
				default:
					break;
			}
		}
		return r;
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
			s3eDeviceYield(0);
		}
		curl_multi_cleanup(curlm);
		curlm = 0;
		curl_share_cleanup(curlsh);
		curlsh = 0;
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
	if( ExampleStep >= 100 ) {
		manager.stop();
		while( manager.get_queue().begin() != manager.get_queue().end() )
			manager.clean(*manager.get_queue().begin());
		ExampleStep = 0;
	}

	if( !ExampleStep ) {
		printf("--------------- STARTING ---------------\n");
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
	s3eDeviceYield(0);
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
