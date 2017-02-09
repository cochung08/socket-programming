#include "unp.h"
#define SIZE 1024
#define PORT 21696

void str_cli_echo(FILE *, int, char *);

// the first argument argv[1] is ip address
// the second argument argv[2] is the descriptor of sending end of pipe
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

  // Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
  if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {

    char *ptr;
    int pfd = strtol(argv[2], &ptr, 10);
    write(pfd, termInfo4, strlen(termInfo4) + 1);
    err_sys("connect error");
  }

  str_cli_echo(stdin, sockfd, argv[2]);

  exit(0);
}

// echo function:
// modified from :Figure 6.13 str_cli function using select that handles EOF
// correctly.
// fp is the user input descriptor
// sockfd is the socket descriptor
// pfdStr is the string format of the descriptor of pipe

void str_cli_echo(FILE *fp, int sockfd, char *pfdStr) {
  printf("%s\n\n", "echo service started");
  printf("%s\n\n", "please type something to echo:   ");
  int maxfdp1, stdineof;
  fd_set rset;
  char buf[MAXLINE];
  memset(buf, 0, strlen(buf));
  int n;

  stdineof = 0;
  FD_ZERO(&rset);

  char *ptr;
  int pfd = strtol(pfdStr, &ptr, 10);

  char termInfo[] = "server process crash\n";
  char termInfo2[] = "echo child process terminated\n";
  char termInfo3[] = "user enter ctrl D (EOF)\n";

  for (;;) {

    // use select to monitor the descriptor of socket and user input

    if (stdineof == 0)
      FD_SET(fileno(fp), &rset);
    FD_SET(sockfd, &rset);
    maxfdp1 = max(fileno(fp), sockfd) + 1;
    Select(maxfdp1, &rset, NULL, NULL, NULL);

    if (FD_ISSET(sockfd, &rset)) { /* socket is readable */
      memset(buf, 0, strlen(buf));
      if ((n = Read(sockfd, buf, MAXLINE)) == 0) {
        if (stdineof == 1) {
          write(pfd, termInfo, strlen(termInfo) + 1);
          return; /* normal termination */
        } else {
          write(pfd, termInfo, strlen(termInfo) + 1);
          err_quit("str_cli: server terminated prematurely");
        }
      }

      Write(pfd, buf, n + 1);
      // write(pfd, buf, strlen(buf) + 1);

      printf("echo mesaage: %s\n", buf);
      printf("%s\n\n", "please type something to echo:   ");
    }

    if (FD_ISSET(fileno(fp), &rset)) {                 /* input is readable */
      if ((n = Read(fileno(fp), buf, MAXLINE)) == 0) { /*user enter ctrL D*/
        write(pfd, termInfo3, strlen(termInfo3) + 1);
        // write(sockfd, termInfo3, strlen(termInfo3) + 1);
        stdineof = 1;
        Shutdown(sockfd, SHUT_WR); /* send FIN */
        FD_CLR(fileno(fp), &rset);
        break;
      }

      Writen(sockfd, buf, n);
    }
  }
}
