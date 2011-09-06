#ifndef __ARES_H__
#define __ARES_H__

#include <netdb.h>
#include <sys/select.h>
#include <time.h>

#include <ares_version.h>

#define CARES_EXTERN


#define ARES_SUCCESS            0

/* Server error codes (ARES_ENODATA indicates no relevant answer) */
#define ARES_ENODATA            1
#define ARES_EFORMERR           2
#define ARES_ESERVFAIL          3
#define ARES_ENOTFOUND          4
#define ARES_ENOTIMP            5
#define ARES_EREFUSED           6

/* Locally generated error codes */
#define ARES_EBADQUERY          7
#define ARES_EBADNAME           8
#define ARES_EBADFAMILY         9
#define ARES_EBADRESP           10
#define ARES_ECONNREFUSED       11
#define ARES_ETIMEOUT           12
#define ARES_EOF                13
#define ARES_EFILE              14
#define ARES_ENOMEM             15
#define ARES_EDESTRUCTION       16
#define ARES_EBADSTR            17

/* ares_getnameinfo error codes */
#define ARES_EBADFLAGS          18

/* ares_getaddrinfo error codes */
#define ARES_ENONAME            19
#define ARES_EBADHINTS          20

/* Uninitialized library error code */
#define ARES_ENOTINITIALIZED    21          /* introduced in 1.7.0 */

/* ares_library_init error codes */
#define ARES_ELOADIPHLPAPI           22     /* introduced in 1.7.0 */
#define ARES_EADDRGETNETWORKPARAMS   23     /* introduced in 1.7.0 */

/* More error codes */
#define ARES_ECANCELLED         24          /* introduced in 1.7.0 */

/* Flag values */
#define ARES_FLAG_USEVC         (1 << 0)
#define ARES_FLAG_PRIMARY       (1 << 1)
#define ARES_FLAG_IGNTC         (1 << 2)
#define ARES_FLAG_NORECURSE     (1 << 3)
#define ARES_FLAG_STAYOPEN      (1 << 4)
#define ARES_FLAG_NOSEARCH      (1 << 5)
#define ARES_FLAG_NOALIASES     (1 << 6)
#define ARES_FLAG_NOCHECKRESP   (1 << 7)

/* Option mask values */
#define ARES_OPT_FLAGS          (1 << 0)
#define ARES_OPT_TIMEOUT        (1 << 1)
#define ARES_OPT_TRIES          (1 << 2)
#define ARES_OPT_NDOTS          (1 << 3)
#define ARES_OPT_UDP_PORT       (1 << 4)
#define ARES_OPT_TCP_PORT       (1 << 5)
#define ARES_OPT_SERVERS        (1 << 6)
#define ARES_OPT_DOMAINS        (1 << 7)
#define ARES_OPT_LOOKUPS        (1 << 8)
#define ARES_OPT_SOCK_STATE_CB  (1 << 9)
#define ARES_OPT_SORTLIST       (1 << 10)
#define ARES_OPT_SOCK_SNDBUF    (1 << 11)
#define ARES_OPT_SOCK_RCVBUF    (1 << 12)
#define ARES_OPT_TIMEOUTMS      (1 << 13)
#define ARES_OPT_ROTATE         (1 << 14)

/* Nameinfo flag values */
#define ARES_NI_NOFQDN                  (1 << 0)
#define ARES_NI_NUMERICHOST             (1 << 1)
#define ARES_NI_NAMEREQD                (1 << 2)
#define ARES_NI_NUMERICSERV             (1 << 3)
#define ARES_NI_DGRAM                   (1 << 4)
#define ARES_NI_TCP                     0
#define ARES_NI_UDP                     ARES_NI_DGRAM
#define ARES_NI_SCTP                    (1 << 5)
#define ARES_NI_DCCP                    (1 << 6)
#define ARES_NI_NUMERICSCOPE            (1 << 7)
#define ARES_NI_LOOKUPHOST              (1 << 8)
#define ARES_NI_LOOKUPSERVICE           (1 << 9)
/* Reserved for future use */
#define ARES_NI_IDN                     (1 << 10)
#define ARES_NI_IDN_ALLOW_UNASSIGNED    (1 << 11)
#define ARES_NI_IDN_USE_STD3_ASCII_RULES (1 << 12)

