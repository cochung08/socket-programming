#include "unp.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define BUFSIZE 1500

/* globals */
char sendbuf[BUFSIZE];

int datalen; /* # bytes of data following ICMP header */
char *host;
int nsent; /* add 1 for each sendto() */
pid_t pid; /* our PID */
int sockfd;
int verbose;
