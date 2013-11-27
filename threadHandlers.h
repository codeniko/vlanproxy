#ifndef _threadHandlers_h_
#define _threadHandlers_h_

#include "vpnproxy.h"

#define MAX_CONNECTIONS		20

void *vpnlisten(void *args);
void *handle_public(void *arg);
void *handle_private(void *arg);
//void handle_signal(int sig);

#endif
