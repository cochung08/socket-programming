//#include	"unp.h"
#include "sys/time.h"
#include "unpifiplus.h"
#include "unpthread.h"
#include <math.h>

struct Interface {
  int label;
  char address[INET_ADDRSTRLEN];
  char networkMask[INET_ADDRSTRLEN];
  char subnetAddress[INET_ADDRSTRLEN];
};

struct Parameter_of_print {
  SlidingWindow *recv_base;
  int *num_seg_in_buf_pointer;
  int *last_seg_acked_pointer;
  int *last_seg_in_transfer_pointer;
};

pthread_mutex_t ndone_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ndone_cond = PTHREAD_COND_INITIALIZER;

#define M 100
#define TIMER 5

int getNetworkMaskNum(char *);
void getBitString(char *, char *);

int getLongest(char *, char *);

void printbits(int n);
int get_bit(int n, int bitnum);

ssize_t Dg_send_recv(int, const void *, size_t, void *, size_t, const SA *,
                     socklen_t);

void dg_cli1(FILE *, int, int);

void dg_cli2(FILE *, int, const SA *, socklen_t);

void mydg_echo(int, SA *, socklen_t, struct sockaddr_in *);
extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

void print_data(SlidingWindow *, int *, int *);
void *print_data2(void *);

static void data_alarm(int);

static void after_time_wait(int);

static int pipefd[2];

void print_list(SlidingWindow *head) {
  printf("current receiving window ::: ");
  SlidingWindow *nodePtr = head;
  while (nodePtr->nextSeg != NULL) {
    nodePtr = nodePtr->nextSeg;
    printf("%d -- ", nodePtr->seg.sequence_num);
  }
}

static float configProb;
static int mean;
static char filename[MAXLINE];
static int max_window_size;

