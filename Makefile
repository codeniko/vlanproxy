CFLAGS = -Wall -g

all: vpnproxy
.PHONY: all
.PHONY: LL
.PHONY: clean

LL: LL.o

LL.o: LL.c LL.h
	gcc $(CFLAGS) -c LL.c

vpnproxy.o: vpnproxy.c vpnproxy.h defines.h threadHandlers.h typeHandlers.h uthash.h
	gcc $(CFLAGS) -c vpnproxy.c threadHandlers.c typeHandlers.c -pthread

vpnproxy cs352proxy: vpnproxy.o LL.o
	gcc $(CFLAGS) -o cs352proxy vpnproxy.o LL.o -pthread
	@cp cs352proxy vpnproxy
	
clean:
	rm -f cs352proxy vpnproxy *.o *.h.gch
