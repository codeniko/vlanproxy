#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include <netdb.h>
#include "dbg.h"
#include <signal.h>

#define MODE_SERVER 		0
#define MODE_CLIENT 		1
#define MAX_CONNECTIONS		1
#define HEADER_FIELD_SIZE	2
#define HEADER_SIZE			4
#define BUFFER_SIZE			2048

static pthread_t thread_public;
static pthread_t thread_private;

struct ProxyInfo;
void printHelp();
void closeFDs(struct ProxyInfo *proxyinfo);
int allocate_tunnel(char *dev, int flags);
int createConnection(struct ProxyInfo *proxyinfo);
void *threadTCP(void *arg);
void *threadTAP(void *arg);
int createSocket(char *host, int port, struct ProxyInfo *proxyinfo);
void handle_signal(int sig);

//Structure holding mode of local machine, File Descriptors, and sock address
struct ProxyInfo
{
	int mode;
	int connectionFD;
	int tapFD;
	struct sockaddr_in sockaddr;
};

//Print usage of program if arguments are incorrect.
void printHelp()
{
	printf("Usage: \nServer: vpnproxy <port> <local interface>\nClient: vpnproxy <remote host> <remote port> <local interface>");
}

//Signal handler to kill threads.
void handle_signal(int sig)
{
	pthread_cancel(thread_public);
	pthread_cancel(thread_private);
}

//Close local File Descriptors and exit if remote proxy was terminated
void closeFDs(struct ProxyInfo *proxyinfo)
{
	handle_signal(2);
	close(proxyinfo->connectionFD);
	close(proxyinfo->tapFD);
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
int createConnection(struct ProxyInfo *proxyinfo)
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

		debug("SERVER: before connection accept");
		//Accept connection if server
		int connectFD = accept(proxyinfo->connectionFD, NULL, NULL);
		debug("SERVER: after connection accept");
		if (connectFD < 0)
		{
			perror("Server unable to accept connection. \n");
			return -1;
		}

		//replace socketFD that was initially opened with active connection FD
		close(proxyinfo->connectionFD);
		proxyinfo->connectionFD = connectFD;
		return 0;
	} else if (proxyinfo->mode == MODE_CLIENT)
	{
		debug("Client before connection");
		//Connect to server if client
		if (connect(proxyinfo->connectionFD, (struct sockaddr *) &(proxyinfo->sockaddr), sizeof(proxyinfo->sockaddr)) < 0)
		{
			perror("Connection to server has failed. \n");
			return -1;
		}
		debug("Client after connection");

		return 0;
	}
	return -1;
}

//Handle for public interface - receive encapsulated, send decapsulated to private interface
void *handle_public(void *arg)
{
	struct ProxyInfo *proxyinfo = (struct ProxyInfo *)arg;
	char buffer[BUFFER_SIZE];
	int16_t *p_type = (int16_t *)buffer; //pointer to space in buffer holding type
	uint16_t *p_length = ((uint16_t *)buffer)+1; //pointer to space in buffer holding length of message

	if (proxyinfo->mode == MODE_SERVER || proxyinfo->mode == MODE_CLIENT)
	{
		//Perform read/write to TAP interface for active connection indefinitely
		while (1)
		{
			log_info("CLIENT: Loop restart");
			//Read data from TCP and store number of bytes read
			int bytesRead = read(proxyinfo->connectionFD, buffer, BUFFER_SIZE);
			//Get length of data from header
			int length = ntohs(*p_length);
			log_info("Length of packet is %d", length);
			if (bytesRead == 0 || length == 0)
			{
				fprintf(stderr, "Connection was closed. Exiting proxy.\n");
				closeFDs(proxyinfo);
				return NULL;
			}

			//Get type from header and check if equal to random 0xABCD
			int type = ntohs(*p_type);
			if (type != 0xABCD)
			{
				fprintf(stderr, "Client received wrong type. Dropping this message. Size of error msg = %d", ntohs(*p_length));
				return NULL;
			}

			/*************** DEBUG *************/
			printf("Data by byte- sending to Tun from TCP (%d total):\n",bytesRead);
			int i;
			for(i=HEADER_SIZE;i<length;i++) {
				printf("%4d ",buffer[i]);
			}
			printf("\n\n");

			/*************************************/

			//Send data to TAP (private) interface
			if (write(proxyinfo->tapFD, buffer+HEADER_SIZE, length) < 0)
			{
				perror("Failed to write to TAP interface. \n");
				break;
			}
		}
	}
	return NULL;
}

