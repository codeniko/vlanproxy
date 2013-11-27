#ifndef _typeHandlers_h_
#define _typeHandlers_h_

#include "vpnproxy.h"

void dataHandle(char *buffer, int len);
void leaveHandle(char *buffer, int len);
void quitHandle(char *buffer, int len);
void linkStateHandle(char *buffer, int len, Peer *peer);

#endif
