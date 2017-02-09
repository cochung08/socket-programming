#include "function.h"
#include "odr.h"
#include "unp.h"


void sayhi() { printf("\n\n\nqqqqqqqqqqqqqq\n\n\n"); }

void msg_send(int sockfd, char *dest_address, int dest_port, char *msg,
              int flag) {

  struct sockaddr_un cliaddr, servaddr;
  bzero(&servaddr, sizeof(servaddr)); /* fill in server's address */
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, UNIXDG_CLIENT);

  char msgSequence[MAXLINE];
  sprintf(msgSequence, "%s;%d;%d;%s", dest_address, dest_port, flag, msg);

  Sendto(sockfd, msgSequence, strlen(msgSequence), 0, (SA *)&servaddr,
         sizeof(servaddr));
}

char *msg_recv(int sockfd, int port) {
  if (port == DEST_PORT) {
    struct sockaddr_un cliaddr;
    int n;
    socklen_t len;
    char mesg[MAXLINE];
    char *ch;

    len = sizeof(cliaddr);

    n = Recvfrom(sockfd, mesg, MAXLINE, 0, (SA *)&cliaddr, &len);

    // printf("\nthe message has received from odr:%s \n", mesg);

    char splitMsg[100];
    strcpy(splitMsg, mesg);

    char *source_addr = strtok(splitMsg, ";");
    printf("source_addr: %s\n", source_addr);

    char *sourcePortStr = strtok(NULL, ";");
    //	printf("destinationPortStr: %s\n", destinationPortStr);

    int sourcePortNum = atoi(sourcePortStr);
    //	printf("destinationPortINT: %d\n", destinationPortNum);

    char *source_msg = strtok(NULL, ";");

    struct in_addr dest_ipv4addr;
    inet_pton(AF_INET, source_addr, &dest_ipv4addr);
    struct hostent *heDest =
        gethostbyaddr(&dest_ipv4addr, sizeof dest_ipv4addr, AF_INET);
    char sourceHostName[20];
    strcpy(sourceHostName, heDest->h_name);

    char vmName[40];
    gethostname(vmName, sizeof(vmName));
    printf("server at node %s  received time request from %s: \n", vmName,
           sourceHostName);
    printf("received message: %s  \n", source_msg);

    printf("\n\nserver at node %s responding to request from %s\n\n", vmName,
           sourceHostName);

    char buff[1024];
    time_t ticks;
    ticks = time(NULL);
    snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
    printf("current time   %s\n", buff);

    char tmp[108];
    strcpy(tmp, cliaddr.sun_path);

    msg_send2(sockfd, "", 147, buff, 0, tmp);

    return tmp;
  } else {
    char recvline[MAXLINE + 1];
    int n = Recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);

    recvline[n] = 0; /* null terminate */
    // Fputs(recvline, stdout);

    char *dest_addr = strtok(recvline, ";");
    // printf("dest_addr: %s\n", dest_addr);

    char *sourcePortStr = strtok(NULL, ";");

    char *flagStr = strtok(NULL, ";");
    //	printf("destinationPortStr: %s\n", destinationPortStr);

    int sourcePortNum = atoi(sourcePortStr);
    //	printf("destinationPortINT: %d\n", destinationPortNum);

    char *source_msg = strtok(NULL, ";");

    char vmName[40];
    gethostname(vmName, sizeof(vmName));

    struct in_addr dest_ipv4addr;
    inet_pton(AF_INET, dest_addr, &dest_ipv4addr);
    struct hostent *heSource =
        gethostbyaddr(&dest_ipv4addr, sizeof dest_ipv4addr, AF_INET);
    char srcHostName[20];
    strcpy(srcHostName, heSource->h_name);

    printf("client at node: %s : received from %s   \n  ", vmName, srcHostName);
    printf("time message : %s :", source_msg);

    printf("\n\n");
    return NULL;
  }
}

void msg_send2(int sockfd, char *dest_address, int dest_port, char *msg,
               int flag, char *sun_path) {

  if (dest_port == 147) {
    struct sockaddr_un cliaddr, servaddr;
    bzero(&servaddr, sizeof(servaddr)); /* fill in server's address */
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, sun_path);

    Sendto(sockfd, msg, strlen(msg), 0, (SA *)&servaddr, sizeof(servaddr));
  }

  else {

    struct sockaddr_un cliaddr, servaddr;
    bzero(&servaddr, sizeof(servaddr)); /* fill in server's address */
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, sun_path);

    char msgSequence[MAXLINE];
    sprintf(msgSequence, "%s;%d;%d;%s", dest_address, dest_port, flag, msg);

    Sendto(sockfd, msgSequence, strlen(msgSequence), 0, (SA *)&servaddr,
           sizeof(servaddr));
  }
}

void printReceivingMsg(char *recvline, char *src_hostName,
                       char *dest_hostName) {
  //

  // recvline[n] = 0; /* null terminate */
  // Fputs(recvline, stdout);

  char *dest_addr = strtok(recvline, ";");
  // printf("dest_addr: %s\n", dest_addr);

  char *sourcePortStr = strtok(NULL, ";");

  char *flagStr = strtok(NULL, ";");
  //	printf("destinationPortStr: %s\n", destinationPortStr);

  int sourcePortNum = atoi(sourcePortStr);
  //	printf("destinationPortINT: %d\n", destinationPortNum);

  char *source_msg = strtok(NULL, ";");

  printf("client at node: %s : received from %s   \n  ", dest_hostName,
         src_hostName);
  printf("time message : %s :", source_msg);

  printf("\n\n");
}
