#include "typeHandlers.h"

//len is full length, including header

int createStateMessage(char **buffer, int init)
{ //NOTE need the number of neighbors
	int i, bufsize;
	int numEdges = config->edgeList->size;
	printf("DEBUG: numEdges=%d\n",numEdges);
	if (init == 1)
		bufsize = 74;
	else 
		bufsize = 26+48*numEdges;
	printf("DEBUG: bufsize=%d\n",bufsize);
	*buffer = (char *)malloc(sizeof(char)*bufsize);
	if (*buffer == NULL) {
		fprintf(stderr, "Fatal Error: malloc has failed.");
		exit(1);
	}
	memset(*buffer, '\0', bufsize);
	unsigned long long *int64 = (unsigned long long *)((*buffer) + 26);
	uint32_t *int32 = (uint32_t *)(*buffer);
	uint16_t *int16 = (uint16_t *)(*buffer);
	uint8_t *int8 = (uint8_t *)(*buffer);
	*int16 = htons(TYPE_LINKSTATE); //MESSAGE TYPE, 2 bytes
	int8 = (uint8_t *)((*buffer)+6);
	for (i = 0; i < IP_SIZE; i++)
		*(int8 + i) = (config->peer->ip)[i]; // source listen IP
	int16 = (uint16_t *)((*buffer) + 10);
	*int16 = htons(config->peer->port); // source listen port
	int8 = (uint8_t *)((*buffer)+12);
	for (i = 0; i < MAC_SIZE; i++)
		*(int8 + i) = (config->peer->tapMac)[i]; // source tap mac
	int8 = (uint8_t *)((*buffer)+18);
	for (i = 0; i < MAC_SIZE; i++)
		*(int8 + i) = (config->peer->ethMac)[i]; // source eth mac

	int16 = (uint16_t *)((*buffer)+2);
	//generate neighbor list
	if (init == 1) // create initial connection link state, single record
	{
		*int16 = htons(70); //MESSAGE LENGTH, 2 bytes
		*(int16+1) = htons(1); // # of edges, 2 bytes
		int16 = (uint16_t *)((*buffer)+24);
		*int16 = htons(1); // # of edges, 2 bytes

		*int64 = genID(); // unique ID for edge record
		memcpy(buffer+26, buffer+6, 20);
		int32 = (uint32_t *)((*buffer)+70);
		*int32 = htonl(1); //link weight
	} else {
		int k;
		*int16 = htons(bufsize-HEADER_SIZE); //MESSAGE LENGTH, 2 bytes
		*(int16+1) = htons(numEdges); // # of edges, 2 bytes
		int16 = (uint16_t *)((*buffer)+24);
		*int16 = htons(numEdges); // # of edges, 2 bytes

		LLNode *edgeNode = config->edgeList->head;
		for (k = 26; k < bufsize; k+=48, edgeNode=edgeNode->next) {
			Edge *edge = (Edge *)(edgeNode->data);
			int64 = (unsigned long long *)((*buffer)+k);
			*int64 = genID(); // unique ID for edge record
			edge->id = (unsigned long long)(*int64);
			int8 = (uint8_t *)((*buffer)+k+8);
			for (i = 0; i < IP_SIZE; i++)
				*(int8 + i) = (edge->peer1->ip)[i]; // peer 1 listen IP
			int16 = (uint16_t *)((*buffer) + k + 12);
			*int16 = htons(edge->peer1->port); // peer 1 listen port
			int8 = (uint8_t *)((*buffer)+ k + 14);
			for (i = 0; i < MAC_SIZE; i++)
				*(int8 + i) = (edge->peer1->tapMac)[i]; // peer 1 tap mac
			int8 = (uint8_t *)((*buffer)+k+20);
			for (i = 0; i < MAC_SIZE; i++)
				*(int8 + i) = (edge->peer1->ethMac)[i]; // peer 1 eth mac
			int8 = (uint8_t *)((*buffer)+k+26);
			for (i = 0; i < IP_SIZE; i++)
				*(int8 + i) = (edge->peer2->ip)[i]; // peer 2 listen IP
			int16 = (uint16_t *)((*buffer) + k + 30);
			*int16 = htons(edge->peer2->port); // peer 2 listen port
			int8 = (uint8_t *)((*buffer)+ k + 32);
			for (i = 0; i < MAC_SIZE; i++)
				*(int8 + i) = (edge->peer2->tapMac)[i]; // peer 2 tap mac
			int8 = (uint8_t *)((*buffer)+k+38);
			for (i = 0; i < MAC_SIZE; i++)
				*(int8 + i) = (edge->peer2->ethMac)[i]; // peer 2 eth mac
			int32 = (uint32_t *)((*buffer)+k+44);
			*int32 = htonl(1); //Link weight
		}
	}
	return bufsize;
}

