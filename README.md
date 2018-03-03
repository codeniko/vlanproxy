# vlanproxy

A LAN virtualized over the internet that runs Layer 2 (datalink) over Layer 3 (network). 

The object here was to create a proxy program that runs
on each machine. Multiple proxies join and leave the
virtual local area network. It also implements a basic routing protocol between the
proxies to support topologies where not all the proxies are connected

Upon start up, a client proxy will connect to a single peer proxy. From this point on the proxies
will send various packet types to maintain the VLAN. These include: (1) link state packets, (2)
data packets, and (3) probe packets. Link state packets describe the state, in terms of IP
addresses and network performance, between two proxies. Probe packets are sent to measure
link performance. Data packets send and receive the data that emulates the VLAN.

Proxies will flood link state packets amongst themselves. These describe the state of the TAP
interface and the TCP socket connection between two proxies. Upon receiving a link state
packet, proxies will update these structures:

1. Membership list: a list of all the proxies in the VLAN.
2. A link state list. Each link state represents the connection status between 2 proxies.
3. Forwarding table: describes the next hop for a given layer-2 destination.
4. Routing graph: representing the topology, or connections between all the
proxies. The directed graph is computable from the membership set, which are
the vertices, and the link states, which are the edges. 

The membership of the proxies must be refreshed periodically or members will
be removed from the list. Proxies maintain themselves on peer proxies’ membership lists by
periodically sending updated link state records.. Each link-state record gets a unique ID, which
is timestamped on arrival

## Function descriptions
`main(int argc, char **argv)`  
	Create space in memory for the ProxyInfo struct. Add signal handlers for SIGHUP,SIGINT,SIGPIPE,SIGALRM,SIGTERM,SIGUSR1, and SIGUSR2. Based on the number of the arguments, determine if local proxy is a SERVER or CLIENT. Call a sequence of other functions to establish a socket connection and finally create threads to handle read/write to the public/private interfaces

`printHelp()`  
	If there is an invalid input when invoking the program, this function will print the correct usage.

`handle_signal(int sig)`  
	If a termination signal was sent to the program, handle it here by cancelling both threads (if active), closing all file descriptors and freeing memory. Whichever proxy receives the terminating signal, this proxy will send a custom message to the remote proxy to gracefully terminate that one as well. Both Proxies terminate at roughly the same time.

`allocate_tunnel(char *dev, int flags)`  
	Open the private interface device and return its file descriptor. This function was given to us in the Project Description.

`createSocket(char *host, int port)`  
	Create a socket by specifying the host and port number for the socket address. A lot of the code here was given in the Project Description. Return a file descriptor onces the socket is created as a SOCK_STREAM and uses the TCP protocol

`createConnection()`  
	Create a connection between 2 proxies on a public interface. If server, listen for a client to connect. If client, attempt to connect to the server. Once the connection is established, a boolean is switched to true indicating that we have an active connection.

`handle_public(void *arg)`  
	A thread that indefinitely reads from the socket, receives an encapsulated message with 4 bytes of headers (tag/length), and writes the decapsulated message to the private interface (tap) of the local machine. It works by creating a buffer, filling it by reading from the socket, and sending it to the private interface if the entire message is in the buffer. The messages it accepts must contain the tag 0xABCD in the encapsulated header. Occasionally, the buffer actually reads more data than the length of the message in the header. The partial buffer is sent to the private interface and the remaining bytes that was extra is rewritten to the beginning of the buffer. The loop continues and sends the remaining data. If the message is incomplete (the length in the header > the number of actual bytes in the buffer), then the program reads the remaining length and adds it to the buffer after the incomplete portion. Finally, we also check for the custom TERMINATE message to arrive here IF the message length is equal to the message length of the TERMINATION phrase. If the lengths are equal, then the message is read and compared in order to confirm TERMINATION. Once confirmed, the proxy gracefully exits by calling handle_signal(-1). The argument value of -1 will tell the handler to NOT send a termination message back to the other proxy.

`handle_private(void *arg)`  
	A thread that indefinitely reads from the tap interface, receives a raw message, encapsulates the message with 4 bytes of headers (tag/length), and writes to the socket. This one is more simple than the socket handler as all it does is read, encapsulate, and write back out. There are very few things that can happen wrong here.
