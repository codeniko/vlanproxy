#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_tun.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#include <netdb.h>
#include <signal.h>

#define MODE_SERVER			0
#define MODE_CLIENT			1
#define MAX_CONNECTIONS		1
#define HEADER_FIELD_SIZE	2
#define HEADER_SIZE			4
#define BUFFER_SIZE			2048
#define VLAN_TAG			0xABCD

struct ProxyInfo;
static struct ProxyInfo *proxyinfo;
static pthread_t thread_public;
static pthread_t thread_private;

void printHelp();
void closeFDs();
int allocate_tunnel(char *dev, int flags);
int createConnection();
void *threadTCP(void *arg);
void *threadTAP(void *arg);
int createSocket(char *host, int port);
void handle_signal(int sig);
