CC = gcc

LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
FLAGS = -g -O2
CFLAGS = ${FLAGS} -I /home/users/cse533/Stevens/unpv13e/lib/

all: get_hw_addrs.o function.o odr_chechung  client_chechung server_chechung



get_hw_addrs.o: get_hw_addrs.c
		${CC} ${CFLAGS} -c get_hw_addrs.c

function.o: function.c
		${CC} ${CFLAGS} -c function.c





odr_chechung:	odr_chechung.o
		${CC} ${CFLAGS} -o $@ odr_chechung.o get_hw_addrs.o function.o ${LIBS}


client_chechung:	client_chechung.o
		${CC} ${CFLAGS} -o $@ client_chechung.o function.o ${LIBS}

server_chechung:		server_chechung.o
		${CC} ${CFLAGS} -o $@ server_chechung.o function.o ${LIBS}




clean:
	rm get_hw_addrs.o odr_chechung odr_chechung.o client_chechung client_chechung.o server_chechung server_chechung.o
