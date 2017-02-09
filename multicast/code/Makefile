CC = gcc

LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
FLAGS = -g -O2
CFLAGS = ${FLAGS} -I /home/users/cse533/Stevens/unpv13e/lib/

all: get_hw_addrs.o arp_chechung tour_chechung



get_hw_addrs.o: get_hw_addrs.c
		${CC} ${CFLAGS} -c get_hw_addrs.c

arp_chechung:	arp_chechung.o
		${CC} ${CFLAGS} -o $@ arp_chechung.o get_hw_addrs.o ${LIBS}




tour_chechung: tour_chechung.o get_hw_addrs.o  
	${CC} -o tour_chechung tour_chechung.o get_hw_addrs.o ${LIBS}
tour_chechung.o: tour_chechung.c
	${CC} ${CFLAGS} -c tour_chechung.c


clean:
	rm arp_chechung tour_chechung tour_chechung.o get_hw_addrs.o