int main(int argc, char **argv) {
  // if (argc != 2)
  // 	err_quit("usage: udpcli <IPaddress>");
  // char* currentServAddr = argv[1];
  char currentServAddr[MAXLINE];
  int PORT_NUMBER;

  // int max_window_size ;
  int seed;

  char parameter[MAXLINE];
  char file_name[] = "client.in";
  FILE *fp;
  fp = fopen(file_name, "r");

  if (fp != NULL) {
    printf("read client.in:\n");
    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      strcpy(currentServAddr, parameter);

      if (currentServAddr[(strlen(currentServAddr) - 1)] == '\n') {
        currentServAddr[(strlen(currentServAddr) - 1)] = '\0';
      }

      printf("Server IP Address: %s\n", currentServAddr);
    }
    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      PORT_NUMBER = atoi(parameter);
      printf("Server Port Number: %d\n", PORT_NUMBER);
    }
    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      strcpy(filename, parameter);

      if (filename[(strlen(filename) - 1)] == '\n') {
        filename[(strlen(filename) - 1)] = '\0';
      }

      printf("filename to be transferred: %s\n", filename);
    }
    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      max_window_size = atoi(parameter);
      printf("Receiving sliding-window size: %d\n", max_window_size);
    }

    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      seed = atoi(parameter);
      // srand(seed);
      printf("Random generator seed value: %d\n", seed);
    }
    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      configProb = atof(parameter);
      printf("Probability : %f\n", configProb);
      configProb = 1 - configProb;
    }
    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      mean = atoi(parameter);
      printf("Mean: %d\n", mean);
    }

  } else {
    printf("Error\n");
    exit(1);
  }

  struct Interface interface;
  struct Interface interfaceArray[FD_SETSIZE];

  int k;
  for (k = 0; k < FD_SETSIZE; k++)
    interfaceArray[k].label = -1;

  int i, family, doaliases;

  family = AF_INET;
  doaliases = 1;
  struct ifi_info *ifi, *ifihead;
  struct sockaddr *getInterfaceSA;
  u_char *getInterfacePtr;

  double dropProb = 1;
  int maxfd;
  fd_set rset;

  // srand(time(0));
  srand(seed);

  for (ifihead = ifi = Get_ifi_info_plus(family, doaliases); ifi != NULL;
       ifi = ifi->ifi_next) {
    printf("%s: ", ifi->ifi_name);
    if (ifi->ifi_index != 0)
      printf("(%d) ", ifi->ifi_index);
    printf("<");
    /* *INDENT-OFF* */
    if (ifi->ifi_flags & IFF_UP)
      printf("UP ");
    if (ifi->ifi_flags & IFF_BROADCAST)
      printf("BCAST ");
    if (ifi->ifi_flags & IFF_MULTICAST)
      printf("MCAST ");
    if (ifi->ifi_flags & IFF_LOOPBACK)
      printf("LOOP ");
    if (ifi->ifi_flags & IFF_POINTOPOINT)
      printf("P2P ");
    printf(">\n");
    /* *INDENT-ON* */

    if ((i = ifi->ifi_hlen) > 0) {
      getInterfacePtr = ifi->ifi_haddr;
      do {
        printf("%s%x", (i == ifi->ifi_hlen) ? "  " : ":", *getInterfacePtr++);
      } while (--i > 0);
      printf("\n");
    }
    if (ifi->ifi_mtu != 0)
      printf("  MTU: %d\n", ifi->ifi_mtu);

    //		char *serverAddressStr;
    char interfaceIPAddressStr[INET_ADDRSTRLEN];
    //		char *interfaceIPAddressStr;
    in_addr_t uintaddr;
    if ((getInterfaceSA = ifi->ifi_addr) != NULL) {
      //			memset(interfaceIPAddressStr, 0, sizeof
      //interfaceIPAddressStr);
      //			interfaceIPAddressStr = (char*) malloc(
      //								strlen(
      //										Sock_ntop_host(getInterfaceSA,
      //												sizeof(*getInterfaceSA)))
      //* sizeof(char));
      //						;

      strcpy(interfaceIPAddressStr,
             Sock_ntop_host(getInterfaceSA, sizeof(*getInterfaceSA)));
      printf("  IP addr: %s\n", interfaceIPAddressStr);

      inet_pton(AF_INET, interfaceIPAddressStr, &uintaddr);
    }

    /*=================== cse 533 Assignment 2 modifications
     * ======================*/

    //		char interfaceNetworkMask[INET_ADDRSTRLEN];
    char *interfaceNetworkMask;
    in_addr_t uintmask;
    if ((getInterfaceSA = ifi->ifi_ntmaddr) != NULL) {

      interfaceNetworkMask = (char *)malloc(
          strlen(Sock_ntop_host(getInterfaceSA, sizeof(*getInterfaceSA))) *
          sizeof(char));
      ;
      strcpy(interfaceNetworkMask,
             Sock_ntop_host(getInterfaceSA, sizeof(*getInterfaceSA)));
      printf("  network mask: %s\n", interfaceNetworkMask);
      // uintmask = inet_addr(networkMask);
      inet_pton(AF_INET, interfaceNetworkMask, &uintmask);
    }

    in_addr_t uintsubnetAddress = uintaddr & uintmask;
    struct in_addr subnetAddressIn_addr;
    subnetAddressIn_addr.s_addr = uintsubnetAddress;
    // char *subnetAddress = inet_ntoa(tmp);
    char interfaceSubnetAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &subnetAddressIn_addr, interfaceSubnetAddress,
              sizeof(interfaceSubnetAddress));

    printf("  subnetAddress: %s\n\n", interfaceSubnetAddress);

    // if ((getInterfaceSA = ifi->ifi_brdaddr) != NULL)
    // 	printf("  broadcast addr: %s\n",
    // 			Sock_ntop_host(getInterfaceSA,
    // sizeof(*getInterfaceSA)));
    // if ((getInterfaceSA = ifi->ifi_dstaddr) != NULL)
    // 	printf("  destination addr: %s\n",
    // 			Sock_ntop_host(getInterfaceSA,
    // sizeof(*getInterfaceSA)));

    int j;
    for (j = 0; j < FD_SETSIZE; j++)
      if (interfaceArray[j].label < 0) {
        interfaceArray[j].label = 1;
        strcpy(interfaceArray[j].address, interfaceIPAddressStr);
        strcpy(interfaceArray[j].networkMask, interfaceNetworkMask);
        strcpy(interfaceArray[j].subnetAddress, interfaceSubnetAddress);

        //				interfaceArray[j].address =
        //interfaceIPAddressStr;
        //				interfaceArray[j].networkMask =
        //interfaceNetworkMask;
        //				interfaceArray[j].subnetAddress =
        //interfaceSubnetAddress;

        break;
      }
  }

  int longestInt = -1;
  char clientAddrStr[INET_ADDRSTRLEN];
  if (strcmp(currentServAddr, "127.0.0.1") == 0) {
    strcpy(clientAddrStr, "127.0.0.1");
    printf("server and client are in the same localhost, loopback address is "
           "used\n");
  } else {

    int index = -1;
    int w;
    for (w = 0; w < FD_SETSIZE; w++) {
      if (interfaceArray[w].label > 0) {
        //				printf("%d\n", w);
        //				printf("%s\n",
        //interfaceArray[w].address);
        //				printf("%s\n",
        //interfaceArray[w].networkMask);
        //				printf("%s\n",
        //interfaceArray[w].subnetAddress);

        int networkMaskNum = getNetworkMaskNum(interfaceArray[w].networkMask);
        //				printf("networkMaskNum: %d\n",
        //networkMaskNum);
        int tmpLong = getLongest(interfaceArray[w].address, currentServAddr);
        //				printf("longestPrefix: %d\n", tmpLong);

        if (tmpLong >= networkMaskNum && tmpLong > longestInt) {
          longestInt = tmpLong;
          index = w;

          //					printf("%s\n", "match");
          //					printf("index: %d\n", w);
          //					printf("address: %s\n",
          //interfaceArray[w].address);
          //					printf("networkMask: %s\n",
          //interfaceArray[w].networkMask);
          //					printf("subnetAddress: %s\n",
          //							interfaceArray[w].subnetAddress);
          //					printf("currentServAddr: %s\n\n",
          //currentServAddr);
        }

      } else
        break;
    }

    if (longestInt == 32) {
      printf("\n\nclient and server in the same localhost\n\n");
      strcpy(currentServAddr, "127.0.0.1");
      strcpy(clientAddrStr, "127.0.0.1");
      printf("switch to loopback address:  %s\n\n", currentServAddr);

    } else if (longestInt != -1) {
      strcpy(clientAddrStr, interfaceArray[index].address);

      printf("%s: %d\n", "longestPrefix", longestInt);
      // printf("longestIndex: %d\n", index);
      printf("\n\nclient and server in the same subnet\n\n");
      printf("currentServAddr: %s\n", currentServAddr);
      printf("client interface address: %s\n", interfaceArray[index].address);
      printf("networkMask: %s\n", interfaceArray[index].networkMask);
      printf("subnetAddress: %s\n", interfaceArray[index].subnetAddress);
    } else {

      int h;
      for (h = 0; h < FD_SETSIZE; h++) {
        if (interfaceArray[h].label > 0)
          if (strcmp(interfaceArray[h].address, "127.0.0.1") != 0) {
            strcpy(clientAddrStr, interfaceArray[h].address);
            printf("\nnot local server, pick a random client interface\n\n");
            printf("currentServAddr: %s\n\n", currentServAddr);
            printf("client interface address: %s\n", interfaceArray[h].address);
            printf("networkMask: %s\n", interfaceArray[h].networkMask);
            printf("subnetAddress: %s\n", interfaceArray[h].subnetAddress);
            break;
          }
      }
    }

    // currentServAddr = interfaceArray[index].address;
  }

  free_ifi_info_plus(ifihead);

  int on = 1;
  int sockfd;
  sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in clientSockaddr_in;

  if (longestInt == 32) {
    // printf("\n\nclient and server in the same localhost\n\n");
    // currentServAddr="127.0.0.1";
    // printf("switch to loopback address:  %s\n\n",currentServAddr);

  } else if (longestInt != -1)
    if (setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on)) < 0) {
      printf("error setting socket option: %s\n", strerror(errno));
      exit(1);
    } else {
      printf("\nset socket not to route successfully\n\n");
    }

  memset(&clientSockaddr_in, 0, sizeof(struct sockaddr_in));
  clientSockaddr_in.sin_family = AF_INET;
  //		clientSockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
  Inet_pton(AF_INET, clientAddrStr, &clientSockaddr_in.sin_addr.s_addr);
  clientSockaddr_in.sin_port = htons(0);

  if (bind(sockfd, (struct sockaddr *)&clientSockaddr_in,
           sizeof(struct sockaddr_in)) == -1) {
    close(sockfd);
    exit(1);
  }

  int client_len = sizeof(clientSockaddr_in);

  if (getsockname(sockfd, (SA *)&clientSockaddr_in, &client_len) == -1) {
    perror("getsockname() failed");
    exit(1);
  }

  char clientIP[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &clientSockaddr_in.sin_addr, clientIP, sizeof clientIP);
  printf("\n\nSelected client IP address is : %s\n", clientIP);
  printf("client port is: %d\n", (int)ntohs(clientSockaddr_in.sin_port));

  struct sockaddr_in servSockaddr_in;
  bzero(&servSockaddr_in, sizeof(servSockaddr_in));
  servSockaddr_in.sin_family = AF_INET;
  // servaddr.sin_port = htons(SERV_PORT);
  servSockaddr_in.sin_port = htons(PORT_NUMBER);
  Inet_pton(AF_INET, currentServAddr, &servSockaddr_in.sin_addr);

  Connect(sockfd, (SA *)&servSockaddr_in, sizeof(servSockaddr_in));

  socklen_t lenSA;
  struct sockaddr_storage addr;
  char ipstr[INET6_ADDRSTRLEN];
  int portSA;

  lenSA = sizeof addr;
  getpeername(sockfd, (struct sockaddr *)&addr, &lenSA);

  // deal with both IPv4 and IPv6:
  if (addr.ss_family == AF_INET) {
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    portSA = ntohs(s->sin_port);
    inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
  } else { // AF_INET6
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
    portSA = ntohs(s->sin6_port);
    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
  }

  printf("Server IP address: %s\n", ipstr);
  printf("Server port      : %d\n", portSA);

  // The client sends a datagram to the server giving the filename for the
  // transfer. This send needs to be
  // backed up by a timeout in case the datagram is lost.

  int n;
  int success = 0;
  char sendline[MAXLINE], recvline[MAXLINE + 1], acknowledgement[MAXLINE];

  // int sockfd;

  Pipe(pipefd);
  maxfd = max(sockfd, pipefd[0]) + 1;
  FD_ZERO(&rset);
  Signal(SIGALRM, data_alarm);

  while (success == 0) {
    // printf("requested filename :  %s\n",filename);
    // printf("enter filename :  \n");
    // char filename1[MAXLINE];
    // if (Fgets(filename1, MAXLINE, stdin) != NULL) {
    double temp = (double)rand() / RAND_MAX;
    printf("%f\n", temp);
    if (temp < dropProb) {
      Write(sockfd, filename, strlen(filename));
      printf("sending request for %s\n", filename);
    }

    alarm(TIMER);
    for (;;) {
      int t;
      FD_SET(sockfd, &rset);
      FD_SET(pipefd[0], &rset);
      if ((t = select(maxfd, &rset, NULL, NULL, NULL)) < 0) {
        if (errno == EINTR) {
          continue;
        } else {
          err_sys("select error");
        }
      }

      if (FD_ISSET(pipefd[0], &rset)) {
        printf("no reply from server, please try again\n");
        Read(pipefd[0], &n, 1);
        break;
      }

      if (FD_ISSET(sockfd, &rset)) {
        n = Read(sockfd, recvline, MAXLINE);

        printf("recvfrom ip %s : %d\n", ipstr, portSA);

        // char str[INET_ADDRSTRLEN];
        //  const char *serverAddress = inet_ntop(AF_INET, &servaddr.sin_addr,
        //  str, sizeof(str));
        //
        // printf("recvfrom ip %s : %d\n", serverAddress,
        // 		ntohs(servaddr.sin_port));

        recvline[n] = 0; /* null terminate */
        // Fputs(recvline, stdout);

        printf("new server connection socket port: %s\n", recvline);
        success = 1;
        // alarm(0);
        break;
      }
    }
    //}
    alarm(0);
    // test
    if (success == 1) {
      // sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

      char *tmpptr;
      int connectionSocketPort = strtol(recvline, &tmpptr, 10);

      servSockaddr_in.sin_port = htons(connectionSocketPort);

      Connect(sockfd, (SA *)&servSockaddr_in, sizeof(servSockaddr_in));
      printf("switch to connection sokcet\n");

      getpeername(sockfd, (struct sockaddr *)&addr, &lenSA);

      // deal with both IPv4 and IPv6:
      if (addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        portSA = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
      } else { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        portSA = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
      }

      printf("New Connection socket IP address: %s\n", ipstr);
      printf("New Connection socket port      : %d\n", portSA);

      // printf("enter your acknowledgement\n");

      // if (Fgets(acknowledgement, MAXLINE, stdin) != NULL) {
      strncpy(acknowledgement, "ACK", 3);
      acknowledgement[3] = '\0';
      double temp = (double)rand() / RAND_MAX;
      printf("%f\n", temp);
      if (temp < dropProb) {
        Write(sockfd, acknowledgement, strlen(acknowledgement));
      }

      for (;;) {
        int t;
        FD_SET(sockfd, &rset);
        FD_SET(pipefd[0], &rset);
        int t_maxfd = max(maxfd, sockfd) + 1;
        alarm(TIMER);
        if ((t = select(t_maxfd, &rset, NULL, NULL, NULL)) < 0) {
          if (errno == EINTR) {
            continue;
          } else {
            err_sys("select error");
          }
        }

        if (FD_ISSET(pipefd[0], &rset)) {
          printf("no reply from server, please try again\n");
          Read(pipefd[0], &n, 1);
          success = 0;
          Close(sockfd);
          printf("closing sockfd: %d\n", sockfd);
          break;
        }

        if (FD_ISSET(sockfd, &rset)) {
          n = Read(sockfd, recvline, MAXLINE);

          printf("recvfrom ip %s : %d\n", ipstr, portSA);

          recvline[n] = 0;

          // printf("new server connection socket port: %s\n",
          // 		recvline);
          success = 1;

          recvline[n] = 0; /* null terminate */

          printf("Your acknowledgement is: %s\n", recvline);
          //		Fputs(recvline, stdout);
          break;
        }
      }
      alarm(0);
      //}
    }
  }

  printf("starting to receive file from server\n");

  dg_cli1(stdin, sockfd, max_window_size);

  exit(0);
}