/* Addrinfo flag values */
#define ARES_AI_CANONNAME               (1 << 0)
#define ARES_AI_NUMERICHOST             (1 << 1)
#define ARES_AI_PASSIVE                 (1 << 2)
#define ARES_AI_NUMERICSERV             (1 << 3)
#define ARES_AI_V4MAPPED                (1 << 4)
#define ARES_AI_ALL                     (1 << 5)
#define ARES_AI_ADDRCONFIG              (1 << 6)
/* Reserved for future use */
#define ARES_AI_IDN                     (1 << 10)
#define ARES_AI_IDN_ALLOW_UNASSIGNED    (1 << 11)
#define ARES_AI_IDN_USE_STD3_ASCII_RULES (1 << 12)
#define ARES_AI_CANONIDN                (1 << 13)

#define ARES_AI_MASK (ARES_AI_CANONNAME|ARES_AI_NUMERICHOST|ARES_AI_PASSIVE| \
                      ARES_AI_NUMERICSERV|ARES_AI_V4MAPPED|ARES_AI_ALL| \
                      ARES_AI_ADDRCONFIG)
#define ARES_GETSOCK_MAXNUM 16 /* ares_getsock() can return info about this
                                  many sockets */
#define ARES_GETSOCK_READABLE(bits,num) (bits & (1<< (num)))
#define ARES_GETSOCK_WRITABLE(bits,num) (bits & (1 << ((num) + \
                                         ARES_GETSOCK_MAXNUM)))

/* c-ares library initialization flag values */
#define ARES_LIB_INIT_NONE   (0)
#define ARES_LIB_INIT_WIN32  (1 << 0)
#define ARES_LIB_INIT_ALL    (ARES_LIB_INIT_WIN32)

typedef void *ares_channel;
typedef int ares_socket_t;
#define ARES_SOCKET_BAD -1

typedef void (*ares_host_callback)(void *arg,
                                   int status,
                                   int timeouts,
                                   struct hostent *hostent);
#ifdef __cplusplus
extern "C" {
#endif

CARES_EXTERN void ares_cancel(ares_channel channel);

CARES_EXTERN void ares_destroy(ares_channel channel);

CARES_EXTERN int ares_dup(ares_channel *dest,
                          ares_channel src);


CARES_EXTERN void ares_gethostbyaddr(ares_channel channel,
                                     const void *addr,
                                     int addrlen,
                                     int family,
                                     ares_host_callback callback,
                                     void *arg);

CARES_EXTERN void ares_gethostbyname(ares_channel channel,
                                     const char *name,
                                     int family,
                                     ares_host_callback callback,
                                     void *arg);

CARES_EXTERN int ares_getsock(ares_channel channel,
                              ares_socket_t *socks,
                              int numsocks);

CARES_EXTERN int ares_init(ares_channel *channelptr);

CARES_EXTERN void ares_library_cleanup(void);

CARES_EXTERN int ares_library_init(int flags);

CARES_EXTERN void ares_process(ares_channel channel,
                               fd_set *read_fds,
                               fd_set *write_fds);

CARES_EXTERN void ares_process_fd(ares_channel channel,
                                  ares_socket_t read_fd,
                                  ares_socket_t write_fd);


CARES_EXTERN int ares_fds(ares_channel channel,
                          fd_set *read_fds,
                          fd_set *write_fds);

CARES_EXTERN const char *ares_strerror(int code);

CARES_EXTERN struct timeval *ares_timeout(ares_channel channel,
                                          struct timeval *maxtv,
                                          struct timeval *tv);

CARES_EXTERN const char *ares_version(int *version);
#ifdef __cplusplus
}
#endif

#endif