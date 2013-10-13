all:
	gcc -c vpnproxy.c vpnproxy.h -pthread
	gcc -o cs352proxy vpnproxy.o -pthread