void dg_cli1(FILE *fp, int sockfd, int max_window_size) {

  unsigned char recvBuf[UDP_SEGMENT_SIZE];

  SlidingWindow *recv_base = malloc(sizeof(SlidingWindow));
  memset(recv_base, 0, sizeof(SlidingWindow));
  // SlidingWindow* last_seg = recv_base;
  int num_seg_in_buf = 0;
  int last_seg_acked = 0;
  int last_seg_in_transfer = -1;

  int receive_complete = 0;

  pthread_t tid;
  struct Parameter_of_print *para_of_print =
      malloc(sizeof(struct Parameter_of_print));
  para_of_print->recv_base = recv_base;
  para_of_print->num_seg_in_buf_pointer = &num_seg_in_buf;
  para_of_print->last_seg_acked_pointer = &last_seg_acked;
  para_of_print->last_seg_in_transfer_pointer = &last_seg_in_transfer;

  Pthread_create(&tid, NULL, &print_data2, para_of_print);

  while (receive_complete == 0) {
    /* first send a request packet to server with advertising window */

    Segment request_seg;
    memset(&request_seg, 0, sizeof request_seg);

    request_seg.ack_num = last_seg_acked + 1;
    request_seg.rwnd = max_window_size - num_seg_in_buf;

    if (last_seg_in_transfer == last_seg_acked) {
      receive_complete = 1;
      request_seg.is_complete = 1;
      request_seg.ack_num = last_seg_in_transfer;
    }

    // --------- convert seg to string ---------
    int copyIndex = 0;
    unsigned char segBuf[UDP_SEGMENT_SIZE];

    unsigned char converter[4];
    int temp;

    converter[0] = (request_seg.sequence_num >> 24) & 0xFF;
    converter[1] = (request_seg.sequence_num >> 16) & 0xFF;
    converter[2] = (request_seg.sequence_num >> 8) & 0xFF;
    converter[3] = request_seg.sequence_num & 0xFF;

    for (temp = 0; temp < 4; temp++) {
      segBuf[copyIndex++] = converter[temp];
    }
    converter[0] = (request_seg.ack_num >> 24) & 0xFF;
    converter[1] = (request_seg.ack_num >> 16) & 0xFF;
    converter[2] = (request_seg.ack_num >> 8) & 0xFF;
    converter[3] = request_seg.ack_num & 0xFF;

    for (temp = 0; temp < 4; temp++) {
      segBuf[copyIndex++] = converter[temp];
    }

    segBuf[copyIndex++] = request_seg.is_eof;
    segBuf[copyIndex++] = request_seg.is_probe;
    segBuf[copyIndex++] = request_seg.is_complete;
    segBuf[copyIndex++] = request_seg.is_resent;

    converter[0] = (request_seg.rwnd >> 24) & 0xFF;
    converter[1] = (request_seg.rwnd >> 16) & 0xFF;
    converter[2] = (request_seg.rwnd >> 8) & 0xFF;
    converter[3] = request_seg.rwnd & 0xFF;

    for (temp = 0; temp < 4; temp++) {
      segBuf[copyIndex++] = converter[temp];
    }

    converter[0] = (request_seg.timestamp >> 24) & 0xFF;
    converter[1] = (request_seg.timestamp >> 16) & 0xFF;
    converter[2] = (request_seg.timestamp >> 8) & 0xFF;
    converter[3] = request_seg.timestamp & 0xFF;

    for (temp = 0; temp < 4; temp++) {
      segBuf[copyIndex++] = converter[temp];
    }

    segBuf[copyIndex++] = request_seg.is_last_in_iter;

    int j;
    for (j = 0; j < UDP_PAYLOAD_SIZE; j++) {
      segBuf[copyIndex++] = request_seg.payload[j];
    }

    // ---------- end conversion ---------------------

    double temp_prob = (double)rand() / RAND_MAX;
    printf("send temp_prob:%f :: %s\n", temp_prob,
           (temp_prob < configProb) ? "success" : "fail");
    if (temp_prob < configProb) {
      Send(sockfd, segBuf, sizeof segBuf, 0);
      printf("request segment sent with ACK %d and rwnd %d\n",
             request_seg.ack_num, request_seg.rwnd);
    }

    if (last_seg_in_transfer == last_seg_acked) {
      continue;
    }

    /*
     we accept as many packets as we advertised for
     some exceptions:
     1. the ack was lost - the server might send a dup packet
     stop checking for more packets since server only retransmit once
     2. the ack contains rwnd == 0 - the client will wait for a probe segment
     that is sent by the server
     3. (normal case) - accept as many packets as we advertised for
     for every packet, check our sliding window, and add to window
     if it wasn't buffered yet, updates the ack value to send
     */

    int init_rwnd = request_seg.rwnd;
    int probe = 0;
    if (init_rwnd == 0) {
      probe = 1;
    }
    /* we should listen to socket if we expect data packets, or probing packets
     */
    while (init_rwnd > 0 || (init_rwnd == 0 && probe == 1)) {
      if (last_seg_in_transfer == last_seg_acked) {
        break;
      }

      Segment seg;
      memset(&seg, 0, sizeof seg);

      Read(sockfd, recvBuf, sizeof(recvBuf));

      int byteRead = 0;

      // unsigned char converter[4];
      // int temp;

      for (temp = 0; temp < 4; temp++) {
        converter[temp] = recvBuf[byteRead++];
      }
      int sequence_num = (converter[0] << 24 | converter[1] << 16 |
                          converter[2] << 8 | converter[3]);
      seg.sequence_num = sequence_num;

      for (temp = 0; temp < 4; temp++) {
        converter[temp] = recvBuf[byteRead++];
      }
      int ack_num = (converter[0] << 24 | converter[1] << 16 |
                     converter[2] << 8 | converter[3]);
      seg.ack_num = ack_num;

      char is_eof = recvBuf[byteRead++];
      char is_probe = recvBuf[byteRead++];
      char is_complete = recvBuf[byteRead++];
      char is_resent = recvBuf[byteRead++];

      seg.is_eof = is_eof;
      seg.is_probe = is_probe;
      seg.is_complete = is_complete;
      seg.is_resent = is_resent;

      for (temp = 0; temp < 4; temp++) {
        converter[temp] = recvBuf[byteRead++];
      }
      int rwnd = (converter[0] << 24 | converter[1] << 16 | converter[2] << 8 |
                  converter[3]);
      seg.rwnd = rwnd;

      for (temp = 0; temp < 4; temp++) {
        converter[temp] = recvBuf[byteRead++];
      }
      int timestamp = (converter[0] << 24 | converter[1] << 16 |
                       converter[2] << 8 | converter[3]);
      seg.timestamp = timestamp;

      seg.is_last_in_iter = recvBuf[byteRead++];

      int j;
      for (j = 0; j < UDP_PAYLOAD_SIZE; j++) {
        seg.payload[j] = recvBuf[byteRead++];
      }

      double temp_prob = (double)rand() / RAND_MAX;
      printf("receive temp_prob:%f :: %s\n", temp_prob,
             (temp_prob < configProb) ? "success" : "fail");
      if (temp_prob < configProb) {
        if (seg.sequence_num == -1 && seg.ack_num == -1) {
          printf("server reply: file not found\n");
          last_seg_acked = -1;
          exit(2);
        }

        if (seg.is_probe > 0) {
          break;
        }

        /* if we have already ack this segment, just ignore it */
        printf("received segment number %d\n", seg.sequence_num);

        if (seg.sequence_num > last_seg_acked) {

          if (pthread_mutex_lock(&ndone_mutex) != 0) {
            printf("Lock error : %s\n", strerror(errno));
            exit(1);
          };

          if (num_seg_in_buf < max_window_size) {

            /* insert segment into sliding window, if it is not a duplicate */
            SlidingWindow *next = malloc(sizeof(SlidingWindow));
            memset(next, 0, sizeof(SlidingWindow));
            next->seg = seg;

            SlidingWindow *nodePtr = recv_base;
            while (nodePtr->nextSeg != NULL) {
              if (nodePtr->nextSeg->seg.sequence_num == seg.sequence_num) {
                break;
              } else if (nodePtr->nextSeg->seg.sequence_num >
                         seg.sequence_num) {
                next->nextSeg = nodePtr->nextSeg;
                nodePtr->nextSeg = next;
                num_seg_in_buf++;
                break;
              }
              nodePtr = nodePtr->nextSeg;
            }

            if (nodePtr->nextSeg == NULL) {
              nodePtr->nextSeg = next;
              num_seg_in_buf++;
            }

            if (num_seg_in_buf == max_window_size) {
              printf("maximum window size has been reached\n");
            }
          }

          /*last_seg->nextSeg = next;
           last_seg = next;*/

          if (pthread_mutex_unlock(&ndone_mutex) != 0) {
            printf("Unlock error :%s\n", strerror(errno));
            exit(1);
          }
        }

        /* packet received was the last one */
        if (seg.is_eof > 0) {
          // receive_complete = 1;
          last_seg_in_transfer = seg.sequence_num;
          break;
        }

        /* packet received was a duplicate */
        if (seg.is_resent > 0) {
          break;
        }

        if (seg.is_last_in_iter > 0) {
          break;
        }
      }

      // printf("Data for segment number %d:\n%s\n", seg.sequence_num,
      // seg.payload);

      init_rwnd--;
    }

    /* prints all in order segments in the window, should be a new thread */

    // print_list(recv_base);
    // 		pthread_t tid;
    // 		struct Parameter_of_print* para_of_print = malloc(sizeof(struct
    // Parameter_of_print));
    // 	para_of_print->recv_base=recv_base;
    // 	para_of_print->num_seg_in_buf_pointer=&num_seg_in_buf;
    // 	para_of_print->last_seg_acked_pointer=&last_seg_acked;
    //
    //
    //
    //
    // Pthread_create(&tid, NULL, &print_data2, para_of_print);
    //	print_data(recv_base, &num_seg_in_buf, &last_seg_acked);
    print_list(recv_base);
    printf("\nlast_seg_acked: %d\n", last_seg_acked);
    /*if(num_seg_in_buf == 0){
     last_seg = recv_base;
     }*/
  }

  free(recv_base);
  /*
   struct sockaddr_in servaddr;
   socklen_t len = sizeof(struct sockaddr);

   int n;
   char sendline[MAXLINE], recvline[MAXLINE + 1];

   while (Fgets(sendline, MAXLINE, fp) != NULL) {

   Write(sockfd, sendline, strlen(sendline));
   n = Read(sockfd, recvline, MAXLINE);

   recvline[n] = 0;
   Fputs(recvline, stdout);
   }
   */

  pthread_join(tid, NULL);

  printf("\ntimewait_state starts\n\n");
  // test

  //
  Sigfunc *sigfunc;
  int n;
  sigfunc = Signal(SIGALRM, after_time_wait);
  if (alarm(15) != 0)
    err_msg("Read_timeo: alarm was already set");

  while (1) {
    if ((n = read(sockfd, recvBuf, sizeof(recvBuf))) < 0) {
      close(sockfd);
      if (errno == EINTR) {
        errno = ETIMEDOUT;
        printf("\ntime is up  for timewaiting status\n");
        break;
      }
    } else {

      Segment request_seg;
      memset(&request_seg, 0, sizeof request_seg);

      request_seg.ack_num = last_seg_acked + 1;
      request_seg.rwnd = max_window_size - num_seg_in_buf;

      if (last_seg_in_transfer == last_seg_acked) {
        receive_complete = 1;
        request_seg.is_complete = 1;
        request_seg.ack_num = last_seg_in_transfer;
      }

      // --------- convert seg to string ---------
      int copyIndex = 0;
      unsigned char segBuf[UDP_SEGMENT_SIZE];

      unsigned char converter[4];
      int temp;

      converter[0] = (request_seg.sequence_num >> 24) & 0xFF;
      converter[1] = (request_seg.sequence_num >> 16) & 0xFF;
      converter[2] = (request_seg.sequence_num >> 8) & 0xFF;
      converter[3] = request_seg.sequence_num & 0xFF;

      for (temp = 0; temp < 4; temp++) {
        segBuf[copyIndex++] = converter[temp];
      }
      converter[0] = (request_seg.ack_num >> 24) & 0xFF;
      converter[1] = (request_seg.ack_num >> 16) & 0xFF;
      converter[2] = (request_seg.ack_num >> 8) & 0xFF;
      converter[3] = request_seg.ack_num & 0xFF;

      for (temp = 0; temp < 4; temp++) {
        segBuf[copyIndex++] = converter[temp];
      }

      segBuf[copyIndex++] = request_seg.is_eof;
      segBuf[copyIndex++] = request_seg.is_probe;
      segBuf[copyIndex++] = request_seg.is_complete;
      segBuf[copyIndex++] = request_seg.is_resent;

      converter[0] = (request_seg.rwnd >> 24) & 0xFF;
      converter[1] = (request_seg.rwnd >> 16) & 0xFF;
      converter[2] = (request_seg.rwnd >> 8) & 0xFF;
      converter[3] = request_seg.rwnd & 0xFF;

      for (temp = 0; temp < 4; temp++) {
        segBuf[copyIndex++] = converter[temp];
      }

      converter[0] = (request_seg.timestamp >> 24) & 0xFF;
      converter[1] = (request_seg.timestamp >> 16) & 0xFF;
      converter[2] = (request_seg.timestamp >> 8) & 0xFF;
      converter[3] = request_seg.timestamp & 0xFF;

      for (temp = 0; temp < 4; temp++) {
        segBuf[copyIndex++] = converter[temp];
      }

      int j;
      for (j = 0; j < UDP_PAYLOAD_SIZE; j++) {
        segBuf[copyIndex++] = request_seg.payload[j];
      }

      // ---------- end conversion ---------------------

      double temp_prob = (double)rand() / RAND_MAX;
      printf("send temp_prob:%f :: %s\n", temp_prob,
             (temp_prob < configProb) ? "success" : "fail");
      if (temp_prob < configProb) {
        Send(sockfd, segBuf, sizeof segBuf, 0);
        printf("request segment sent with ACK %d and rwnd %d\n",
               request_seg.ack_num, request_seg.rwnd);
      }
    }
  }

  alarm(0);
  Signal(SIGALRM, sigfunc);

  printf("(file transfer) printing completed!\n");
}

