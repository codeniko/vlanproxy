#include "vpnproxy.h"

static char *TERM_MESSAGE = "YOYOYO -- Time To Close Connection -- TERMINATE";

//Structure holding mode of local machine, File Descriptors, and sock address
struct ProxyInfo
{
	int mode;
	int connectionFD;
	int tapFD;
	int activeConnection;
	struct sockaddr_in sockaddr;
};

//Print usage of program if arguments are incorrect.
void printHelp()
{
	printf("Usage: \nServer: vpnproxy <port> <local interface>\nClient: vpnproxy <remote host> <remote port> <local interface>");
}

//Signal handler to gracefully exit
void handle_signal(int sig)
{
	//Write a special message to remote proxy to terminate if TERM-like Signal and active connection
	if (sig != -1 && proxyinfo->activeConnection == 1) 
	{
		char buffer[BUFFER_SIZE];
		uint16_t *p_tag = (uint16_t *)buffer; //ptr to space in buffer holding tag
		uint16_t *p_length = ((uint16_t *)buffer)+1; //ptr to space in buffer holding length of message
		*p_tag = htons(VLAN_TAG);
		*p_length = htons(strlen(TERM_MESSAGE)+1);
		bcopy(TERM_MESSAGE, buffer+HEADER_SIZE, strlen(TERM_MESSAGE)+1+HEADER_SIZE);
		write(proxyinfo->connectionFD, buffer, strlen(TERM_MESSAGE)+1+HEADER_SIZE);
	}

	if (proxyinfo->activeConnection == 1)
	{
		pthread_cancel(thread_public);
		pthread_cancel(thread_private);
	}
	printf("Connection has been terminated!\n");
	close(proxyinfo->connectionFD);
	close(proxyinfo->tapFD);
	free(proxyinfo);
	exit(0);
}


/**************************************************
 * allocate_tunnel:
 * open a tun or tap device and returns the file
 * descriptor to read/write back to the caller
 *****************************************/
