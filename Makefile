all:
	gcc -c vlanproxy.c vlanproxy.h -pthread
	gcc -o cs352proxy vlanproxy.o -pthread
