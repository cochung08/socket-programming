
CC = gcc

LIBS =  -lsocket\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a

FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: udpserv udpcli


get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c











udpserv: udpserv.o get_ifi_info_plus.o
	${CC} ${FLAGS} -o udpserv udpserv.o get_ifi_info_plus.o ${LIBS} -lm
udpserv.o: udpserv.c
	${CC} ${CFLAGS} -c udpserv.c

udpcli: udpcli.o dg_send_recv.o get_ifi_info_plus.o
	${CC} ${FLAGS} -o udpcli udpcli.o dg_send_recv.o get_ifi_info_plus.o  ${LIBS} -lm
udpcli.o: udpcli.c
	${CC} ${CFLAGS} -c udpcli.c

clean:
	rm udpserv udpserv.o get_ifi_info_plus.o udpcli udpcli.o dg_send_recv.o
