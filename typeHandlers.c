#include "typeHandlers.h"

//len is full length, including header

int createStateMessage(char **buffer, Peer *peer, int init)
{ //NOTE need the number of neighbors
	int i, bufsize;
	int numEdges = config->edgeList->size;
	if (init == 1)
		bufsize = 74;
	else 
		bufsize = 26+48*numEdges;
	*buffer = (char *)malloc(sizeof(char)*bufsize);
	if (*buffer == NULL) {
		fprintf(stderr, "Fatal Error: malloc has failed.");
		exit(1);
	}
	memcpy(*buffer, '\0', bufsize);
	unsigned long long *int64 = (*buffer) + 26;
	uint32_t *int32 = (*buffer);
	uint16_t *int16 = (*buffer);
	uint8_t *int8 = (*buffer);
	*int16  = htons(TYPE_LINKSTATE); //MESSAGE TYPE, 2 bytes
	int8 = (*buffer)+6;
	for (i = 0; i < IP_SIZE; i++)
		*(int8 + i) = (config->ip)[i]; // source listen IP
	int16 = (*buffer) + 10;
	*int16 = htons(config->port); // source listen port
	int8 = (*buffer)+12;
	for (i = 0; i < MAC_SIZE; i++)
		*(int8 + i) = (config->tapMac)[i]; // source tap mac
	int8 = (*buffer)+18;
	for (i = 0; i < MAC_SIZE; i++)
		*(int8 + i) = (config->ethMac)[i]; // source eth mac

	int16 = (*buffer)+2;
	//generate neighbor list
	if (init == 1) // create initial connection link state, single record
	{
		*int16 = htons(70); //MESSAGE LENGTH, 2 bytes
		*(int16+1) = htons(1); // # of edges, 2 bytes
		int16 = (*buffer)+24;
		*int16 = htons(1); // # of edges, 2 bytes

		*int64 = htobe64(genID()); // unique ID for edge record
		int8 = (*buffer)+34;
		for (i = 0; i < IP_SIZE; i++)
			*(int8 + i) = (config->ip)[i]; // source listen IP
		int16 = (*buffer) + 38;
		*int16 = htons(config->port); // source listen port
		int8 = (*buffer)+40;
		for (i = 0; i < MAC_SIZE; i++)
			*(int8 + i) = (config->tapMac)[i]; // source tap mac
		int8 = (*buffer)+46;
		for (i = 0; i < MAC_SIZE; i++)
			*(int8 + i) = (config->ethMac)[i]; // source eth mac
		int32 = (*buffer)+70;
		*int32 = htonl(1); //link weight
	} else {
		int k;
		*int16 = htons(bufsize-HEADER_SIZE); //MESSAGE LENGTH, 2 bytes
		*(int16+1) = htons(numEdges); // # of edges, 2 bytes
		int16 = (*buffer)+24;
		*int16 = htons(numEdges); // # of edges, 2 bytes

		LLNode *edgeNode = config->edgeList->head;
		for (k = 26; k < bufsize; k+=48, edgeNode=edgeNode->next) {
			Edge *edge = (Edge *)(edgeNode->data);
			int64 = (*buffer)+k;
			*int64 = htobe64(genID()); // unique ID for edge record
			int8 = (*buffer)+k+8;
			for (i = 0; i < IP_SIZE; i++)
				*(int8 + i) = (edge->peer1->ip)[i]; // peer 1 listen IP
			int16 = (*buffer) + k + 12;
			*int16 = htons(edge->peer1->port); // peer 1 listen port
			int8 = (*buffer)+ k + 14;
			for (i = 0; i < MAC_SIZE; i++)
				*(int8 + i) = (edge->peer1->tapMac)[i]; // peer 1 tap mac
			int8 = (*buffer)+k+20;
			for (i = 0; i < MAC_SIZE; i++)
				*(int8 + i) = (edge->peer1->ethMac)[i]; // peer 1 eth mac
			int8 = (*buffer)+k+26;
			for (i = 0; i < IP_SIZE; i++)
				*(int8 + i) = (edge->peer1->ip)[i]; // peer 2 listen IP
			int16 = (*buffer) + k + 30;
			*int16 = htons(edge->peer1->port); // peer 2 listen port
			int8 = (*buffer)+ k + 32;
			for (i = 0; i < MAC_SIZE; i++)
				*(int8 + i) = (edge->peer1->tapMac)[i]; // peer 2 tap mac
			int8 = (*buffer)+k+38;
			for (i = 0; i < MAC_SIZE; i++)
				*(int8 + i) = (edge->peer1->ethMac)[i]; // peer 2 eth mac
			int32 = (*buffer)+k+44;
			*int32 = htonl(1); //Link weight
		}
	}
	return bufsize;
}

void sendInitState(Peer *peer)
{
	char *buffer = NULL;
	int bufsize = createStateMessage(&buffer, peer, 1);
	if (sendall(peer->sock, buffer, &bufsize) == -1) {
		perror("Failed to send INIT STATE");
		fprintf(stderr, "#bytes left to send: %d", bufsize);
		exit(1);
	}
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
	if ( sendall(config->tapFD, buffer+HEADER_SIZE, len-HEADER_SIZE) == 0 )
	{
		fprintf(stderr,"Unable to write to private interface.\n");
		exit(2);
	}
}
void leaveHandle(char *buffer, int len)
{
	if ( sendall(config->tapFD, buffer+HEADER_SIZE, len-HEADER_SIZE) == 0 )
	{
		fprintf(stderr,"Unable to write to private interface.\n");
		exit(2);
	}
}
void quitHandle(char *buffer, int len)
{
	if ( sendall(config->tapFD, buffer+HEADER_SIZE, len-HEADER_SIZE) == 0 )
	{
		fprintf(stderr,"Unable to write to private interface.\n");
		exit(2);
	}
}
void linkHandle(char *buffer, int len)
{
	if ( sendall(config->tapFD, buffer+HEADER_SIZE, len-HEADER_SIZE) == 0 )
	{
		fprintf(stderr,"Unable to write to private interface.\n");
		exit(2);
	}
}
