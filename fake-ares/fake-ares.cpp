#include <ares.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>

#include <string>
#include <list>
#include <vector>
#include <set>

#include <s3e.h>

#define IW_DEBUG_FAKE_ARES

#if defined(IW_DEBUG) && defined(IW_DEBUG_FAKE_ARES)
inline char *mvsprintf(const char *fmt,...)
{
	static char buf[10000];
    va_list ap;

    va_start (ap, fmt);

	vsprintf(buf,fmt,ap);

	va_end (ap);
	return buf;
}
#define DebugTracePrintf(a) s3eDebugTraceLine(mvsprintf a)
#else
#define DebugTracePrintf(a)
#endif

namespace __ares_internal__ {
	struct QueueEntry {
		std::string host;
		int64 timeout;
		ares_host_callback cb;
		void *arg;
		QueueEntry() {}
		~QueueEntry() {}
	};

	class Queue {
		std::list<QueueEntry *> _queue;

		Queue(const Queue &);
	public:
		Queue() {
			DebugTracePrintf(("Queue constructed:%p",this));
		}
		~Queue() {
			while( _queue.size() ) {
				first_done(ARES_EDESTRUCTION,0);
			}
			DebugTracePrintf(("Queue destructed:%p",this));
		}
		void add(std::string host,int64 timeout,ares_host_callback cb,void *arg)
		{
			DebugTracePrintf(("Request enqueued:%p -> %s",this,host.c_str()));
			QueueEntry *e = new QueueEntry();
			e->host = host;
			e->timeout = timeout;
			e->cb = cb;
			e->arg = arg;
			_queue.push_back(e);
		}
		QueueEntry *first() {
			if( _queue.begin() == _queue.end() )
				return 0;
			return *_queue.begin();
		}
		void first_done(int status, hostent *ent) {
			if( status == ARES_SUCCESS ) {
				DebugTracePrintf(("Request result returned:%p -> %s:%d.%d.%d.%d",this,ent->h_name,ent->h_addr[0],ent->h_addr[1],ent->h_addr[2],ent->h_addr[3]));
			} else {
				DebugTracePrintf(("Request result error returned:%p",this));
			}
			if( _queue.begin() == _queue.end() )
				return;
			QueueEntry *f = *_queue.begin();
			_queue.pop_front();
			f->cb(f->arg,status,0,ent);
			delete f;
		}
		void cancel() {
			DebugTracePrintf(("Channel cancel has been called:%p",this));
			while( _queue.size() ) {
				first_done(ARES_ECANCELLED,0);
			}
		}
		size_t check_timeouts(int64 timenow) {
			std::list<QueueEntry *>::iterator e = _queue.end();
			std::list<QueueEntry *>::iterator i = _queue.begin();
			while( i != e ) {
				QueueEntry *f = *i;
				std::list<QueueEntry *>::iterator t = i;
				i++;
				if( f->timeout <= timenow ) {
					DebugTracePrintf(("Timeout happens:%p -> %s",this,f->host.c_str()));
					f->cb( f->arg,ARES_ETIMEOUT,1,0 );
					_queue.erase(t); // TODO: check for consistence!
				}
			}
			return _queue.size();
		}
	};
	class QueueManager {
		std::set<Queue *> _channels;
		Queue *current;
		s3eInetIPAddress result; // 0 - not yet received, s3eInetIPAddress(-1) - error received
		s3eInetAddress buffer;
		//int dummy_socket;

		enum {
			IDLE = 0,
			OUTSTANDING = 1
		} status;
		QueueManager() : current(0),result(0),status(IDLE)
		{
			DebugTracePrintf(("Queue manager constructed:%p",this));
			//dummy_socket = socket(AF_INET, SOCK_DGRAM, 0);
		}
		~QueueManager() {
			DebugTracePrintf(("Queue manager destructed:%p",this));
			s3eInetLookupCancel();
			while( _channels.size() ) {
				Queue *c = *_channels.begin();
				_channels.erase(c);
				delete c;
			}
			//close(dummy_socket);
		}
		static QueueManager *_manager;
	public:
		static QueueManager *manager();
		static void deinitialize();

