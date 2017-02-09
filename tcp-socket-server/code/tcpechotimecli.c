#include "unp.h"

#define ECHO "echo"
#define TIME "time"
#define EXIT "quit"

void sig_chld(int signo) {
  pid_t pid;
  int stat;

  while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    printf("child %d terminated\n\n", pid);
  return;
}

int main(int argc, char **argv) {

  char localAddr[] = "127.0.0.1";

  int nread;
  char userInput[MAXLINE];

  char *decimalAddr;

  struct in_addr **pptr;
  struct in_addr *inetaddrp[2];
  struct in_addr inetaddr;
  struct hostent *hp;
  struct servent *sp;
  // void sig_chld(int);
  char termInfo[] = "server process crash\n";
  char termInfo4[] = "cannot connect to server\n";

  if (inet_pton(AF_INET, argv[1], &inetaddr) == 1) {

    if (hp = gethostbyaddr((char *)&inetaddr, sizeof(inetaddr), AF_INET)) {
      printf("address type: decimal address \n\n");
    }
  } else if ((hp = gethostbyname(argv[1])) != NULL) {
    printf("addressType: hostname\t \n\n");
  } else
    err_quit("hostname error for %s: %s", argv[1], hstrerror(h_errno));

  printf("The server host is:\t%s\n\n", hp->h_name);

  pptr = (struct in_addr **)hp->h_addr_list;

  int k = 0;
  for (k = 0; pptr[k] != NULL; k++) {

    char str[INET_ADDRSTRLEN];
    decimalAddr = inet_ntop(AF_INET, pptr[k], str, sizeof(str));
    printf("ip address:\t %s\n\n", decimalAddr);
  }

  // convert network address to decimal form
  char decimalAddressC[1024];
  int p = 0;
  int leng = strlen(decimalAddr);
  for (; p < leng; p++)
    decimalAddressC[p] = decimalAddr[p];
  decimalAddressC[leng] = '\0';

  Signal(SIGCHLD, sig_chld);

  while (1) {
    sleep(1);

    printf("%s\n", "if you want to echo, please enter <echo> ");
    printf("%s\n", "if you want to report time, please enter <time> ");
    printf("%s\n\n", "if you want to quit, please enter <quit> ");

    if (Fgets(userInput, MAXLINE, stdin) != NULL) {

      //		printf("%s %d\n", ECHO1, strlen(ECHO1));
      printf("the service you choose:	%s \n", userInput);
      userInput[strlen(userInput) - 1] = '\0';

      if (strcasecmp(userInput, ECHO) == 0) {

        int pfd[2];
        int tmp1 = pipe(pfd);

        if (fork() == 0) {

          int pid_ = getpid();
          printf("echo child process created, id: %d\n\n", pid_);
          //			printf("%s\n", "children");
          close(pfd[0]);

          // convert the int type of desciptor of pipe into string format
          char pfdStr[15];
          sprintf(pfdStr, "%d", pfd[1]);

          if ((execlp("xterm", "xterm", "-e", "./echo_cli", decimalAddressC,
                      pfdStr, (char *)0)) < 0) {
            printf("%s\n", "execlpError");
          } else {
            printf("%s\n", "noError");
          }
          exit(1);
        } else {
          //			printf("%s\n", "parent1");
          close(pfd[1]);

          char buf[MAXLINE];

          while (1) {
            nread = read(pfd[0], buf, MAXLINE);
            // printf("nread:  %d\n", nread);
            if (nread > 0) {
              // if server crash, parent will receive info from children and
              // then terminate the program
              if (strcmp(buf, termInfo) == 0) {
                printf("%s\n", buf);
                exit(1);
              } else if (strcmp(buf, termInfo4) == 0) {
                // if cannot connect to server, go back to main menu
                printf("%s\n", buf);
                sleep(1);
                break;
              } else
                printf("read from echo child: %s\n", buf);

            } else if (nread == 0) {
              printf("echo child terminated\n\n");
              break;
            }
          }
        }
      } else if (strcasecmp(userInput, TIME) == 0) {

        int pfd[2];
        int tmp1 = pipe(pfd);

        if (fork() == 0) {

          int pid_ = getpid();
          printf("time child process created, id: %d\n\n", pid_);
          //			printf("%s\n", "children");
          close(pfd[0]);

          // convert the int type of desciptor of pipe into string format
          char pfdStr[15];
          sprintf(pfdStr, "%d", pfd[1]);

          int execN = execlp("xterm", "xterm", "-e", "./time_cli",
                             decimalAddressC, pfdStr, (char *)0);
          printf("execN%d: \n", execN);

          if (execN < 0) {
            printf("%s\n", "execlpError");
          } else {
            printf("%s\n", "noError");
          }
          printf("sssssssss\n");
          exit(1);
        } else {
          //			printf("%s\n", "parent1");
          close(pfd[1]);

          char buf[MAXLINE];
          while (1) {

            memset(buf, 0, strlen(buf));
            nread = read(pfd[0], buf, MAXLINE);

            // printf("nread:  %d\n", nread);
            if (nread > 0) {
              // if server crash, parent will receive info from children and
              // then terminate the program
              if (strcmp(buf, termInfo) == 0) {
                printf("%s\n", buf);
                exit(1);
              } else if (strcmp(buf, termInfo4) == 0) {
                // if cannot connect to server, go back to main menu
                printf("%s\n", buf);
                sleep(1);
                break;
              } else
                printf("read from time child : %s\n", buf);
            }

            else if (nread == 0) {
              printf("time child terminated\n\n");
              break;
            }
          }
        }
      } else if (strcasecmp(userInput, EXIT) == 0) {
        exit(1);
      } else
        printf("no such service;  please enter again\n");
    }
  }
}
