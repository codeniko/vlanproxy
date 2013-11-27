#include "vpnproxy.h"

//static char *TERM_MESSAGE = "YOYOYO -- Time To Close Connection -- TERMINATE";
Config *config;

//Print usage of program if arguments are incorrect.
void printHelp()
{
	printf("Usage: vpnproxy <Path to Conf file>\n");
}

void freePeer(Peer *peer)
{
	free(peer->host);
	free(peer);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//Get IP address or MAC address of an interface
void getAddr(char *interface, uint8_t *mac, uint8_t *ip)
{
	int s;
	struct ifreq ifbuf;

	if (mac != NULL) {
		mac[MAC_SIZE] = '\0';
		s = socket(PF_INET, SOCK_DGRAM, 0);
		memset(&ifbuf, 0x00, sizeof(ifbuf));
		strcpy(ifbuf.ifr_name, interface);
		ioctl(s, SIOCGIFHWADDR, &ifbuf);
		close(s);
		for( s = 0; s < 6; s++ ) {
			mac[s] = (unsigned char)ifbuf.ifr_hwaddr.sa_data[s];
		}
	} 
	if (ip != NULL) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		ifbuf.ifr_addr.sa_family = AF_INET;
		strcpy(ifbuf.ifr_name, interface);
		ioctl(s, SIOCGIFADDR, &ifbuf);
		close(s);
		struct in_addr sin = ((struct sockaddr_in *)&ifbuf.ifr_addr)->sin_addr;
		char *ip_string = inet_ntoa(sin);
		ip[0] = (uint8_t)atoi(strtok(ip_string, "."));
		ip[1] = (uint8_t)atoi(strtok(NULL, "."));
		ip[2] = (uint8_t)atoi(strtok(NULL, "."));
		ip[3] = (uint8_t)atoi(strtok(NULL, "."));
		//printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifbuf.ifr_addr)->sin_addr));
	}
}

void ipntoh(uint8_t *ip, char *ip_string) //make sure ip_string is [16]
{
	char a[4], b[4], c[4], d[4];
	snprintf(a, 4, "%d", ip[0]);
	snprintf(b, 4, "%d", ip[1]);
	snprintf(c, 4, "%d", ip[2]);
	snprintf(d, 4, "%d", ip[3]);
	ip_string[15] = '\0';
	sprintf(ip_string, "%s.%s.%s.%s", a, b, c, d);
}

