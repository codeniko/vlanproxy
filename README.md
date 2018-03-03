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
