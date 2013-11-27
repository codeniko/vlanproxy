#include "threadHandlers.h"
//Signal handler to gracefully exit on any termination signal. Will cause the remote proxy to terminate as well
/*void handle_signal(int sig)
  {
//Write a special message to remote proxy to terminate if TERM-like Signal and active connection
if (sig != -1 && config->filedesc->activeConnection == 1) 
{
char buffer[BUFFER_SIZE];
uint16_t *p_tag = (uint16_t *)buffer; //ptr to space in buffer holding tag
uint16_t *p_length = ((uint16_t *)buffer)+1; //ptr to space in buffer holding length of message
 *p_tag = htons(TYPE_DATA);
 *p_length = htons(strlen(TERM_MESSAGE)+1);
 bcopy(TERM_MESSAGE, buffer+HEADER_SIZE, strlen(TERM_MESSAGE)+1+HEADER_SIZE);
 write(config->filedesc->connectionFD, buffer, strlen(TERM_MESSAGE)+1+HEADER_SIZE);
 }

 if (config->filedesc->activeConnection == 1)
 {
 pthread_cancel(thread_public);
 pthread_cancel(thread_private);
 }
 printf("Connection has been terminated!\n");
 close(config->filedesc->connectionFD);
 close(config->filedesc->tapFD);
 free(config->filedesc);
 exit(0);
 }
 */

void *vpnlisten(void *args)
{
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;
	char remoteIP[INET_ADDRSTRLEN];
	struct sockaddr_in sockaddr;

	int listensock = createListenSocket(&sockaddr);
	if (listensock == -1)
		exit(1);
	int optval = 1;
	setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
	//Assigning a name to the socket
	if (bind(listensock, (struct sockaddr *) (&sockaddr), sizeof sockaddr) < 0)
	{
		perror("Server binding failed.\n");
		exit(1);
	}

	//Set the number of allowed connections in the queue
	if (listen(listensock, MAX_CONNECTIONS) < 0)
	{
		perror("Server listening failed. \n");
		exit(1);
	}

	while (1)
	{
		//Accept connection if server
		int sock = accept(listensock, (struct sockaddr *)&remoteaddr, &addrlen); //active, connected socket created
		if (sock < 0)
		{
			perror("Server unable to accept connection. \n");
			continue;
		}
		printf("Accepted incoming connection from %s on socket %d\n", inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET_ADDRSTRLEN), sock);

		//replace socketFD that was initially opened with active connection FD
		//close(config->filedesc->connectionFD);
		Peer *peer = (Peer *) malloc(sizeof(Peer));
		peer->host = strdup(remoteIP);
		peer->sock = sock;
		peer->port = -1; //will be set after link state message
		LLappend(config->peersList, peer); //add to list of peers connected to
		//FD_SET(sock, &(config->masterFDSET));
	}
}

//Handle for public interface - receive encapsulated, send decapsulated to private interface
void *handle_public(void *arg)
{
	char buffer[BUFFER_SIZE];
	uint16_t *p_type = (uint16_t *)buffer; //ptr to space in buffer holding message type
	uint16_t *p_length = ((uint16_t *)buffer)+1; //ptr to space in buffer holding length of message
	//int const TERM_MESSAGE_LENGTH = strlen(TERM_MESSAGE)+1;

	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	while(1) 
	{ //over, here, how to add select? maybe inner infinite while loop
		FD_ZERO(&(config->readFDSET));
		config->readFDSET = config->masterFDSET;
		int sRes = select(config->fdMax+1, &(config->readFDSET), NULL, NULL, &tv);
		if ( sRes == -1) {
			perror("select");
			exit(4);
		}
		if (sRes == 0) { //timeout, update FDSET
			printf("Select Timeout - Updating FDSET\n");
			continue;
		}
		int sock = -1;
		int i = 0;
		for (; i <= config->fdMax; i++) {
			if (FD_ISSET(i, &(config->readFDSET))) { // we got one!!
				sock = i;
				printf("Reading from public socket %d\n", sock);
				break;
			}
		}
		if (sock == -1)
			continue;

		int readOffset = 0;
		int tryRead = 1;
		while(1)
		{
			int bytesRead=readOffset;
			if (tryRead)
				bytesRead += read(sock, buffer+readOffset, BUFFER_SIZE-readOffset);

			tryRead = 1;

			// If read < 0, then error - NOTE* bytesRead will always have atleast HEADER_SIZE
			if (bytesRead < readOffset) {
				fprintf(stderr,"Failed to receive message from socket.\n");
				exit(1);
			}

			int msgLength = ntohs(*p_length);
			//will never happen********************
	//		if (msgLength > BUFFER_SIZE) //If msg bigger than buffer, write buffer and will then carry extra over to next write
	//			msgLength = BUFFER_SIZE;

			/*		// Check if we received the TERM_MESSAGE from remote proxy to close connection
					if (ntohs(*p_tag) == TYPE_DATA && msgLength == TERM_MESSAGE_LENGTH && bytesRead > HEADER_SIZE) {
					if (strncmp(buffer+HEADER_SIZE, TERM_MESSAGE, TERM_MESSAGE_LENGTH) == 0)
					handle_signal(-1);
					}*/

			// Make sure we have received the whole message, bytesRead should equal msgLength. If lower, message is incomplete.
			if (bytesRead < HEADER_SIZE || bytesRead < msgLength) {
				readOffset = bytesRead;
				continue;
			}
			// IF WE GET THIS FAR, ENTIRE MESSAGE (MAYBE MORE) IS IN BUFFER

			int msgType = ntohs(*p_type);
			if (msgType == TYPE_DATA) {
				dataHandle(buffer, msgLength+HEADER_SIZE);
			} else if (msgType == TYPE_LEAVE) {
				leaveHandle(buffer, msgLength+HEADER_SIZE);
			} else if (msgType == TYPE_QUIT) {
				quitHandle(buffer, msgLength+HEADER_SIZE);
			} else if (msgType == TYPE_LINKSTATE) {
				linkHandle(buffer, msgLength+HEADER_SIZE, getPeer(sock));
			} else {
				fprintf(stderr,"Received an unknown message. Packet might have been lost or corrupted. Dropping packet.\n");
				readOffset = 0;
				exit(2);
			}

			readOffset = 0;
			//If more bytes were read than the message, copy the extra to beginning of buffer.
			if(bytesRead > msgLength) 
			{
				memcpy(buffer, buffer+HEADER_SIZE+msgLength, bytesRead-msgLength-HEADER_SIZE);
				readOffset = bytesRead-msgLength-HEADER_SIZE;
				tryRead = 0;
			}

			if (tryRead == 1 && readOffset == 0) // done reading, break and listen FD
				break;
		}
	}

	return NULL;
}

