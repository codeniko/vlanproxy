#include "typeHandlers.h"

//len is full length, including header

char *createStateMessage(Peer *peer, int init)
{ //NOTE need the number of neighbors
	int i;
	unsigned char buffer[HEADER_SIZE+70];
	unsigned long long *int64 = buffer + 26;
	uint32_t *int32 = buffer;
	uint16_t *int16 = buffer;
	uint8_t *int8 = buffer;
	*int16  = htons(TYPE_LINKSTATE); //MESSAGE TYPE, 2 bytes
	*(int16+1) = htons(70); //MESSAGE LENGTH, 2 bytes
	int8 = buffer+6;
	for (i = 0; i < IP_SIZE; i++)
		*(int8 + i) = (config->ip)[i]; // source listen IP
	int16 = buffer + 10;
	*int16 = htons(config->port); // source listen port
	int8 = buffer+12;
	for (i = 0; i < MAC_SIZE; i++)
		*(int8 + i) = (config->tapMac)[i]; // source tap mac
	int8 = buffer+18;
	for (i = 0; i < MAC_SIZE; i++)
		*(int8 + i) = (config->ethMac)[i]; // source eth mac
	
	//generate neighbor list
	if (init == 1) // create initial connection link state, single record
	{
		int16 = buffer+4;
		*int16 = htons(1); // # of neighbors, 2 bytes
		int16 = buffer+24;
		*int16 = htons(1); // # of neighbors, 2 bytes

		*int64 = htobe64(genID()); // unique ID for edge record
		int8 = buffer+34;
		for (i = 0; i < IP_SIZE; i++)
			*(int8 + i) = (config->ip)[i]; // source listen IP
		int16 = buffer + 38;
		*int16 = htons(config->port); // source listen port
		int8 = buffer+40;
		for (i = 0; i < MAC_SIZE; i++)
			*(int8 + i) = (config->tapMac)[i]; // source tap mac
		int8 = buffer+46;
		for (i = 0; i < MAC_SIZE; i++)
			*(int8 + i) = (config->ethMac)[i]; // source eth mac
		memcpy(buffer+52, '\0', 18);
		int32 = buffer+70;
		*int32 = htonl(1);
	} else {
		int16 = buffer+4;
		*int16 = htons(1); // # of neighbors, 2 bytes
		int16 = buffer+24;
		*int16 = htons(1); // # of neighbors, 2 bytes

		*int64 = htobe64(genID()); // unique ID for edge record
		int8 = buffer+34;
		for (i = 0; i < IP_SIZE; i++)
			*(int8 + i) = (config->ip)[i]; // source listen IP
		int16 = buffer + 38;
		*int16 = htons(config->port); // source listen port
		int8 = buffer+40;
		for (i = 0; i < MAC_SIZE; i++)
			*(int8 + i) = (config->tapMac)[i]; // source tap mac
		int8 = buffer+46;
		for (i = 0; i < MAC_SIZE; i++)
			*(int8 + i) = (config->ethMac)[i]; // source eth mac
		memcpy(buffer+52, '\0', 18);
		int32 = buffer+70;
		*int32 = htonl(1);
		
	}
}

void sendInitState(Peer *peer)
{
	
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
