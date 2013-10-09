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

#define MODE_SERVER 		0
#define MODE_CLIENT 		1
#define MAX_CONNECTIONS		10
#define HEADER_FIELD_SIZE	2
#define BUFFER_SIZE			2048


struct ProxyInfo
{
	int mode;
	int socketFD;
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

//receive encapsulated, must decapsulate and send to tap
void *threadTCP(void *arg)
{
	struct ProxyInfo *proxyinfo = (struct ProxyInfo *)arg;
	char vlan_type[HEADER_FIELD_SIZE];
	char vlan_length[HEADER_FIELD_SIZE];
	char buffer[BUFFER_SIZE- 2*(HEADER_FIELD_SIZE)];
	int connectFD = -1;

	if (proxyinfo->mode == MODE_SERVER)
	{
		//Assigning a name to the socket
		if (bind(proxyinfo->socketFD, (struct sockaddr *) (&(proxyinfo->sockaddr)), sizeof(proxyinfo->sockaddr)) < 0)
		{
			perror("Server binding failed.\n");
			close(proxyinfo->socketFD);
			return NULL;
		}

		//Set the number of allowed connections in the queue
		if (listen(proxyinfo->socketFD, MAX_CONNECTIONS) < 0)
		{
			perror("Server listening failed. \n");
			close(proxyinfo->socketFD);
			return NULL;
		}

		//Accept connections from client
		for (;;)
		{
			connectFD = accept(proxyinfo->socketFD, NULL, NULL);
			if (connectFD < 0)
			{
				perror("Server unable to accept connection. \n");
				close(proxyinfo->socketFD);
				return NULL;
			}
			
			//Perform read and write to TAP interface for active connection indefinitely
			for (;;)
			{
				//Get vlan type
				if (read(connectFD, vlan_type, HEADER_FIELD_SIZE) < 0)
				{
					break;
				}
				//Get Length of message
				if (read(connectFD, vlan_length, HEADER_FIELD_SIZE) < 0)
				{
					break;
				}

				size_t lengthOfMessage = atoi(vlan_length); //convert string to integer

				//Get data
				if (read(connectFD, buffer, lengthOfMessage) < 0)
				{
					break;
				}

				//Send data to TAP interface
				if (write(proxyinfo->tapFD, buffer, lengthOfMessage) < 0)
				{
					perror("Failed to write to TAP interface. \n");
					//break
				}
			}
		}
	} else if (proxyinfo->mode == MODE_CLIENT)
	{
		if (connect(proxyinfo->socketFD, (struct sockaddr *) &(proxyinfo->sockaddr), sizeof(proxyinfo->sockaddr)) < 0)
		{
			perror("Connection to server has failed. \n");
			close(proxyinfo->socketFD);
			return NULL;
		}

		connectFD = proxyinfo->socketFD;

		//Perform read and write to TAP interface for active connection indefinitely
		for (;;)
		{
			//Get vlan type
			if (read(connectFD, vlan_type, HEADER_FIELD_SIZE) < 0)
			{
				break;
			}
			//Get Length of message
			if (read(connectFD, vlan_length, HEADER_FIELD_SIZE) < 0)
			{
				break;
			}

			size_t lengthOfMessage = atoi(vlan_length); //convert string to integer

			//Get data
			if (read(connectFD, buffer, lengthOfMessage) < 0)
			{
				break;
			}

			//Send data to TAP interface
			if (write(proxyinfo->tapFD, buffer, lengthOfMessage) < 0)
			{
					perror("Failed to write to TAP interface. \n");
					//break
			}
		}
	} else
	{
		perror("A fatal error has occured. Unknown server/client mode. \n");
		close(proxyinfo->socketFD);
		return NULL;
	}

	//Thread ended, close file descriptors
	close(connectFD);
	if (proxyinfo->mode == MODE_SERVER)
	{
		//close socketFD if server because connectFD != socketFD
		close(proxyinfo->socketFD); 	
	}
	return NULL;
}

//receive decapsulated, must encapsulate and send to TCP
void *threadTAP(void *arg)
{
	struct ProxyInfo *proxyinfo = (struct ProxyInfo *)arg;
	int16_t vlan_type = htons(0xABCD); //random value for part 1
	int16_t vlan_length; //holds length of buffer in bytes
	char *buffer[BUFFER_SIZE];
	size_t numBytesRead;

	//read data from TAP and encapsule it
	while ( (numBytesRead = read(proxyinfo->tapFD, &buffer, BUFFER_SIZE)) > 0)
	{
		vlan_length = htons(numBytesRead);
		if ( (write(proxyinfo->socketFD, &vlan_type, HEADER_FIELD_SIZE)) < 0 
			|| (write(proxyinfo->socketFD, &vlan_length, HEADER_FIELD_SIZE)) < 0
			|| (write(proxyinfo->socketFD, &buffer, numBytesRead)) < 0 )
		{
			perror("Failed to send data from TAP to TCP socket.\n");
		}
	}

	close(proxyinfo->tapFD);
	proxyinfo->tapFD = -1;
	return NULL;
}

// returns negative # on fail, FD on success
int createSocket(char *host, unsigned short port, struct ProxyInfo *proxyinfo)
{
	struct sockaddr_in to; /* remote internet address */
	struct hostent *hp = NULL; /* remote host info from gethostbyname() */

	memset(&to, 0, sizeof(to));

	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	if (host == NULL)
	{
		to.sin_addr.s_addr = inet_addr(INADDR_ANY);
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
			bcopy(hp->h_addr, &to.sin_addr, hp->h_length);
		}
	}