//Handle for private interface - receive decapsulated, send encapsulated to public interface
void *handle_private(void *arg)
{
	char buffer[BUFFER_SIZE];
	uint16_t *p_type = (uint16_t *)buffer; //ptr to space in buffer holding tag
	uint16_t *p_length = ((uint16_t *)buffer)+1; //ptr to space in buffer holding length of message

	*p_type = htons(TYPE_DATA); 

	while(1) 
	{
		int bytesRead = read(config->tapFD, buffer+HEADER_SIZE, BUFFER_SIZE-HEADER_SIZE)+HEADER_SIZE;
		*p_length = htons(bytesRead);

		struct ethhdr *ether = (struct ethhdr *)(buffer+HEADER_SIZE);
		Peer *h = findPeer((uint8_t *)(ether->h_dest));
		if (h == NULL)
			continue; //layer 2 address not found, ignore message
		printf("---MAC address found\n");

		if (write(h->sock, buffer, bytesRead) < 0) 
		{
			fprintf(stderr,"Failed to send message over socket.\n");
			return NULL;
		}
	}

	return NULL;
}

void handle_main()
{
	int delay_timeout = config->linkTimeout;
	int delay_sendstate = config->linkPeriod;
	unsigned int last_timeout; //timestamp for timeout
	unsigned int last_sendstate; //timestamp for sendstate 
	struct timeval tv;
	gettimeofday(&tv, NULL);
	last_timeout = tv.tv_sec;
	last_sendstate = tv.tv_sec;
	while(1)
	{
		gettimeofday(&tv, NULL);
		unsigned int time = tv.tv_sec;
		if (last_timeout + delay_timeout < time) {
			printf("Time to check membership timeouts!\n");
			LLNode *lln = (LLNode *) config->edgeList->head;
			for (; lln != NULL; lln = lln->next) {
				Edge *edge = (Edge *) lln->data;
				unsigned long long id = edge->id / 1000000;
				if (id+delay_timeout < time) {
					printf("A member has timed out...\n");
					LLremove(config->edgeList, edge);
					free(edge);
				}
			}
			
			last_timeout = time;
		}
		if (last_sendstate + delay_sendstate < time) {
			printf("Time to send link states!\n");
			char *buffer = NULL;
			int bufsize = createStateMessage(&buffer, 0);
			if (bufsize > 0) {
				LLNode *lln = (LLNode *) config->peersList->head;
				for (; lln != NULL; lln = lln->next) {
					Peer *peer = (Peer *) lln->data;
					sendallstates(peer->sock, buffer, &bufsize);
				}
			}
			last_sendstate = time;
		}

		unsigned int a = last_timeout + delay_timeout;
		unsigned int b = last_sendstate + delay_sendstate;
		if (a < b)
			sleep(last_timeout+delay_timeout-time);
		else
			sleep(last_sendstate+delay_sendstate-time);
	}
}
