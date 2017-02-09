#include "unp.h"
#define SIZE 1024
#define PORT 21697

void *str_cli_time1(FILE *, int, char *);

int main(int argc, char **argv) {
  int i, sockfd;
  struct sockaddr_in servaddr;
  char termInfo4[] = "cannot connect to server\n";

  if (argc != 3)
    printf("%s\n", "usage: tcpcli <IPaddress>");
  //		err_quit("usage: tcpcli <IPaddress>");

  sockfd = Socket(AF_INET, SOCK_STREAM, 0);

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  //		servaddr.sin_port = htons(SERV_PORT);
  servaddr.sin_port = htons(PORT);
  Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

  // if connot connect to server, then it send message to parent window and then
  // close the child thread
  //	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
  if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {

    char *ptr;
    int pfd = strtol(argv[2], &ptr, 10);
    write(pfd, termInfo4, strlen(termInfo4) + 1);

    err_sys("connect error");
  }

  str_cli_time1(stdin, sockfd, argv[2]); /* do it all */
  //	str_cli(stdin, sockfd); /* do it all */

  exit(0);
}

void *str_cli_time1(FILE *fp, int sockfd, char *pfdStr) {
  char sendline[MAXLINE] = "random";
  char recvline[MAXLINE];

  char termInfo[] = "server process crash\n";
  char termInfo2[] = "echo child process terminated\n";
  char termInfo3[] = "user enter ctrl D (EOF)\n";

  printf("%s\n", "time service started");
  char *ptr;
  int pfd = strtol(pfdStr, &ptr, 10);

  while (1) {

    // read time info from the server. If server crashes,read socket return 0
    // and  child tells parent window that server crashs
    if (Readline(sockfd, recvline, MAXLINE) == 0) {
      write(pfd, termInfo, strlen(termInfo) + 1);
      err_quit("str_cli: server terminated prematurely");
    }

    // report the time info to the parent window through pipe
    write(pfd, recvline, strlen(recvline) + 1);
    printf("%s\n", "read from time server, report time to parent client");
    // Fputs(recvline, stdout);
  }
}