void macntoh(uint8_t *mac, char *mac_string) //make sure mac_string is [12]
{
	mac_string[11] = '\0';
	sprintf(mac_string, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}


/**************************************************
 * allocate_tunnel:
 * open a tun or tap device and returns the file
 * descriptor to read/write back to the caller
 *****************************************/
int allocate_tunnel(char *dev, int flags, uint8_t *local_mac) 
{
	int fd, error;
	struct ifreq ifr;
	char *device_name = "/dev/net/tun";
	char buffer[MAX_DEV_LINE];
	if( (fd = open(device_name , O_RDWR)) < 0 ) {
		fprintf(stderr,"error opening /dev/net/tun\n%d:%s\n",errno,strerror(errno));
		return fd;
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = flags;
	if (*dev) {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}
	if( (error = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
		fprintf(stderr,"ioctl on tap failed\n%d:%s\n",errno,strerror(errno));
		close(fd);
		return error;
	}
	strcpy(dev, ifr.ifr_name);
	// Get device MAC address //
	sprintf(buffer,"/sys/class/net/%s/address",dev);
	FILE* f = fopen(buffer,"r");
	fread(buffer,1,MAX_DEV_LINE,f);
	sscanf(buffer,"%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",local_mac,local_mac+1,local_mac+2,local_mac+3,local_mac+4,local_mac+5);
	fclose(f);
	return fd;
}

int sendall(int sock, char *buf, int *len)
{
	int total = 0;        // how many bytes we've sent
	int bytesleft = *len; // how many we have left to send
	int n, i;
	printf("SENDING to SOCK %d LEN %d\n", sock, *len);
	for (i = 0; i < *len; i++)
		printf("%x ", buf[i]);
	printf("\n");

	while(total < *len) {
		n = send(sock, buf+total, bytesleft, 0);
		if (n == -1) { break; }
		total += n;
		bytesleft -= n;
	}

	*len = total; // return number actually sent here

	return n==-1?-1:0; // return -1 on failure, 0 on success
}

/*The buffer for link states packet could potentially be larger
than max 2048 buffer size. Need to break it up into multiple
link state packets */
int sendallstates(int sock, char *buf, int *len)
{
	int setBytes = 1946 //send states in sets of 40s (1946 bytes) if > 2048 bytes
	int remaining = *len;
	while (remaining > 0) {
		if (remaining < BUFFER_SIZE) {
			*len = remaining;
			return sendall(sock, buf, len);
		}

		int l = setBytes;
		if (sendall(sock, buf, &l) == -1) {
			perror("Error sending message in sendallstates!");
			exit(1);
		}
		memcpy(buffer+26, buffer+setBytes, remaining-setBytes);
		remaining = remaining - setBytes +26;
	}
}

//return true/false
int readConf(char *conf)
{
	FILE *file = fopen(conf, "r");
	if (file == NULL) {
		fprintf(stderr, "ERROR: Unable to open Conf file '%s'\n", conf);
		return 0;
	}
	char line[BUFFER_CONF_SIZE];
	while (fgets(line, BUFFER_CONF_SIZE, file) != NULL)
	{
		if (strlen(line) < 2)
			continue;
		char *tok = strtok(line, " ");
		char *tok2 = NULL;
		if (strlen(tok) >= 2 && tok[0] == '/' && tok[1] == '/')
			continue;
		
		if (strncmp(tok, "listenPort", 10)) {
			if ( (tok = strtok(NULL, " ") ) == NULL) {
				fprintf(stderr, "ERROR: Invalid value for listenPort inside '%s'.\n", conf);
				return 0;
			}
			config->port = atoi(tok);
			continue;
		} else if (strncmp(tok, "linkPeriod", 10)) {
			if ( (tok = strtok(NULL, " ") ) == NULL) {
				fprintf(stderr, "ERROR: Invalid value for linkPeriod inside '%s'.\n", conf);
				return 0;
			}
			config->linkPeriod = atoi(tok);
			continue;
		} else if (strncmp(tok, "linkTimeout", 11)) {
			if ( (tok = strtok(NULL, " ") ) == NULL) {
				fprintf(stderr, "ERROR: Invalid value for linkTimeout inside '%s'.\n", conf);
				return 0;
			}
			config->linkTimeout = atoi(tok);
			continue;
		} else if (strncmp(tok, "peer", 4)) {
			if ( (tok = strtok(NULL, " ") ) == NULL || (tok2 = strtok(NULL, " ") ) == NULL ) {
				fprintf(stderr, "ERROR: Invalid value for peer inside '%s'.\n", conf);
				return 0;
			}
			Peer peer = (Peer *) malloc(sizeof(Peer));
			if (peer == NULL) {
				perror("ERROR: Unable to allocate memory. ");
				return 0;
			}
			peer->host = strdup(tok);
			peer->port = atoi(tok2);
			peer->sock = -1;
			LLappend(config->peersList, peer);
			continue;
		} else if (strncmp(tok, "quitAfter", 9)) {
			if ( (tok = strtok(NULL, " ") ) == NULL) {
				fprintf(stderr, "ERROR: Invalid value for quitAfter inside '%s'.\n", conf);
				return 0;
			}
			config->quitAfter = atoi(tok);
			continue;
		} else if (strncmp(tok, "tapDevice", 9)) {
			if ( (tok = strtok(NULL, " ") ) == NULL) {
				fprintf(stderr, "ERROR: Invalid value for tapDevice inside '%s'.\n", conf);
				return 0;
			}
			config->tap = strdup(tok);
			continue;
		}
	}
	return 1;
}

/* Get peer from socket */
Peer *getPeer(int sock)
{
	LLNode *lln = config->peersList->head;
	for (; lln != NULL; lln=lln->next) {
		Peer *peer = (Peer *)lln->data;
		if (peer->sock == sock)
			return peer;
	}
	return NULL;
}

Edge *getEdge(Peer *p1, Peer *p2)
{
	LLNode *lln = config->edgeList->head;
	for (; lln != NULL; lln=lln->next) {
		Edge *edge = (Edge *)lln->data;
		if (edge->peer1 == p1 && edge->peer2 == p2)
			return edge;
	}
	return NULL;
}

int vpnconnect(Peer *peer)
{
	int sock = createSocket(peer);
	//Connect to server if client
	if (connect(sock, (struct sockaddr *) &(peer->sockaddr), sizeof(peer->sockaddr)) < 0)
	{
		perror("Connection to server has failed. \n");
		return -1;
	}
	peer->sock = sock; //successfully connected
	FD_SET(sock, &(config->masterFDSET));
	if (sock > config->fdMax)
		config->fdMax = sock;
	return sock;
}


//Create a socket by specifying the host and port.
//RETURNS: File Descriptor on success, -1 on failure.
int createListenSocket(struct sockaddr_in *sockaddr)
{
	struct sockaddr_in to; /* remote internet address */
	struct hostent *hp = NULL; /* remote host info from gethostbyname() */

	memset(&to, 0, sizeof(to));

	to.sin_family = AF_INET;
	to.sin_port = htons(config->port); //listening port
	to.sin_addr.s_addr = INADDR_ANY;


	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
	{
		perror("Failed to create socket.\n");
		return -1;
	}


	memset(sockaddr, 0, sizeof(sockaddr));
	sockaddr = &to;
	return sock;
}
//Create a socket by specifying the host and port.
//RETURNS: File Descriptor on success, -1 on failure.
int createSocket(Peer *peer)
{
	struct sockaddr_in to; /* remote internet address */
	struct hostent *hp = NULL; /* remote host info from gethostbyname() */

	memset(&to, 0, sizeof(to));

	to.sin_family = AF_INET;
	to.sin_port = htons(peer->port); //remote port
	/* If internet "a.d.c.d" address is specified, use inet_addr()
	 * to convert it into real address. If host name is specified,
	 * use gethostbyname() to resolve its address */
	to.sin_addr.s_addr = inet_addr(peer->host); /* If "a.b.c.d" addr */
	if (to.sin_addr.s_addr == -1)
	{
		hp = gethostbyname(peer->host); //hostname in form of text
		if (hp == NULL) {
			fprintf(stderr, "Host name %s not found\n", peer->host);
			return -1;
		}
		bcopy((char *)hp->h_addr, (char *)&(to.sin_addr.s_addr), hp->h_length);
	}


	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
	{
		perror("Failed to create socket.\n");
		return -1;
	}

	int optval = 1;
	/* avoid EADDRINUSE error on bind() */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0) {
		perror("Failed to set socket options.\n");
		return -1;
	}

	memset(&(peer->sockaddr), 0, sizeof(peer->sockaddr));
	peer->sockaddr = to;
	return sock;
}

int main(int argc, char **argv)
{
	struct Hash *ht = NULL; 
	config = (Config *)malloc(sizeof(Config));
	config->peer = (Peer *) malloc(sizeof(Peer));
	config->peersList= (LL *)malloc(sizeof(LL));
	config->edgeList= (LL *)malloc(sizeof(LL));
	config->tap = NULL;
	config->fdMax = -1;
	getAddr("eth0", config->peer->ethMac, config->peer->ip);
	FD_ZERO(&(config->masterFDSET));
	FD_ZERO(&(config->readFDSET));

/*	//set up signal handler if proxy is killed
	signal(SIGHUP, handle_signal);
	signal(SIGINT, handle_signal);
	signal(SIGPIPE, handle_signal);
	signal(SIGALRM, handle_signal);
	signal(SIGTERM, handle_signal);
	signal(SIGUSR1, handle_signal);
	signal(SIGUSR2, handle_signal);
*/
	if (argc != 2)
	{
		printHelp();
		return 1;
	}

	if ( (config->tapFD = allocate_tunnel(tap, IFF_TAP | IFF_NO_PI, config->peer->tapMac)) < 0 ) 
	{
		perror("Opening tap interface failed! \n");
		return 1;
	}

	
	pthread_create(&(config->listenTID),NULL,&vpnlisten, NULL); //listen thread

	if (config->peersList->size > 0)
	{ //initial peers defined in config, attempt to connect to peers
		LLNode cur = config->peersList->head;
		int sock;
		while (cur != NULL)
		{
			Peer *peer = (Peer *) cur->data;
			sock = vpnconnect(peer);
			if (sock != -1) { //successful connection!
				FD_SET(sock, &(config->masterFDSET));
				if (sock > config->fdMax)
					config->fdMax = sock;
				sendInitState(peer);
				cur = cur->next;
			} else { //unable to connect, remove from peersList to remain disconnected
				cur = cur->next;
				LLremove(config->peersList, peer);
				freePeer(peer);
			}
		}
	}




	pthread_create(&(config->publicTID),NULL,&handle_public, NULL);
	pthread_create(&(config->privateTID),NULL,&handle_private, NULL);
	//pthread_join(thread_public,NULL);
	//pthread_join(thread_private,NULL);

//BELOW HERE UNCHECKED
	close(config->filedesc->connectionFD);
	close(config->filedesc->tapFD);
	free(config->filedesc);
	free(config);
	return 0;
}