void sendInitState(Peer *peer)
{
	printf("Sending Init state packet to socket %d!\n", peer->sock);
	char *buffer = NULL;
	int bufsize = createStateMessage(&buffer, 1);
	printf("bufsize = %d\n", bufsize);
	if (sendall(peer->sock, buffer, &bufsize) == -1) {
		perror("Failed to send INIT STATE");
		fprintf(stderr, "#bytes left to send: %d", bufsize);
		exit(1);
	}
	free(buffer);
}

unsigned long long genID()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	unsigned long long timestamp = (unsigned int)tv.tv_sec;
	timestamp *= 1000000;
	timestamp += (unsigned int)tv.tv_usec;
	return timestamp;
}

void dataHandle(char *buffer, int len)
{
	int l = len-HEADER_SIZE;
	if ( sendall(config->tapFD, buffer+HEADER_SIZE, &l) == -1 )
	{
		perror("Unable to write to private interface.\n");
		exit(2);
	}
}
void leave(char *buffer, int len)
{
	uint16_t *int16 = (uint16_t *)(buffer+2); //length
	uint8_t *int8 = (uint8_t *)(buffer+10);//mac
	int i;

	if (ntohs(*int16) != 20)
		return;
	
	uint8_t mac[MAC_SIZE];
	for (i = 0; i < MAC_SIZE; i++)
		mac[i] = *(int8 + i); // peer 1 tap mac
	Peer *h = findPeer(mac);
	if (h == NULL)
		return;
	else
		closePeer(h);
}
void leaveHandle(char *buffer, int len)
{
	leave(buffer, len);
	LLNode *lln = config->peersList->head;
	for (; lln != NULL; lln = lln->next) {
		Peer *peer = (Peer *)lln->data;
		int l = len;
		if ( sendall(peer->sock, buffer, &l) == -1 )
		{
			perror("Unable to forward LEAVE message to eth0 interface.\n");
			exit(2);
		}
	}
}
void quitHandle(char *buffer, int len)
{
	leave(buffer, len);
	LLNode *lln = config->peersList->head;
	for (; lln != NULL; lln = lln->next) {
		Peer *peer = (Peer *)lln->data;
		int l = len;
		if ( sendall(peer->sock, buffer, &l) == -1 )
		{
			perror("Unable to forward QUIT message to eth0 interface.\n");
			exit(2);
		}
		closePeer(peer);
	}
	freeConfig();
	exit(0);
}

//is mac address my own?
int isSelf(uint8_t *mac)
{
	uint8_t *mine = config->peer->tapMac;
	if (mac[0]==mine[0] && mac[1]==mine[1] && mac[2]==mine[2] && mac[3]==mine[3] && mac[4]==mine[4] && mac[5]==mine[5])
		return 1; //true
	else
		return 0; //false
}

