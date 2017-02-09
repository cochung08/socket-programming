#include "hw_addrs.h"
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/socket.h>


#include "function.h"
#include "odr.h"
#include "unp.h"


int main(int argc, char **argv) {

  printf("server start\n\n");

  int unSockfd;
  struct sockaddr_un servaddr, cliaddr;

  unSockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);

  unlink(UNIXDG_SERVER);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, UNIXDG_SERVER);

  Bind(unSockfd, (SA *)&servaddr, sizeof(servaddr));

  while (1) {

    //		int n;
    //		socklen_t len;
    //		char mesg[MAXLINE];
    //		char *ch;
    //
    //		len = sizeof(cliaddr);
    //
    //		n = Recvfrom(unSockfd, mesg, MAXLINE, 0, (SA *) &cliaddr, &len);
    //
    //		//printf("\nthe message has received from odr:%s \n", mesg);
    //
    //		char splitMsg[100];
    //		strcpy(splitMsg, mesg);
    //
    //		char *source_addr = strtok(splitMsg, ";");
    //		printf("source_addr: %s\n", source_addr);
    //
    //		char *sourcePortStr = strtok(NULL, ";");
    ////	printf("destinationPortStr: %s\n", destinationPortStr);
    //
    //		int sourcePortNum = atoi(sourcePortStr);
    ////	printf("destinationPortINT: %d\n", destinationPortNum);
    //
    //		char *source_msg = strtok(NULL, ";");
    //
    //		struct in_addr dest_ipv4addr;
    //		inet_pton(AF_INET, source_addr, &dest_ipv4addr);
    //		struct hostent *heDest = gethostbyaddr(&dest_ipv4addr,
    //				sizeof dest_ipv4addr,
    //				AF_INET);
    //		char sourceHostName[20];
    //		strcpy(sourceHostName, heDest->h_name);
    //
    //		char vmName[40];
    //		gethostname(vmName, sizeof(vmName));
    //		printf("server at node %s  received time request from %s: \n",
    //vmName,
    //				sourceHostName);
    //		printf("received message: %s  \n",source_msg );
    //
    //		char buff[1024];
    //		time_t ticks;
    //		ticks = time(NULL);
    //		snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
    //		printf("current time   %s\n", buff);

    //		Sendto(unSockfd, buff, n, 0, (SA *) &cliaddr, len);

    msg_recv(unSockfd, DEST_PORT);

    printf("sending back to server ODR   \n");
    printf("_____________________________________________\n");
  }
}
