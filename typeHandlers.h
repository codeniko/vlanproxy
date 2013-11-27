#ifndef _typeHandlers_h_
#define _typeHandlers_h_

#include "vpnproxy.h"

int createStateMessage(char **buffer, int init);
void sendInitState(Peer *peer);
unsigned long long genID();
void dataHandle(char *buffer, int len);
void leave(char *buffer, int len);
void leaveHandle(char *buffer, int len);
void quitHandle(char *buffer, int len);
void linkHandle(char *buffer, int len, Peer *peer);

#endif
