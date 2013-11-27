#ifndef _vpnproxy_h_
#define _vpnproxy_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
#include <endian.h>

#include <netdb.h>
#include <signal.h>
#include "LL.h"
#include "defines.h"
#include "threadHandlers.h"

#define BUFFER_CONF_SIZE	256
#define MAX_DEV_LINE 256

void printHelp();
int allocate_tunnel(char *dev, int flags char* local_mac);
int sendall(int sock, char *buf, int *len);
int createListenSocket(struct sockaddr_in *sockaddr);
int createSocket(Peer *peer);
int readConf(char *conf);
int vpnconnect(Peer *peer);
void freePeer(Peer *peer);
void *get_in_addr(struct sockaddr *sa);

#endif