int allocate_tunnel(char *dev, int flags) 
{
	int fd, error;
	struct ifreq ifr;
	char *device_name = "/dev/net/tun";
	if( (fd = open(device_name , O_RDWR)) < 0 ) {
		perror("error opening /dev/net/tun.\n");
		return fd;
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = flags;
	if (*dev) {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}
	if( (error = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
		perror("ioctl on tap failed.\n");
		close(fd);
		return error;
	}
	strcpy(dev, ifr.ifr_name);
	return fd;
}

//Create active connection
//RETURNS: File Descriptor on success, -1 on failure.
int createConnection()
{
	if (proxyinfo->mode == MODE_SERVER)
	{
		//Assigning a name to the socket
		if (bind(proxyinfo->connectionFD, (struct sockaddr *) (&(proxyinfo->sockaddr)), sizeof(proxyinfo->sockaddr)) < 0)
		{
			perror("Server binding failed.\n");
			return -1;
		}

		//Set the number of allowed connections in the queue
		if (listen(proxyinfo->connectionFD, MAX_CONNECTIONS) < 0)
		{
			perror("Server listening failed. \n");
			return -1;
		}

		//Accept connection if server
		int connectFD = accept(proxyinfo->connectionFD, NULL, NULL);
		if (connectFD < 0)
		{
			perror("Server unable to accept connection. \n");
			return -1;
		}

		//replace socketFD that was initially opened with active connection FD
		close(proxyinfo->connectionFD);
		proxyinfo->connectionFD = connectFD;
		proxyinfo->activeConnection = 1;
		return 0;
	} else if (proxyinfo->mode == MODE_CLIENT)
	{
		//Connect to server if client
		if (connect(proxyinfo->connectionFD, (struct sockaddr *) &(proxyinfo->sockaddr), sizeof(proxyinfo->sockaddr)) < 0)
		{
			perror("Connection to server has failed. \n");
			return -1;
		}

		proxyinfo->activeConnection = 1;
		return 0;
	}
	return -1;
}

//Handle for public interface - receive encapsulated, send decapsulated to private interface
void *handle_public(void *arg)
{
	char buffer[BUFFER_SIZE];
	uint16_t *p_tag = (uint16_t *)buffer; //ptr to space in buffer holding tag
	uint16_t *p_length = ((uint16_t *)buffer)+1; //ptr to space in buffer holding length of message
	int const TERM_MESSAGE_LENGTH = strlen(TERM_MESSAGE)+1;

	int readOffset = 0;
	int tryRead = 1;
	while(1 && proxyinfo->activeConnection == 1) 
	{
		int bytesRead=readOffset;
		if (tryRead)
		{
			bytesRead += read(proxyinfo->connectionFD, buffer+readOffset, BUFFER_SIZE-readOffset);
		}

		tryRead = 1;

		// If read < 0, then error - NOTE* bytesRead will always have atleast HEADER_SIZE
		if (bytesRead < readOffset) 
		{
			fprintf(stderr,"Failed to receive message from socket.\n");
			return NULL;
		}

		int msgLength = ntohs(*p_length);
		if (msgLength > BUFFER_SIZE) //If msg bigger than buffer, write buffer and will then carry extra over to next write
		{
			msgLength = BUFFER_SIZE;
		}

		// Check if we received the TERM_MESSAGE from remote proxy to close connection
		if (ntohs(*p_tag) == VLAN_TAG && msgLength == TERM_MESSAGE_LENGTH && bytesRead > HEADER_SIZE)
		{
			if (strncmp(buffer+HEADER_SIZE, TERM_MESSAGE, TERM_MESSAGE_LENGTH) == 0)
			{
				handle_signal(-1);
			}
		}

		// Confirm vlan tag is correct
		if (ntohs(*p_tag) != VLAN_TAG && bytesRead >= HEADER_SIZE) 
		{
			fprintf(stderr,"Received an unknown message. Packet might have been lost or corrupted. Dropping packet.\n");
			readOffset = 0;
			continue;
		}

		// Make sure we have received the whole message, bytesRead should equal msgLength. If lower, message is incomplete.
		if (bytesRead < HEADER_SIZE || bytesRead < msgLength) 
		{
			readOffset = bytesRead;
			continue;
		}

		if( write(proxyinfo->tapFD, buffer+HEADER_SIZE, msgLength-HEADER_SIZE) < 0 ) 
		{
			fprintf(stderr,"Unable to write to private interface.\n");
			return NULL;
		}

		readOffset = 0;

		//If more bytes were read than the message, copy the extra to beginning of buffer.
		if(bytesRead > msgLength) 
		{
			bcopy(buffer+msgLength, buffer, bytesRead-msgLength);
			readOffset = bytesRead-msgLength;
			tryRead = 0;
		}
	}

	return NULL;
}

//Handle for private interface - receive decapsulated, send encapsulated to public interface
void *handle_private(void *arg)
{
	char buffer[BUFFER_SIZE];
	uint16_t *p_tag = (uint16_t *)buffer; //ptr to space in buffer holding tag
	uint16_t *p_length = ((uint16_t *)buffer)+1; //ptr to space in buffer holding length of message

	*p_tag = htons(VLAN_TAG); 

	while(1 && proxyinfo->activeConnection == 1) 
	{
		int bytesRead = read(proxyinfo->tapFD, buffer+HEADER_SIZE, BUFFER_SIZE-HEADER_SIZE)+HEADER_SIZE;

		// If read < 0, then error - NOTE* bytesRead will always have atleast HEADER_SIZE
		if (bytesRead < HEADER_SIZE) 
		{ 
			fprintf(stderr,"Failed to receive message from private interface.\n");
			return NULL;
		}

		*p_length = htons(bytesRead);

		if (write(proxyinfo->connectionFD, buffer, bytesRead) < 0) 
		{
			fprintf(stderr,"Failed to send message over socket.\n");
			return NULL;
		}
	}

	return NULL;
}

// returns negative # on fail, FD on success
int createSocket(char *host, int port)
{
	struct sockaddr_in to; /* remote internet address */
	struct hostent *hp = NULL; /* remote host info from gethostbyname() */

	memset(&to, 0, sizeof(to));

	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	if (host == NULL)
	{
		to.sin_addr.s_addr = INADDR_ANY;
	} else
	{
		/* If internet "a.d.c.d" address is specified, use inet_addr()
		 * to convert it into real address. If host name is specified,
		 * use gethostbyname() to resolve its address */
		to.sin_addr.s_addr = inet_addr(host); /* If "a.b.c.d" addr */
		if (to.sin_addr.s_addr == -1)
		{
			hp = gethostbyname(host); //hostname in form of text
			if (hp == NULL) {
				fprintf(stderr, "Host name %s not found\n", host);
				return -1;
			}
			bcopy((char *)hp->h_addr, (char *)&(to.sin_addr.s_addr), hp->h_length);
		}
	}


	int connectionFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connectionFD == -1)
	{
		perror("Failed to create socket.\n");
		return -1;
	}

	int optval = 1;
	/* avoid EADDRINUSE error on bind() */
	if (setsockopt(connectionFD, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0) {
		perror("Failed to set socket options.\n");
		return -1;
	}

	memset(&(proxyinfo->sockaddr), 0, sizeof(proxyinfo->sockaddr));
	proxyinfo->sockaddr = to;
	return connectionFD;
}



int main(int argc, char **argv)
{
	proxyinfo = (struct ProxyInfo *)malloc(sizeof(struct ProxyInfo));
	int port;
	char *host;
	char *tap;

	//set up signal handler if proxy is killed
	signal(SIGHUP, handle_signal);
	signal(SIGINT, handle_signal);
	signal(SIGPIPE, handle_signal);
	signal(SIGALRM, handle_signal);
	signal(SIGTERM, handle_signal);
	signal(SIGUSR1, handle_signal);
	signal(SIGUSR2, handle_signal);

	proxyinfo->activeConnection = 0;
	if (argc == 3) // SERVER
	{
		proxyinfo->mode = MODE_SERVER;
		host = NULL;
		port = (int)atoi(argv[1]);
		tap = argv[2];
	} else if (argc == 4) // CLIENT
	{
		proxyinfo->mode = MODE_CLIENT;
		host = argv[1];
		port = (int)atoi(argv[2]);
		tap = argv[3];
	} else
	{
		printHelp();
		return 1;
	}

	if ( (proxyinfo->connectionFD = createSocket(host, port) ) == -1 )
	{
		perror("Creating an endpoint for communication failed! \n");
		return 1;
	}

	if ( (proxyinfo->tapFD = allocate_tunnel(tap, IFF_TAP | IFF_NO_PI)) < 0 ) 
	{
		perror("Opening tap interface failed! \n");
		return 1;
	}

	if (proxyinfo->mode == MODE_SERVER)
	{
		printf("Waiting for a client to connect!\n");
	}

	if (createConnection() == -1)
	{
		return 1;
	}

	printf("Connection has been established!\n");


	pthread_create(&thread_public,NULL,&handle_public, NULL);
	pthread_create(&thread_private,NULL,&handle_private, NULL);
	pthread_join(thread_public,NULL);
	pthread_join(thread_private,NULL);

	close(proxyinfo->connectionFD);
	close(proxyinfo->tapFD);
	free(proxyinfo);
	return 0;
}
