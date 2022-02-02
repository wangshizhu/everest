#ifndef PLATFORM_H_
#define PLATFORM_H_

#if defined( __WIN32__ ) || defined( WIN32 ) || defined( _WIN32 )
#pragma warning(disable:4996)

#ifndef WIN32_LEAN_AND_MEAN 
//#include <winsock2.h>		// 必须在windows.h之前包含， 否则网络模块编译会出错
//#include <mswsock.h> 
#define WIN32_LEAN_AND_MEAN
#endif

//#include <windows.h> 
//#include <memory>
//#include <Ws2tcpip.h>

#define _SCL_SECURE_NO_WARNINGS

#else
// linux include
#include <poll.h>
#include <errno.h>
#include <float.h>
#include <pthread.h>	
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <iconv.h>
#include <langinfo.h>   /* CODESET */
#include <stdint.h>
#include <signal.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h> 
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <tr1/unordered_map>
#include <tr1/functional>
#include <tr1/memory>
#include <linux/types.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/resource.h> 
#include <linux/errqueue.h>
#endif

NAMESPACE_EVEREST_BEGIN

// current platform and compiler
#define PLATFORM_WIN32			0
#define PLATFORM_UNIX			1
#define UNIX_FLAVOUR_LINUX		1

#if defined( __WIN32__ ) || defined( WIN32 ) || defined( _WIN32 )
#define GENERAL_PLATFORM PLATFORM_WIN32
#define GENERAL_SNPRINTF _snprintf
#else
#define GENERAL_PLATFORM PLATFORM_UNIX
#define GENERAL_SNPRINTF snprintf
#endif

NAMESPACE_EVEREST_END

#endif // !PLATFORM_H_

