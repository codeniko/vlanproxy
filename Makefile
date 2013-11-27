CFLAGS = -Wall -g

all: vpnproxy
.PHONY: all
.PHONY: LL
.PHONY: clean

LL: LL.o

LL.o: LL.c LL.h
	gcc $(CFLAGS) -c LL.c

threadHandlers.o: threadHandlers.h
	gcc $(CFLAGS) -c threadHandlers.c

typeHandlers.o: typeHandlers.h
	gcc $(CFLAGS) -c typeHandlers.c

vpnproxy.o: vpnproxy.c defines.h threadHandlers.h typeHandlers.h vpnproxy.h
	gcc $(CFLAGS) -c vpnproxy.c -pthread

vpnproxy cs352proxy: vpnproxy.o LL.o threadHandlers.o typeHandlers.o
	gcc $(CFLAGS) -o cs352proxy vpnproxy.o LL.o threadHandlers.o typeHandlers.o -pthread
	@cp cs352proxy vpnproxy
	
clean:
	rm -f cs352proxy vpnproxy *.o *.h.gch
