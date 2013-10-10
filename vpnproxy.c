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

#define MODE_SERVER 		0
#define MODE_CLIENT 		1
#define MAX_CONNECTIONS		1
#define HEADER_FIELD_SIZE	2
#define HEADER_SIZE			4
#define BUFFER_SIZE			2048


struct ProxyInfo
{
	int mode;
	int connectionFD;
	int tapFD;
	struct sockaddr_in sockaddr;
};

void printHelp()
{
	printf("Usage: \nServer: vpnproxy <port> <local interface>\nClient: vpnproxy <remote host> <remote port> <local interface>");
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

//returns negative number on fail
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
		if (connect(proxyinfo->connectionFD, (struct sockaddr *) &(proxyinfo->sockaddr), sizeof(proxyinfo->sockaddr)) < 0)
		{
			perror("Connection to server has failed. \n");
			return -1;
		}
		debug("Client after connection");
		return 0;
	} else
	{
		fprintf(stderr, "Error: unknown mode");
	}

	return -1;
}

//receive encapsulated, must decapsulate and send to tap
void *threadTCP(void *arg)
{
	struct ProxyInfo *proxyinfo = (struct ProxyInfo *)arg;
	char buffer[BUFFER_SIZE];
	int16_t *p_type = (int16_t *)buffer; //pointer to space in buffer holding type
	int16_t *p_length = ((int16_t *)buffer)+1; //pointer to space in buffer holding length of message

	if (proxyinfo->mode == MODE_SERVER || proxyinfo->mode == MODE_CLIENT)
	{
		//Perform read and write to TAP interface for active connection indefinitely
		int offset = 0;
		while (1)
		{
			log_info("CLIENT: Loop restart");

			//Read data from TCP
			int bytesRead = read(proxyinfo->connectionFD, buffer, BUFFER_SIZE);
			if (bytesRead < HEADER_SIZE)
			{
				fprintf(stderr, "Invalid packet received, too short.\n");
				continue;
			}

			//Get type from header and check if equal to random 0xABCD
			int type = ntohs(*p_type);
			if (type != 0xABCD)
			{
				fprintf(stderr, "Client received wrong type. Dropping this message. Size of error msg = %d", ntohs(*p_length));
				//continue;
			}

			//Get length of data from header
			int length = ntohs(*p_length);
			log_info("Length of packet is %d", length);
			//Send data to TAP interface
			if (write(proxyinfo->tapFD, buffer+HEADER_SIZE, length) < 0)
			{
					perror("Failed to write to TAP interface. \n");
					//break
			}
		}
	} else
	{
		fprintf(stderr, "Error: unknown mode");
	}

	return NULL;
}

//receive decapsulated, must encapsulate and send to TCP
void *threadTAP(void *arg)
{
	struct ProxyInfo *proxyinfo = (struct ProxyInfo *)arg;
	char buffer[BUFFER_SIZE];
	int16_t *p_type = (int16_t *)buffer; //pointer to space in buffer holding type
	uint16_t *p_length = ((uint16_t *)buffer)+1; //pointer to space in buffer holding length of message

	debug("Starting Tap Thread");

	*p_type = htons(0xABCD); //random value for type

	//read data from TAP and encapsule it
	while (1)
	{
		int dataLength = read(proxyinfo->tapFD, buffer+HEADER_SIZE, BUFFER_SIZE-HEADER_SIZE);
		*p_length = htons(dataLength);
		
		log_info("Sending a packet with data length: %d", dataLength);
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
	pthread_t tcpThread;
	pthread_t tapThread;

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

	if ( (proxyinfo.connectionFD = createSocket(host, port, &proxyinfo) ) < 0 )
	{
		return 2;
	}

	if ( (proxyinfo.tapFD = allocate_tunnel(tap, IFF_TAP | IFF_NO_PI)) < 0 ) 
	{
		perror("Opening tap interface failed! \n");
		return 3;
	}

	if (createConnection(&proxyinfo) < 0)
	{
		return 4;
	}


	pthread_create(&tcpThread,NULL,&threadTCP, &proxyinfo);
	pthread_create(&tapThread,NULL,&threadTAP, &proxyinfo);
	pthread_join(tcpThread,NULL);
	pthread_join(tapThread,NULL);

	close(proxyinfo.connectionFD);
	close(proxyinfo.tapFD);
	return 0;
}
