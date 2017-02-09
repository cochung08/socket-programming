/*client.c*/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {

  int socket_fd, file_fd;
  struct sockaddr_in netaddr;
  struct sockaddr_in fromaddr;
  struct sockaddr_in servaddr;
  socklen_t len = sizeof(struct sockaddr);

  memset(&netaddr, 0, len);
  netaddr.sin_family = AF_INET;
  netaddr.sin_port = htons(0);
  netaddr.sin_addr.s_addr = INADDR_ANY;

  if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Fail to create socket");
    return -1;
  }
  if (bind(socket_fd, (struct sockaddr *)&netaddr, sizeof(struct sockaddr)) ==
      -1) {
    perror("bind");
    exit(1);
  }

  memset(&servaddr, 0, len);
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(5000);
  char *currentServAddr = argv[1];
  // char interfaceSubnetAddress[INET_ADDRSTRLEN]=currentServAddr;
  inet_ntop(AF_INET, &servaddr.sin_addr.s_addr, currentServAddr,
            sizeof(currentServAddr));
  // servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");;

  if (sendto(socket_fd, "hello", 5, 0, (struct sockaddr *)&servaddr, len) ==
      -1) {
    perror("sendto");
    exit(1);
  }
  char buf[100] = {};
  if (recvfrom(socket_fd, buf, 100, 0, (struct sockaddr *)&fromaddr, &len) ==
      -1) {
    perror("recvfrom");
    exit(1);
  }
  //  printf("recvfrom ip %s : %d\n", inet_ntoa(fromaddr.sin_addr),
  //  ntohs(fromaddr.sin_port));
  // printf("buf=%s\n", buf);

  int i;
  for (i = 0; i < 10; i++) {
    char buf[100];
    int ret = sprintf(buf, "shit %d", i);

    if (sendto(socket_fd, buf, ret, 0, (struct sockaddr *)&fromaddr, len) ==
        -1) {
      perror("sendto");
      exit(1);
    }
    printf("send ok\n");
    sleep(1);
  }

  // sleep(5);
  close(socket_fd);

  return 0;
}