		size_t check_timeouts(int64 timenow, Queue *channel) {
			if( status == OUTSTANDING ) { // outstanding request
				if( (channel == current) && current->first()->timeout <= timenow ) { // timeout on outstanding
					s3eInetLookupCancel();
					status = IDLE;
				}
			}
			std::set<Queue *>::iterator e = _channels.end();
			std::set<Queue *>::iterator f = _channels.find(channel);
			size_t s = 0;
			if( f != e ) {
				s += (*f)->check_timeouts(timenow);
			}
			return s;
		}

		void check_result(Queue *channel) {
			if( status == OUTSTANDING ) { // outstanding request
				if( (channel == current) && result ) { // result has been just received
					if( result == s3eInetIPAddress(-1) ) { // error received
						DebugTracePrintf(("Error happened for:%p",current));
						current->first_done(ARES_ENOTFOUND,0);
					} else {
						DebugTracePrintf(("Success happened for:%p",current));
						hostent ent;
						//char *hostname = new char[current->first()->host.size()+1];
						//memcpy(hostname,current->first()->host.c_str(),current->first()->host.size());
						//hostname[current->first()->host.size()] = 0;
						//ent.h_name = hostname;
						ent.h_name = (char *)current->first()->host.c_str();
						ent.h_length = 4;
						char *addr_list[2];
						char *aliases[1] = { NULL };
						addr_list[0] = (char*)&result;
						addr_list[1] = NULL;
						ent.h_addr_list = addr_list;
						ent.h_aliases = aliases;
						ent.h_addrtype = AF_INET;
						current->first_done(ARES_SUCCESS,&ent);
						//delete [] hostname;
					}
					result = 0;
					status = IDLE;
				}
			}
		}
		Queue *next_channel() {
			if( status == IDLE ) {
				std::set<Queue *>::iterator e = _channels.end();
				std::set<Queue *>::iterator b = _channels.begin();
				if( b != e ) {
					if( current ) {
						std::set<Queue *>::iterator i = _channels.find(current);
						if( i != e )
							i++;
						if( i == e )
							i = b;
						return *i;
					} else {
						return *b;
					}
				}
			}
			return 0;
		}
		void check_queue(Queue *channel) {
			if( status == IDLE ) {
				Queue *start = next_channel();
				current = start;
				for( ;current && start && next_channel() != start; current=next_channel() ) {
					if( current->first() ) {
						break;
					}
				}
				if( current && current->first() ) {
					QueueEntry *c = current->first();
					result = 0;
					DebugTracePrintf(("New lookup for:%p -> %s",current,c->host.c_str()));
					s3eInetLookup(c->host.c_str(),&buffer, lookupCallback, this);
					status = OUTSTANDING;
				}
			}
		}
		static int32 lookupCallback(void* systemData, void* userData)
		{
			DebugTracePrintf(("Lookup callback for:%p",userData));
			if( !systemData ) {
				DebugTracePrintf(("Lookup callback for:%p - error reported",userData));
				manager()->result = s3eInetIPAddress(-1);
			} else {
				DebugTracePrintf(("Lookup callback for:%p - success reported",userData));
				s3eInetAddress *a = (s3eInetAddress *)systemData;
				manager()->result = a->m_IPAddress;
			}
			return 0;
		}

		void step(Queue *channel) {
			check_result(channel);
			check_timeouts(s3eTimerGetUTC(),channel);
			check_queue(channel);
		}

		Queue *create_queue() {
			Queue *q = new Queue();
			_channels.insert(q);
			return q;
		}

		void cancel_queue(Queue *q) {
			if( q == current && status == OUTSTANDING ) {
				status = IDLE;
				s3eInetLookupCancel();
			}
			q->cancel();
		}

		void remove_queue(Queue *q) {
			if( q == current && status == OUTSTANDING ) {
				status = IDLE;
				s3eInetLookupCancel();
			}
			_channels.erase(q);
			delete q;
		}

