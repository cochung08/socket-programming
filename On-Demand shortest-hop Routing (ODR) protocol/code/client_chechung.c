#include "odr.h"
#include "unp.h"


#include "function.h"

static void recvfrom_alarm(int signo) { return; }

int main(int argc, char **argv) {

  int sockfd;
  struct sockaddr_un cliaddr;

  sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);

  bzero(&cliaddr, sizeof(cliaddr));
  cliaddr.sun_family = AF_LOCAL;

  char *tmpArray = malloc(sizeof(char) * 1024);

  strcpy(tmpArray, "/tmp/myTmpFile-XXXXXX");
  mkstemp(tmpArray);
  unlink(tmpArray);

  strcpy(cliaddr.sun_path, tmpArray);

  Bind(sockfd, (SA *)&cliaddr, sizeof(cliaddr));

  int vmNameSize = 200;
  char vmName[200];

  char sourcevmName[40];
  gethostname(sourcevmName, sizeof(sourcevmName));

  sigset_t sigset_alrm;

  Sigemptyset(&sigset_alrm);
  Sigaddset(&sigset_alrm, SIGALRM);
  Signal(SIGALRM, recvfrom_alarm);

  while (1) {

    printf("please enter virtual machine name:  vm1 vm2 vm3 vm4 vm5 vm6 vm7 "
           "vm8 vm9 vm10\n\n");
    //		Fgets(vmName, vmNameSize, stdin);
    scanf("%s", vmName);

    printf("\nplease enter 0 or 1 for forced discovery, 1 for forced , 0 for "
           "not\n");
    int flag;
    scanf("%d", &flag);

    struct hostent *hptr = gethostbyname(vmName);
    char **pptr = hptr->h_addr_list;
    char str[INET_ADDRSTRLEN];

    char *dest_address;
    for (; *pptr != NULL; pptr++)
      dest_address = Inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str));

    // int flag = 0;
    char msg[100] = "request time";

    msg_send(sockfd, dest_address, DEST_PORT, msg, flag);

    // msg_recv(sockfd,1111);
    alarm(2);
    char recvline[MAXLINE + 1];

    Sigprocmask(SIG_UNBLOCK, &sigset_alrm, NULL);
    int n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);
    Sigprocmask(SIG_BLOCK, &sigset_alrm, NULL);

    printf("\n\nclient at node %s sending request to server at %s\n\n",
           sourcevmName, vmName);

    if (n < 0) {
      if (errno == EINTR) {
        printf("\n\ntimeout!!!\n\n");
        printf("client at node: %s : timeout on response from %s  \n  ",
               sourcevmName, vmName);
        printf("\nforced a route rediscovery now\n ");
        printf("\n\nplease wait for 2 seconds for reply\n\n");

        char msg1[100] = "request time";

        msg_send(sockfd, dest_address, DEST_PORT, msg1, 1);

        alarm(2);
        // msg_recv(sockfd, 1111);

        char recvline2[MAXLINE + 1];

        Sigprocmask(SIG_UNBLOCK, &sigset_alrm, NULL);
        int nn = recvfrom(sockfd, recvline2, MAXLINE, 0, NULL, NULL);
        Sigprocmask(SIG_BLOCK, &sigset_alrm, NULL);

        if (nn < 0) {
          if (errno == EINTR) {
            printf("\n\ntimeout!!!\n\n");
            printf("client at node: %s : timeout again on response from %s    "
                   "<timestamp>\n  ",
                   sourcevmName, vmName);
            printf("\nthis destination may not be reachable now\n\n\n\n");
          }
        } else {

          printReceivingMsg(recvline2, vmName, sourcevmName);
        }

        continue;
      } else
        err_sys("recvfrom error");
    } else {

      printReceivingMsg(recvline, vmName, sourcevmName);
    }
  }

  exit(0);
}