void linkHandle(char *buffer, int len, Peer *peer)
{
	unsigned long long *int64 = (unsigned long long *)(buffer + 26);
	uint16_t *int16 = (uint16_t *)(buffer+24);
	uint8_t *int8 = (uint8_t *)buffer;
	int i;

	uint8_t sIP[IP_SIZE]; //source ip
	uint16_t sPort; //source port
	uint8_t sTapMac[MAC_SIZE]; //source tap mac address
	uint8_t sEthMac[MAC_SIZE]; //source eth mac address
	int numEdges = ntohs(*int16); // number of edges


	int8 = (uint8_t *)(buffer+6);
	for (i = 0; i < IP_SIZE; i++)
		sIP[i] = *(int8 + i); // source listen IP
	int16 = (uint16_t *)(buffer + 10);
	sPort = ntohs(*int16); // source listen port
	int8 = (uint8_t *)(buffer+12);
	for (i = 0; i < MAC_SIZE; i++)
		sTapMac[i] = *(int8 + i); // source tap mac
	int8 = (uint8_t *)(buffer+18);
	for (i = 0; i < MAC_SIZE; i++)
		sEthMac[i] = *(int8 + i); // source eth mac
	int8 = (uint8_t *)buffer;

	//determine if initial link state
	if (peer->port == -1 && numEdges == 1) {
		peer->port = sPort;
		for (i = 0; i < MAC_SIZE; i++) {
			peer->tapMac[i] = sTapMac[i];
			peer->ethMac[i] = sEthMac[i];
		}
		for (i = 0; i < IP_SIZE; i++)
			peer->ip[i] = sIP[i];
		
		printf("Creating edge from init! port1: %d, port2: %d", config->peer->port, peer->port);
		//create an edge
		Edge *edge = (Edge *) malloc(sizeof(Edge));
		edge->id = *int64;
		edge->peer1 = config->peer;
		edge->peer2 = peer;
		edge->weight = 1; /************************** change for part 3 ********/
		LLappend(config->edgeList, edge);
		sendInitState(peer);
	} else if (numEdges == 1 && buffer[56] == '\0')
		return;
	else {
		Peer *peer1 = NULL, *peer2 = NULL;
		uint8_t p_ip[IP_SIZE]; //source ip
		uint16_t p_port; //source port
		uint8_t p_tapMac[MAC_SIZE]; //source tap mac address
		uint8_t p_ethMac[MAC_SIZE]; //source eth mac address
		int e, offset;
		for (e = 0, offset = 26; e < numEdges; e++, offset+=48) {
			int64 = (unsigned long long *)(buffer+offset);
			printf("ID: %lld\n", *int64);
			//check if connected to peer 1
			int8 = (uint8_t *)(buffer+offset+14);
			Peer *h = NULL;
			for (i = 0; i < MAC_SIZE; i++)
				p_tapMac[i] = *(int8 + i); // peer 1 tap mac
	char x[12];
	macntoh(p_tapMac, x);
	printf("1 MAC: %s\n", x);
			h = findPeer(p_tapMac);
			if (h == NULL) {
				if (isSelf(p_tapMac) == 1) {
					h = config->peer;
				} else {
					int8 = (uint8_t *)(buffer+offset+8);
					for (i = 0; i < IP_SIZE; i++)
						p_ip[i] = *(int8 + i); // peer 1 listen IP
					int16 = (uint16_t *)(buffer + offset+12);
					p_port = ntohs(*int16); // peer 1 listen port
					Peer *newpeer = (Peer *) malloc(sizeof(Peer));
					char newip[16];
					ipntoh(p_ip, newip);
		printf("1   %s\n", newip);
					newpeer->host = strdup(newip);
					newpeer->port = p_port;
					int sock = vpnconnect(newpeer);
					if (sock != -1) { //successful connection!
						FD_SET(sock, &(config->masterFDSET));
						if (sock > config->fdMax)
							config->fdMax = sock;
						LLappend(config->peersList, newpeer);
						sendInitState(newpeer);
					} else
						freePeer(newpeer);
				}
			} else
				peer1 = h;

			h = NULL;
			//check if connected to peer 2
			int8 = (uint8_t *)(buffer+offset+32);
			for (i = 0; i < MAC_SIZE; i++)
				p_tapMac[i] = *(int8 + i); // peer 1 tap mac
	char y[12];
macntoh(p_tapMac, y);
	printf("2 MAC: %s\n", y);
			h = findPeer(p_tapMac);
			if (h == NULL) {
				if (isSelf(p_tapMac) == 1) {
					h = config->peer;
				} else {
					int8 = (uint8_t *)(buffer+offset+26);
					for (i = 0; i < IP_SIZE; i++)
						p_ip[i] = *(int8 + i); // peer 1 listen IP
					int16 = (uint16_t *)(buffer + offset+30);
					p_port = ntohs(*int16); // peer 1 listen port
					Peer *newpeer = (Peer *) malloc(sizeof(Peer));
					char newip[16];
					ipntoh(p_ip, newip);
		printf("2   %s\n", newip);
					newpeer->host = strdup(newip);
					newpeer->port = p_port;
					int sock = vpnconnect(newpeer);
					if (sock != -1) { //successful connection!
						FD_SET(sock, &(config->masterFDSET));
						if (sock > config->fdMax)
							config->fdMax = sock;
						LLappend(config->peersList, newpeer);
						sendInitState(newpeer);
					} else
						freePeer(newpeer);
				}
			} else
				peer2 = h;

			if (peer1 != NULL && peer2 != NULL) { //connected to both peers, update edge
				Edge *edge = getEdge(peer1, peer2);
				if (edge == NULL) {
					printf("Edge is not there, but should be... Adding edge");
					edge = (Edge *) malloc(sizeof(Edge));
					edge->id = *int64;
					edge->peer1 = peer1;
					edge->peer2 = peer2;
					edge->weight = 1; /************** NEED TO CHANGE PART 3 ***/
				} else { //update ID if newer, else discard
					if (edge->id < *int64)
						edge->id = *int64;
				}
			}
		}
	}
}