	int socketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socketFD == -1)
	{
		perror("Failed to create socket.\n");
		return -1;
	}

	int optval = 1;
	/* avoid EADDRINUSE error on bind() */
	if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0) {
		perror("Failed to set socket options.\n");
		return -1;
	}

	memset(&(proxyinfo->sockaddr), 0, sizeof(proxyinfo->sockaddr));
	proxyinfo->sockaddr = to;
	return socketFD;


	/* give the connect call the sockaddr_in struct that has the address
	* in it on the connect call */
/*	if (connect(socketFD, &to, sizeof(to)) < 0) {
		perror("Connect failed");
		return -3;
	}
*/
}

// returns -1 on fail, FD on success
int createTap(char *tap)
{
	int tapFD = -1;
	if ( (tapFD = allocate_tunnel(tap, IFF_TAP | IFF_NO_PI)) < 0 ) 
	{
		perror("Opening tap interface failed! \n");
		return -1;
	}
	return tapFD;
}

int main(int argc, char **argv)
{
	struct ProxyInfo proxyinfo;
	unsigned short port;
	char *host;
	char *tap;
	pthread_t tcpThread;
	pthread_t tapThread;

	if (argc == 3) // SERVER
	{
		proxyinfo.mode = MODE_SERVER;
		host = NULL;
		port = (unsigned short)strtoul(argv[1], NULL, 0);
		tap = argv[2];
	} else if (argc == 4) // CLIENT
	{
		proxyinfo.mode = MODE_CLIENT;
		host = argv[1];
		port = (unsigned short)strtoul(argv[2], NULL, 0);
		tap = argv[3];
	} else
	{
		printHelp();
		return 1;
	}

	if ( (proxyinfo.socketFD = createSocket(host, port, &proxyinfo) ) < 0 )
	{
		return 2;
	}

	if ( (proxyinfo.tapFD = createTap(tap) ) < 0 )
	{
		return 3;
	}

	pthread_create(&tcpThread,NULL,&threadTCP, &proxyinfo);
	pthread_create(&tapThread,NULL,&threadTAP, &proxyinfo);
	pthread_join(tcpThread,NULL);
	pthread_join(tapThread,NULL);
	
	return 0;
}