void dg_cli2(FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen) {
  printf("%s\n", "ss");
  ssize_t n;
  char sendline[MAXLINE], recvline[MAXLINE + 1];

  while (Fgets(sendline, MAXLINE, fp) != NULL) {

    n = Dg_send_recv(sockfd, sendline, strlen(sendline), recvline, MAXLINE,
                     pservaddr, servlen);

    recvline[n] = 0; /* null terminate */
    Fputs(recvline, stdout);
  }
}

int getLongest(char left[], char right[]) {

  char bitPtrLeft[33];
  char bitPtrRight[33];

  getBitString(left, bitPtrLeft);
  bitPtrLeft[32] = '\0';
  getBitString(right, bitPtrRight);
  bitPtrRight[32] = '\0';
  //	printf("getleftaddr: '%s\n", left);
  //	printf("getleftBitString: %s\n", bitPtrLeft);
  //	printf("getrightaddr: '%s\n", right);
  //	printf("getrightBitString: %s\n", bitPtrRight);

  int i;
  for (i = 0; i < 32; i++) {
    if (bitPtrLeft[i] != bitPtrRight[i])
      break;
  }
  return i;
}

int getNetworkMaskNum(char *str) {

  int subnetMaskNum = 0;
  struct in_addr ad;
  Inet_pton(AF_INET, str, &ad);
  uint32_t tmp = ad.s_addr;

  while (tmp) {
    int bitN = tmp & 1;

    if (bitN == 1)
      subnetMaskNum++;
    tmp >>= 1;
  }
  return subnetMaskNum;
}