		//int get_dummy_socket() {
		//	return dummy_socket;
		//}
	};

	QueueManager *QueueManager::_manager;

	QueueManager *QueueManager::manager() {
		if(!_manager) _manager = new QueueManager();
		return _manager;
	}
	void QueueManager::deinitialize() {
		if( _manager ) delete _manager;
		_manager = 0;
	}
}

typedef __ares_internal__::Queue Queue;
typedef __ares_internal__::QueueEntry QueueEntry;
typedef __ares_internal__::QueueManager QueueManager;


extern "C" {
void ares_cancel(ares_channel channel)
{
	Queue *q = (Queue *) channel;
	QueueManager::manager()->cancel_queue(q);
}

void ares_destroy(ares_channel channel)
{
	Queue *q = (Queue *) channel;
	QueueManager::manager()->remove_queue(q);
}

int ares_dup(ares_channel *dest,
                          ares_channel src)
{
	return ares_init(dest);
}


void ares_gethostbyname(ares_channel channel,
                                     const char *name,
                                     int family,
                                     ares_host_callback callback,
                                     void *arg)
{
	Queue *q = (Queue *)channel;
	q->add(name,s3eTimerGetUTC()+5000,callback,arg);
	QueueManager::manager()->step(q);
}

int ares_getsock(ares_channel channel,
                              ares_socket_t *socks,
                              int numsocks)
{
	Queue *q = (Queue *)channel;
	QueueManager::manager()->step(q);
	//Queue *q = (Queue *)channel;
	//if( q->first() && numsocks ) {
	//	socks[0] = QueueManager::manager()->get_dummy_socket();
	//	return 1;
	//}
	return 0;
}

int ares_init(ares_channel *channelptr)
{
	Queue *q = QueueManager::manager()->create_queue();
	*channelptr = q;
	return ARES_SUCCESS;
}

void ares_library_cleanup(void)
{
	QueueManager::deinitialize();
}

int ares_library_init(int flags)
{
	QueueManager::manager();
	return ARES_SUCCESS;
}

void ares_process(ares_channel channel,
                               fd_set *read_fds,
                               fd_set *write_fds)
{
	Queue *q = (Queue *)channel;
	QueueManager::manager()->step(q);
}

void ares_process_fd(ares_channel channel,
                                  ares_socket_t read_fd,
                                  ares_socket_t write_fd)
{
	Queue *q = (Queue *)channel;
	QueueManager::manager()->step(q);
}



const char *ares_strerror(int code)
{
  /* Return a string literal from a table. */
  const char *errtext[] = {
    "Successful completion",
    "DNS server returned answer with no data",
    "DNS server claims query was misformatted",
    "DNS server returned general failure",
    "Domain name not found",
    "DNS server does not implement requested operation",
    "DNS server refused query",
    "Misformatted DNS query",
    "Misformatted domain name",
    "Unsupported address family",
    "Misformatted DNS reply",
    "Could not contact DNS servers",
    "Timeout while contacting DNS servers",
    "End of file",
    "Error reading file",
    "Out of memory",
    "Channel is being destroyed",
    "Misformatted string",
    "Illegal flags specified",
    "Given hostname is not numeric",
    "Illegal hints flags specified",
    "c-ares library initialization not yet performed",
    "Error loading iphlpapi.dll",
    "Could not find GetNetworkParams function",
    "DNS query cancelled"
  };

  if(code >= 0 && code < (int)(sizeof(errtext) / sizeof(*errtext)))
    return errtext[code];
  else
    return "unknown";
}

struct timeval *ares_timeout(ares_channel channel,
                                          struct timeval *maxtv,
                                          struct timeval *tv)
{
	Queue *q = (Queue *)channel;
	QueueManager::manager()->step(q);
	return maxtv;
}

const char *ares_version(int *version)
{
  if(version)
    *version = ARES_VERSION;

  return ARES_VERSION_STR;
}
}