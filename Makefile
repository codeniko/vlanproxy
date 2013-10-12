all:
	gcc -c vpnproxy.c vpnproxy.h -pthread
	gcc -o vpnproxy vpnproxy.o -pthread