void getBitString(char *str, char *bitPtr1) {

  // char *bitString = malloc(sizeof(char) * 32);
  struct in_addr ad;
  Inet_pton(AF_INET, str, &ad);
  uint32_t tmp1 = ad.s_addr;

  int n = htonl(tmp1);
  int j;

  for (j = 0; j < 32; j++) {
    bitPtr1[j] = get_bit(n, j) + '0';
    //			printf("%d", get_bit(n, j));
  }
  printf("\n");
}

void printbits(int a) {

  printf("hello\n");
  int n = htonl(a);
  int j;

  for (j = 0; j < 32; j++) {
    printf("%d", get_bit(n, j));
  }
  printf("\n");
}

int get_bit(int n, int bitnum) { return ((n >> (31 - bitnum)) & 1); }

static void data_alarm(int signo) {
  Write(pipefd[1], "", 1);
  return;
}

/* currently prints everything in the window, and clears everything that is
 printed,
 does not consider out of order packets yet */
void print_data(SlidingWindow *slide_win, int *num_seg_in_buf,
                int *last_seg_acked) {
  printf("*******************************************************");
  printf("VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV");
  SlidingWindow *nodePtr = slide_win;
  while (nodePtr->nextSeg != NULL) {

    Segment seg = nodePtr->nextSeg->seg;
    SlidingWindow *temp = nodePtr->nextSeg;
    nodePtr->nextSeg = nodePtr->nextSeg->nextSeg;
    printf("printing segment %d with data:\n%s\n", seg.sequence_num,
           seg.payload);
    if (*last_seg_acked < seg.sequence_num) {
      *last_seg_acked = seg.sequence_num;
    }
    free(temp);
    *num_seg_in_buf = *num_seg_in_buf - 1;
  }
  printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
  printf("*******************************************************");
}

