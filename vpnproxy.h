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
#include <stdint.h>
#include <linux/if_ether.h>

#include <netdb.h>
#include <signal.h>
#include "LL.h"
#include "defines.h"
#include "threadHandlers.h"
#include "typeHandlers.h"

#define BUFFER_CONF_SIZE	256
#define MAX_DEV_LINE 256

void printHelp();
void freePeer(Peer *peer);
void closePeer(Peer *peer);
void freeConfig();
void *get_in_addr(struct sockaddr *sa);
void getAddr(char *interface, uint8_t *mac, uint8_t *ip);
void ipntoh(uint8_t *ip, char *ip_string);
void macntoh(uint8_t *mac, char *mac_string);
int allocate_tunnel(char *dev, int flags, uint8_t *local_mac);
int sendall(int sock, char *buf, int *len);
int sendallstates(int sock, char *buf, int *len);
int readConf(char *conf);
Peer *getPeer(int sock);
Edge *getEdge(Peer *p1, Peer *p2);
Peer *findPeer(uint8_t *mac);
void removeAllEdgesWithPeer(Peer *p);
int vpnconnect(Peer *peer);
int createListenSocket(struct sockaddr_in *sockaddr);
int createSocket(Peer *peer);

void msg(char *buf, int len);
void dumpPeersList();
void printMac(char *a, uint8_t *mac);
void printIP(char *a, uint8_t *ip);

#endif