//Handle for private interface - receive decapsulated, send encapsulated to public interface
void *handle_private(void *arg)
{
	struct ProxyInfo *proxyinfo = (struct ProxyInfo *)arg;
	char buffer[BUFFER_SIZE];
	int16_t *p_type = (int16_t *)buffer; //pointer to space in buffer holding type
	uint16_t *p_length = ((uint16_t *)buffer)+1; //pointer to space in buffer holding length of message

	debug("Starting Tap Thread");
	*p_type = htons(0xABCD); //random value for type

	//read data from TAP (private), encapsulate, and send to public interface
	while (1)
	{
		int dataLength = read(proxyinfo->tapFD, buffer+HEADER_SIZE, BUFFER_SIZE-HEADER_SIZE);
		*p_length = htons(dataLength);

		log_info("Sending a packet with data length: %d", dataLength);
		
		
		
		/****************DEBUG ************/
		printf("Data by byte- sending to TCP from Tun (%d total):\n",dataLength);
		int i;
		for(i=0;i<dataLength;i++) {
			printf("%4d ",buffer[i]);
			if(i==3)
				printf("\n");
		}
		printf("\n\n");
		/********************************************/


		//write to TCP socket
		if (write(proxyinfo->connectionFD, buffer, dataLength+HEADER_SIZE) < 0)
		{
			perror("Unable to send from TAP to TCP.\n");
			return NULL;
		}
	}

	debug("Ending Tap thread");
	return NULL;
}

// returns negative # on fail, FD on success
int createSocket(char *host, int port, struct ProxyInfo *proxyinfo)
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
	struct ProxyInfo proxyinfo;
	int port;
	char *host;
	char *tap;

	//set up signal handler if proxy is killed
	signal(SIGHUP, handle_signal);
	signal(SIGINT, handle_signal);
	signal(SIGKILL, handle_signal);
	signal(SIGPIPE, handle_signal);
	signal(SIGALRM, handle_signal);
	signal(SIGTERM, handle_signal);
	signal(SIGUSR1, handle_signal);
	signal(SIGUSR2, handle_signal);
	signal(SIGSTOP, handle_signal);

	if (argc == 3) // SERVER
	{
		proxyinfo.mode = MODE_SERVER;
		host = NULL;
		port = (int)atoi(argv[1]);
		tap = argv[2];
	} else if (argc == 4) // CLIENT
	{
		proxyinfo.mode = MODE_CLIENT;
		host = argv[1];
		port = (int)atoi(argv[2]);
		tap = argv[3];
	} else
	{
		printHelp();
		return 1;
	}

	if ( (proxyinfo.connectionFD = createSocket(host, port, &proxyinfo) ) == -1 )
	{
		perror("Creating an endpoint for communication failed! \n");
		return 1;
	}

	if ( (proxyinfo.tapFD = allocate_tunnel(tap, IFF_TAP | IFF_NO_PI)) < 0 ) 
	{
		perror("Opening tap interface failed! \n");
		return 1;
	}
	
	if (createConnection(&proxyinfo) == -1)
	{
		return 1;
	}


	pthread_create(&thread_public,NULL,&handle_public, &proxyinfo);
	pthread_create(&thread_private,NULL,&handle_private, &proxyinfo);
	pthread_join(thread_public,NULL);
	pthread_join(thread_private,NULL);

	close(proxyinfo.connectionFD);
	close(proxyinfo.tapFD);
	return 0;
}

