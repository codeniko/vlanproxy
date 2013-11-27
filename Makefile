CFLAGS = -Wall -g

all: vpnproxy
.PHONY: all
.PHONY: LL
.PHONY: clean

LL: LL.o

LL.o: LL.c LL.h
	gcc $(CFLAGS) -c LL.c

vpnproxy.o: vpnproxy.c vpnproxy.h
	gcc $(CFLAGS) -c vpnproxy.c -pthread

vpnproxy cs352proxy: vpnproxy.o
	gcc $(CFLAGS) -o cs352proxy -pthread
	@cp cs352proxy vpnproxy
	
clean:
	rm -f cs352proxy vpnproxy *.o *.h.gch