void *print_data2(void *vptr) {

  struct timeval tv;
  struct timespec ts;
  float sleeptime;
  /*
          int ret;
          FILE *fp;

          char filename[] = "receivedFile.txt";

          fp = fopen(filename, "w");


          fclose(fp);

          ret = remove(filename);

          if (ret == 0) {
                  printf("File deleted successfully11\n\n\n");
          } else {
                  printf("Error: unable to delete the file");
          }


          FILE *pFile2;
          pFile2 = fopen(filename, "a");
          if (pFile2 == NULL) {
                  perror("Error opening file.");
          }
  */
  while (1) {

    /* Set the time to sleep */
    if (gettimeofday(&tv, NULL) < 0) {
      printf("Error gettimeofday :%s\n", strerror(errno));
      exit(1);
    }

    double random_value = (double)rand() / (double)RAND_MAX;
    while (random_value <= 0.000001) {
      random_value = (double)rand() / (double)RAND_MAX;
    }

    sleeptime = (-1) * mean * log(random_value);

    // tv.tv_usec is microseconds 10^6
    // sleeptime is milliseconds, 10^3
    // ts.tv_nsec is  nanoseconds 10^9
    int secPart = (int)sleeptime / 1000;
    int decimalPart = (int)sleeptime % 1000;

    ts.tv_sec = tv.tv_sec + secPart;
    ts.tv_nsec = tv.tv_usec * 1000;
    ts.tv_nsec = ts.tv_nsec + decimalPart * 1000;

    if (pthread_mutex_lock(&ndone_mutex) != 0) {
      printf("Lock error : %s\n", strerror(errno));
      exit(1);
    };
    /*
                    int curr_time = tv.tv_sec * 1000000 + tv.tv_usec +
       sleeptime;
                    int sec = curr_time / 1000000;
                    int nano_sec = curr_time % 1000000;

                    ts.tv_sec = sec;
                    ts.tv_nsec = nano_sec * 1000;
    */

    // ts.tv_sec = (int)sleeptime / 1000000;
    // ts.tv_nsec = (int)sleeptime % 1000000;

    printf("thread is sleeping,sleeptime :%d nanoseconds\n", sleeptime * 1000);
    pthread_cond_timedwait(&ndone_cond, &ndone_mutex, &ts);
    printf("consumer thread wake up now\n");

    struct Parameter_of_print *para_of_print = vptr;

    SlidingWindow *nodePtr = para_of_print->recv_base;
    int *num_seg_in_buf = para_of_print->num_seg_in_buf_pointer;
    int *last_seg_acked = para_of_print->last_seg_acked_pointer;
    int *last_seg_in_transfer = para_of_print->last_seg_in_transfer_pointer;

    while (nodePtr->nextSeg != NULL) {

      Segment seg = nodePtr->nextSeg->seg;
      if (seg.sequence_num == *last_seg_acked + 1) {
        SlidingWindow *temp = nodePtr->nextSeg;
        nodePtr->nextSeg = nodePtr->nextSeg->nextSeg;
        printf("printing segment %d with data:\n%.*s\n", seg.sequence_num,
               UDP_PAYLOAD_SIZE, seg.payload);

        // printf("check:%s",seg.payload);

        // fprintf(pFile2, seg.payload);

        if (*last_seg_acked < seg.sequence_num) {
          *last_seg_acked = seg.sequence_num;
        }
        free(temp);
        *num_seg_in_buf = *num_seg_in_buf - 1;
      } else {
        break;
      }
    }

    if (pthread_mutex_unlock(&ndone_mutex) != 0) {
      printf("Unlock error :%s\n", strerror(errno));
      exit(1);
    }

    if (*last_seg_in_transfer == *last_seg_acked) {
      break;
    }

    if (*last_seg_acked == -1) {
      break;
    }
  }
}

static void after_time_wait(int signo) { return; }
