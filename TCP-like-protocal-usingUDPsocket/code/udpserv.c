/* include udpserv1 */
//#include	"unpifi.h"
#include "unpifiplus.h"
#define TIMER 10
#define RTT_MAXNREXMT 12
#define RTTMIN 1000
#define RTTMAX 3000

struct Interface {

  int sockFd;

  char *address;
  char *networkMask;
  char *subnetAddress;
};

void sig_chld(int signo) {
  pid_t pid;
  int stat;

  while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    printf("child %d terminated\n\n", pid);
  return;
}

int getNetworkMaskNum(char *);
void getBitString(char *, char *);

int getLongest(char *, char *);

void printbits(int n);
int get_bit(int n, int bitnum);

void mydg_echo(int, struct sockaddr_in *, struct Interface *);
extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

void get_cwnd(int *, int *, int *, int *, int *, int *);

static void data_alarm(int);

static int pipefd[2];

static int maxBufferSize;

int main(int argc, char **argv) {

  int PORT_NUMBER;

  char parameter[MAXLINE];
  char file_name[] = "server.in";
  FILE *fp = fopen(file_name, "r");

  if (fp != NULL) {
    printf("read server.in:\n");

    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      PORT_NUMBER = atoi(parameter);
      printf("Server Port Number: %d\n", PORT_NUMBER);
    }

    if (fgets(parameter, sizeof(parameter), fp) != NULL) {

      maxBufferSize = atoi(parameter);
      printf("sending sliding-window size: %d\n", maxBufferSize);
    }

  } else {
    printf("Error\n");
    exit(1);
  }

  const int on = 1;
  pid_t pid;

  int k, maxi, maxfd;
  int nready, client[FD_SETSIZE], sockfd, tmpSockfd;
  ssize_t n;
  fd_set rset, allset;
  FD_ZERO(&allset);
  maxfd = -99;
  maxi = -1; /* index into client[] array */

  struct ifi_info *ifi, *ifihead;
  struct sockaddr *getInterfaceSA;
  u_char *getInterfacePtr;
  int i, family, doaliases;

  family = AF_INET;
  doaliases = 1;

  struct Interface interface;
  struct Interface interfaceArray[FD_SETSIZE];
  int index = 0;

  memset(interfaceArray, 0, sizeof interfaceArray);

  int max_window_size = 5;

  Pipe(pipefd);
  Signal(SIGALRM, data_alarm);

  for (k = 0; k < FD_SETSIZE; k++) {

    interfaceArray[k].sockFd = -1;
  }

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

    char *serverAddress;
    // uint32_t uintaddr;
    in_addr_t uintaddr;
    if ((getInterfaceSA = ifi->ifi_addr) != NULL) {
      serverAddress = (char *)malloc(
          strlen(Sock_ntop_host(getInterfaceSA, sizeof(*getInterfaceSA))) *
          sizeof(char));
      ;
      strcpy(serverAddress,
             Sock_ntop_host(getInterfaceSA, sizeof(*getInterfaceSA)));
      printf("  IP addr: %s\n", serverAddress);

      // uintaddr = inet_addr(serverAddress);
      inet_pton(AF_INET, serverAddress, &uintaddr);
    }

    /*=================== cse 533 Assignment 2 modifications
     * ======================*/

    char *networkMask;
    in_addr_t uintmask;
    if ((getInterfaceSA = ifi->ifi_ntmaddr) != NULL) {

      networkMask = (char *)malloc(
          strlen(Sock_ntop_host(getInterfaceSA, sizeof(*getInterfaceSA))) *
          sizeof(char));
      ;
      strcpy(networkMask,
             Sock_ntop_host(getInterfaceSA, sizeof(*getInterfaceSA)));
      printf("  network mask: %s\n", networkMask);
      // uintmask = inet_addr(networkMask);
      inet_pton(AF_INET, networkMask, &uintmask);
    }

    in_addr_t usubaddr = uintaddr & uintmask;
    struct in_addr tmp;
    tmp.s_addr = usubaddr;
    // char *subnetAddress = inet_ntoa(tmp);
    char subnetAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &tmp, subnetAddress, sizeof(subnetAddress));

    printf("  subnetAddress: %s\n", subnetAddress);

    /*=============================================================================*/

    // if ((getInterfaceSA = ifi->ifi_brdaddr) != NULL)
    // 	printf("  broadcast addr: %s\n",
    // 			Sock_ntop_host(getInterfaceSA,
    // sizeof(*getInterfaceSA)));
    // if ((getInterfaceSA = ifi->ifi_dstaddr) != NULL)
    // 	printf("  destination addr: %s\n",
    // 			Sock_ntop_host(getInterfaceSA,
    // sizeof(*getInterfaceSA)));

    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

    int j;
    for (j = 0; j < FD_SETSIZE; j++)
      if (interfaceArray[j].sockFd < 0) {

        interfaceArray[j].sockFd = sockfd;
        interfaceArray[j].address = serverAddress;
        interfaceArray[j].networkMask = networkMask;
        interfaceArray[j].subnetAddress = subnetAddress;
        break;
      }

    FD_SET(sockfd, &allset); /* add new descriptor to set */
    if (sockfd > maxfd)
      maxfd = sockfd; /* for select */
    if (j > maxi)
      maxi = j;

    Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverAddress, &servaddr.sin_addr.s_addr);

    servaddr.sin_port = htons(PORT_NUMBER);
    Bind(sockfd, (SA *)&servaddr, sizeof(servaddr));
    printf("bound %s\n", Sock_ntop((SA *)&servaddr, sizeof(servaddr)));
    // printf("  IP addr: %s\n",
    // 		Sock_ntop_host((SA*) &servaddr, sizeof(servaddr)));
  }

  free_ifi_info_plus(ifihead);

  for (;;) {
    rset = allset;
    int tmpSockfd;
    // nready = Select(maxfd + 1, &rset, NULL, NULL, NULL);

    int t1;
    if ((t1 = select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0) {
      if (errno == EINTR) {

        continue;
      } else {
        err_sys("select error");
      }
    }

    for (i = 0; i <= maxi; i++) { /* check all clients for data */

      if ((tmpSockfd = interfaceArray[i].sockFd) < 0)
        continue;
      if (FD_ISSET(tmpSockfd, &rset)) {

        char *tmpAddress;
        int p;
        for (p = 0; p < FD_SETSIZE; p++)
          if (interfaceArray[p].sockFd == tmpSockfd) {
            tmpAddress = interfaceArray[p].address;
            break;
          }

        struct sockaddr_in servaddr1;
        bzero(&servaddr1, sizeof(servaddr1));
        servaddr1.sin_family = AF_INET;
        inet_pton(AF_INET, tmpAddress, &servaddr1.sin_addr.s_addr);
        servaddr1.sin_port = htons(PORT_NUMBER);

        mydg_echo(tmpSockfd, &servaddr1, interfaceArray);

        if (--nready <= 0)
          break; /* no more readable descriptors */
      }
    }
  }

  exit(0);
}

/* include mydg_echo */
void mydg_echo(int listenningSockfd, struct sockaddr_in *listenningSA,
               struct Interface *interfaceArray) {

  // int hh;
  // for (hh = 0; hh < FD_SETSIZE; hh++) {
  // 		if (interfaceArray[hh].sockFd > 0) {
  // 	int off=0;
  //
  // 							if (setsockopt(interfaceArray[hh].sockFd, SOL_SOCKET,
  // SO_DONTROUTE,
  // 									&off, sizeof(off)) < 0)
  // {
  // 								printf("error setting socket option:
  // %s\n",
  // 										strerror(errno));
  // 								exit(1);
  // 							} else {
  // 								;
  // 							}
  // 						}
  // 					}

  int n;
  char mesg[MAXLINE];

  int connectionsSocketFd;
  if ((connectionsSocketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("child Fail to create socket");
    exit(1);
  }

  struct sockaddr_in *connectionSA;
  connectionSA = malloc(sizeof(struct sockaddr_in));
  *connectionSA = *listenningSA;
  connectionSA->sin_port = htons(0);

  Bind(connectionsSocketFd, (SA *)connectionSA, sizeof(*connectionSA));

  struct sockaddr_in serverAddress;
  int sa_len;

  sa_len = sizeof(serverAddress);

  if (getsockname(connectionsSocketFd, (SA *)&serverAddress, &sa_len) == -1) {
    perror("getsockname() failed");
    exit(1);
  }

  char connectionAddress[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &serverAddress.sin_addr, connectionAddress,
            sizeof(connectionAddress));

  printf("\n\nServer address is: %s\n", connectionAddress);
  printf("Server Connection Socket port is: %d\n\n",
         (int)ntohs(serverAddress.sin_port));

  char connectionsSockPortNum[30];
  sprintf(connectionsSockPortNum, "%d", (int)ntohs(serverAddress.sin_port));

  char filename[MAXLINE];

  struct sockaddr_in pcliaddr;
  socklen_t len = sizeof(struct sockaddr_in);

  int success = 0;

  while (success == 0) {
    printf("waiting for client to send filename\n");
    n = Recvfrom(listenningSockfd, filename, MAXLINE, 0, (SA *)&pcliaddr, &len);
    printf("parent server %d,filename: %s \ndatagram from %s", getpid(),
           filename, Sock_ntop((SA *)&pcliaddr, len));
    printf(", to %s\n\n",
           Sock_ntop((SA *)listenningSA, sizeof(*((SA *)listenningSA))));

    Sendto(listenningSockfd, connectionsSockPortNum,
           sizeof(connectionsSockPortNum), 0, (SA *)&pcliaddr, len);

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pcliaddr.sin_addr, clientIP, sizeof clientIP);

    int longestInt = -1;
    // char clientAddrStr[INET_ADDRSTRLEN];
    if (strcmp(connectionAddress, "127.0.0.1") == 0) {
      // strcpy(clientAddrStr, "127.0.0.1");
      printf("server and client are in the same localhost, loopback address is "
             "used\n");
    } else {

      int on = 1;
      int servSockfd;
      int index = -1;
      int w;
      for (w = 0; w < FD_SETSIZE; w++) {
        if (interfaceArray[w].sockFd > 0) {
          if (strcmp(connectionAddress, interfaceArray[w].address) == 0) {
            int networkMaskNum =
                getNetworkMaskNum(interfaceArray[w].networkMask);

            int tmpLong = getLongest(interfaceArray[w].address, clientIP);

            if (tmpLong >= networkMaskNum && tmpLong > longestInt) {
              longestInt = tmpLong;
              servSockfd = interfaceArray[w].sockFd;
              index = w;

              printf("\n\nclient and server in the same subnet\n");

              if (setsockopt(connectionsSocketFd, SOL_SOCKET, SO_DONTROUTE, &on,
                             sizeof(on)) < 0) {
                printf("error setting socket option: %s\n", strerror(errno));
                exit(1);
              } else {
                printf("set socket not to route successfully\n");
              }
              break;

              //						printf("%s\n",
              //"match");
              //						printf("index:
              //%d\n", w);
              //						printf("address:
              //%s\n", interfaceArray[w].address);
              //						printf("networkMask:
              //%s\n",
              //								interfaceArray[w].networkMask);
              //						printf("subnetAddress:
              //%s\n",
              //								interfaceArray[w].subnetAddress);
              //						printf("currentClientIP:
              //%s\n\n", clientIP);
            }
          }
        }
      }
    }

    char acknowledgement[MAXLINE];
    len = sizeof(struct sockaddr_in);

    fd_set timeout_fdset;
    FD_ZERO(&timeout_fdset);
    int time_maxfd = max(connectionsSocketFd, pipefd[0]) + 1;

    for (;;) {
      int t;
      FD_SET(connectionsSocketFd, &timeout_fdset);
      FD_SET(pipefd[0], &timeout_fdset);

      alarm(TIMER);

      if ((t = select(time_maxfd, &timeout_fdset, NULL, NULL, NULL)) < 0) {
        if (errno == EINTR) {
          continue;
        } else {
          err_sys("select error");
        }
      }

      if (FD_ISSET(pipefd[0], &timeout_fdset)) {
        printf("no reply from client, please try again\n");
        Read(pipefd[0], &n, 1);
        success = 0;
        break;
      }

      if (FD_ISSET(connectionsSocketFd, &timeout_fdset)) {
        t = Recvfrom(connectionsSocketFd, acknowledgement, MAXLINE, 0,
                     (SA *)&pcliaddr, &len);

        printf("\nselected client:  %s\n", Sock_ntop((SA *)&pcliaddr, len));
        printf("parent server %d, datagram from %s", getpid(),
               Sock_ntop((SA *)&pcliaddr, len));
        printf(", to %s\n\n",
               Sock_ntop((SA *)listenningSA, sizeof(*((SA *)listenningSA))));

        Sendto(connectionsSocketFd, acknowledgement, t, 0, (SA *)&pcliaddr,
               len);

        success = 1;
        break;
      }
    }
    alarm(0);
  }

  Signal(SIGCHLD, sig_chld);

  int pid;
  if ((pid = Fork()) == 0) { /* child */

    /* dummy data from server to client */
    // char dataToSend[10000];
    // int i;
    // int value = 64;
    // for (i = 0; i < 10000; i++) {
    // 	if (i % UDP_PAYLOAD_SIZE == 0) {
    // 		value++;
    // 	}
    // 	dataToSend[i] = (char) value;
    // }
    char *dataToSend = 0;
    long length;
    FILE *f = fopen(filename, "rb");
    int file_exists = 1;

    if (f) {
      fseek(f, 0, SEEK_END);
      length = ftell(f);
      fseek(f, 0, SEEK_SET);
      dataToSend = malloc(length);
      if (dataToSend) {
        fread(dataToSend, 1, length, f);
      }
      fclose(f);
    } else {
      printf("file \"%s\" cannot be found\nchild server terminating...\n",
             filename);
      // exit(2);
      file_exists = 0;
    }

    // printf("%s", dataToSend);

    int copyByte = 0;

    len = sizeof(struct sockaddr_in);
    SlidingWindow *send_base = malloc(sizeof(SlidingWindow));
    // memset(&(send_base->seg), 0, sizeof(Segment));
    memset(send_base, 0, sizeof(SlidingWindow));
    // SlidingWindow last_seg = send_base;
    int num_seg_in_buf = 0;

    // int seq_num_count = 0;
    int last_seg_acked = 0;
    fd_set listen_set;

    int measuredRTT = 0;
    int srtt = 1500000;
    int rttvar = 1500000;
    int RTO;

    int congestion_state = 1;
    int cwnd = 1;
    int ssthresh = 8;
    int prev_ack_seg_num = 0;
    int arrive_ack_seg;
    int num_dup_count = 0;

    struct timeval init_time;
    gettimeofday(&init_time, NULL);
    int last_out_time = init_time.tv_sec * 1000000 + init_time.tv_usec;
    struct timeval out_time;

    int last_packet_drop = 0;
    int conse_packet_drop = 0;
    int update_rtt = 1;

    int sent_complete = 0;
    while (sent_complete == 0) {
      printf("\n");
      if (conse_packet_drop == RTT_MAXNREXMT) {
        printf("maximum number of retransmission reached... aborting\n");
        free(dataToSend);
        exit(1);
      }
      char request[UDP_SEGMENT_SIZE];

      // if(conse_packet_drop == RTT_MAXNREXMT){
      // 	printf("maximum number of retransmission reached...
      // aborting\n");
      // 	exit(1);
      // }

      Segment request_seg;
      memset(&request_seg, 0, sizeof request_seg);

      if (last_packet_drop == 0) {
        if (update_rtt == 1) {
          measuredRTT -= (srtt >> 3);
          srtt += measuredRTT;
          if (measuredRTT < 0) {
            measuredRTT = -measuredRTT;
          }
          measuredRTT -= (rttvar >> 2);
          rttvar += measuredRTT;
          RTO = (srtt >> 3) + rttvar;

          if (RTO < RTTMIN * 1000) {
            RTO = RTTMIN * 1000;
          } else if (RTO > RTTMAX * 1000) {
            RTO = RTTMAX * 1000;
          }

        } else {
          update_rtt = 1;
        }
      }

      struct timeval tv;

      int trans_time = (RTO * pow(2, conse_packet_drop));

      if (trans_time > RTTMAX * 1000) {
        trans_time = RTTMAX * 1000;
      }

      tv.tv_sec = trans_time / 1000000;
      tv.tv_usec = trans_time % 1000000;

      // printf("time RTO is: %d\n", RTO + pow(2, conse_packet_drop));
      // printf("tv_sec: %d, tv_usecL %d\n", tv.tv_sec, tv.tv_usec);

      // for testing time_wait
      // tv.tv_sec = (RTO / 2) / 1000000;
      // tv.tv_usec = (RTO / 2) % 1000000;

      printf("conse_packet_drop: %d\n", conse_packet_drop);
      printf("time RTO is: %d microseconds\n", trans_time);
      // printf("tv_sec: %d, tv_usec: %d\n", tv.tv_sec, tv.tv_usec);

      int nready;
      FD_ZERO(&listen_set);
      FD_SET(connectionsSocketFd, &listen_set);

      /* listens for a request segment from the client, timer is set */
      /* if timer goes off, the segment at the base of the sliding window is
       sent
       if the window is empty, send a dummy segment */

      if ((nready = select(connectionsSocketFd + 1, &listen_set, NULL, NULL,
                           &tv)) < 0) {
        if (errno == EINTR) {
          continue;
        } else {
          err_sys("Select error");
        }
      }

      /* timer went off */
      if (nready == 0) {
        last_packet_drop = 1;
        update_rtt = 0;
        conse_packet_drop++;

        ssthresh = cwnd / 2;
        if (ssthresh <= 0) {
          ssthresh = 1;
        }
        cwnd = 1;
        num_dup_count = 0;
        congestion_state = 1;

        printf("congestion state: %d\n", congestion_state);
        printf("ssthresh: %d\n", ssthresh);
        printf("duplicate packet count: %d\n", num_dup_count);

        Segment seg;
        if (send_base->nextSeg != NULL) {
          seg = send_base->nextSeg->seg;
        } else {
          seg = send_base->seg;
          seg.sequence_num = last_seg_acked;
        }
        seg.is_resent = 1;
        gettimeofday(&out_time, NULL);
        seg.timestamp = out_time.tv_sec * 1000000 + out_time.tv_usec;

        int copyIndex = 0;
        unsigned char segBuf[UDP_SEGMENT_SIZE];

        unsigned char converter[4];
        int temp;

        converter[0] = (seg.sequence_num >> 24) & 0xFF;
        converter[1] = (seg.sequence_num >> 16) & 0xFF;
        converter[2] = (seg.sequence_num >> 8) & 0xFF;
        converter[3] = seg.sequence_num & 0xFF;

        for (temp = 0; temp < 4; temp++) {
          segBuf[copyIndex++] = converter[temp];
        }
        converter[0] = (seg.ack_num >> 24) & 0xFF;
        converter[1] = (seg.ack_num >> 16) & 0xFF;
        converter[2] = (seg.ack_num >> 8) & 0xFF;
        converter[3] = seg.ack_num & 0xFF;

        for (temp = 0; temp < 4; temp++) {
          segBuf[copyIndex++] = converter[temp];
        }

        segBuf[copyIndex++] = seg.is_eof;
        segBuf[copyIndex++] = seg.is_probe;
        segBuf[copyIndex++] = seg.is_complete;
        segBuf[copyIndex++] = seg.is_resent;

        converter[0] = (seg.rwnd >> 24) & 0xFF;
        converter[1] = (seg.rwnd >> 16) & 0xFF;
        converter[2] = (seg.rwnd >> 8) & 0xFF;
        converter[3] = seg.rwnd & 0xFF;

        for (temp = 0; temp < 4; temp++) {
          segBuf[copyIndex++] = converter[temp];
        }

        converter[0] = (seg.timestamp >> 24) & 0xFF;
        converter[1] = (seg.timestamp >> 16) & 0xFF;
        converter[2] = (seg.timestamp >> 8) & 0xFF;
        converter[3] = seg.timestamp & 0xFF;

        for (temp = 0; temp < 4; temp++) {
          segBuf[copyIndex++] = converter[temp];
        }

        segBuf[copyIndex++] = seg.is_last_in_iter;

        int j;
        for (j = 0; j < UDP_PAYLOAD_SIZE; j++) {
          segBuf[copyIndex++] = seg.payload[j];
        }

        // Send(connectionsSocketFd, segBuf, sizeof(segBuf), 0);
        Sendto(connectionsSocketFd, segBuf, sizeof(segBuf), 0, (SA *)&pcliaddr,
               len);
        printf("segment timeout: resending segment %d\n", seg.sequence_num);
      } else if (FD_ISSET(connectionsSocketFd, &listen_set)) {
        /* get the user request */
        Read(connectionsSocketFd, request, sizeof(request));

        last_packet_drop = 0;
        conse_packet_drop = 0;
        gettimeofday(&out_time, NULL);
        int time_back = out_time.tv_sec * 1000000 + out_time.tv_usec;
        measuredRTT = time_back - last_out_time;

        int byteRead = 0;

        unsigned char converter[4];
        int temp;

        for (temp = 0; temp < 4; temp++) {
          converter[temp] = request[byteRead++];
        }
        int sequence_num = (converter[0] << 24 | converter[1] << 16 |
                            converter[2] << 8 | converter[3]);
        request_seg.sequence_num = sequence_num;

        for (temp = 0; temp < 4; temp++) {
          converter[temp] = request[byteRead++];
        }
        int ack_num = (converter[0] << 24 | converter[1] << 16 |
                       converter[2] << 8 | converter[3]);
        request_seg.ack_num = ack_num;

        char is_eof = request[byteRead++];
        char is_probe = request[byteRead++];
        char is_complete = request[byteRead++];
        char is_resent = request[byteRead++];

        request_seg.is_eof = is_eof;
        request_seg.is_probe = is_probe;
        request_seg.is_complete = is_complete;
        request_seg.is_resent = is_resent;

        for (temp = 0; temp < 4; temp++) {
          converter[temp] = request[byteRead++];
        }
        int rwnd = (converter[0] << 24 | converter[1] << 16 |
                    converter[2] << 8 | converter[3]);
        request_seg.rwnd = rwnd;

        for (temp = 0; temp < 4; temp++) {
          converter[temp] = request[byteRead++];
        }
        int timestamp = (converter[0] << 24 | converter[1] << 16 |
                         converter[2] << 8 | converter[3]);
        request_seg.timestamp = timestamp;

        request_seg.is_last_in_iter = request[byteRead++];

        int j;
        for (j = 0; j < UDP_PAYLOAD_SIZE; j++) {
          request_seg.payload[j] = request[byteRead++];
        }

        arrive_ack_seg = request_seg.ack_num;

        get_cwnd(&congestion_state, &cwnd, &ssthresh, &prev_ack_seg_num,
                 &arrive_ack_seg, &num_dup_count);

        printf("congestion state: %d\n", congestion_state);
        printf("ssthresh: %d\n", ssthresh);
        printf("duplicate packet count: %d\n", num_dup_count);

        /* depending on client feedback, we can shift the sliding window */
        printf("received request segment with ACK %d and rwnd %d\n",
               request_seg.ack_num, request_seg.rwnd);

        if (request_seg.is_complete > 0) {
          sent_complete = 1;
          printf("client has received and acked all segments\n");
          continue;
        }

        last_seg_acked = request_seg.ack_num - 1;
        prev_ack_seg_num = request_seg.ack_num;
        SlidingWindow *nodePtr = send_base;

        while (nodePtr->nextSeg != NULL) {
          if (nodePtr->nextSeg->seg.sequence_num <= last_seg_acked) {
            nodePtr->nextSeg = nodePtr->nextSeg->nextSeg;
            num_seg_in_buf--;
          } else {
            break;
          }
        }

        if (1) {

          /* ================================================= */
          /* send as many segments as the client advertised for (did not
           * implement cwnd yet) */
          int recv_rwnd = request_seg.rwnd;
          int window_space = maxBufferSize - num_seg_in_buf;
          int seg_to_send = (recv_rwnd < cwnd) ? recv_rwnd : cwnd;
          seg_to_send =
              (seg_to_send < window_space) ? seg_to_send : window_space;
          printf("rwnd: %d, cwnd: %d, window space: %d\n", recv_rwnd, cwnd,
                 window_space);
          printf("choosing to send %d segment(s)\n", seg_to_send);
          printf("sending %d segments\n", seg_to_send);

          while (seg_to_send >= 0) {
            if (window_space == 0 || seg_to_send == 0) {
              break;
            }
            struct timeval get_time;
            Segment seg;
            memset(&seg, 0, sizeof seg);
            seg.sequence_num = ++last_seg_acked;
            seg.ack_num = 0;
            seg.is_eof = (unsigned char)0;
            seg.is_probe = (unsigned char)0;
            seg.is_complete = (unsigned char)0;
            seg.is_resent = (unsigned char)0;
            seg.rwnd = 0;
            gettimeofday(&get_time, NULL);

            if (file_exists == 0) {
              seg.sequence_num = -1;
              seg.ack_num = -1;
            }

            if (recv_rwnd == 0) {
              seg.is_probe = 1;
            }

            seg.timestamp = get_time.tv_sec * 1000000 + get_time.tv_usec;
            if (seg_to_send == 0) {
              seg.is_last_in_iter = 1;
            }

            seg.timestamp = get_time.tv_sec * 1000000 + get_time.tv_usec;

            if (file_exists == 1) {
              int j;
              copyByte = (last_seg_acked - 1) * UDP_PAYLOAD_SIZE;
              for (j = 0; j < UDP_PAYLOAD_SIZE; j++) {
                seg.payload[j] = dataToSend[copyByte++];
                if (copyByte == length) {
                  // sent_complete = 1;
                  seg.is_eof = 1;
                  break;
                }
              }
            }

            int copyIndex = 0;
            unsigned char segBuf[UDP_SEGMENT_SIZE];

            // unsigned char converter[4];
            // int temp;

            converter[0] = (seg.sequence_num >> 24) & 0xFF;
            converter[1] = (seg.sequence_num >> 16) & 0xFF;
            converter[2] = (seg.sequence_num >> 8) & 0xFF;
            converter[3] = seg.sequence_num & 0xFF;

            for (temp = 0; temp < 4; temp++) {
              segBuf[copyIndex++] = converter[temp];
            }
            converter[0] = (seg.ack_num >> 24) & 0xFF;
            converter[1] = (seg.ack_num >> 16) & 0xFF;
            converter[2] = (seg.ack_num >> 8) & 0xFF;
            converter[3] = seg.ack_num & 0xFF;

            for (temp = 0; temp < 4; temp++) {
              segBuf[copyIndex++] = converter[temp];
            }

            segBuf[copyIndex++] = seg.is_eof;
            segBuf[copyIndex++] = seg.is_probe;
            segBuf[copyIndex++] = seg.is_complete;
            segBuf[copyIndex++] = seg.is_resent;

            converter[0] = (seg.rwnd >> 24) & 0xFF;
            converter[1] = (seg.rwnd >> 16) & 0xFF;
            converter[2] = (seg.rwnd >> 8) & 0xFF;
            converter[3] = seg.rwnd & 0xFF;

            for (temp = 0; temp < 4; temp++) {
              segBuf[copyIndex++] = converter[temp];
            }

            converter[0] = (seg.timestamp >> 24) & 0xFF;
            converter[1] = (seg.timestamp >> 16) & 0xFF;
            converter[2] = (seg.timestamp >> 8) & 0xFF;
            converter[3] = seg.timestamp & 0xFF;

            for (temp = 0; temp < 4; temp++) {
              segBuf[copyIndex++] = converter[temp];
            }

            segBuf[copyIndex++] = seg.is_last_in_iter;

            for (j = 0; j < UDP_PAYLOAD_SIZE; j++) {
              segBuf[copyIndex++] = seg.payload[j];
            }

            SlidingWindow *nodePtr = send_base;
            while (nodePtr->nextSeg != NULL) {
              if (nodePtr->nextSeg->seg.sequence_num == seg.sequence_num) {
                // num_seg_in_buf++;
                break;
              }
              nodePtr = nodePtr->nextSeg;
            }
            SlidingWindow *next = malloc(sizeof(SlidingWindow));
            memset(next, 0, sizeof(SlidingWindow));
            next->seg = seg;
            if (nodePtr->nextSeg == NULL) {
              nodePtr->nextSeg = next;
              num_seg_in_buf++;
            }

            last_out_time = seg.timestamp;

            printf("sending segment: %d\n", seg.sequence_num);
            if (num_seg_in_buf == maxBufferSize) {
              printf("sender is sending at max capacity\n");
            }

            // Send(connectionsSocketFd, segBuf, sizeof(segBuf), 0);
            Sendto(connectionsSocketFd, segBuf, sizeof(segBuf), 0,
                   (SA *)&pcliaddr, len);

            /* we send the last segment */
            if (seg.is_eof > 0) {
              break;
            }

            if (seg.is_probe == 1) {
              break;
            }

            seg_to_send--;
            /*
             n = Recvfrom(connectionsSocketFd, mesg, MAXLINE, 0, (SA*)
             &pcliaddr,
             &len);

             printf("child %d, datagram from %s", getpid(),
             Sock_ntop((SA*) &pcliaddr, len));
             printf(", to %s:%d\n", connectionAddress,
             (int) ntohs(serverAddress.sin_port));

             Sendto(connectionsSocketFd, mesg, n, 0, (SA*) &pcliaddr, len);
             */
          }
        }
      }
    }
    printf("file transfer complete!\n");
    free(dataToSend);
    exit(0); /* never executed */
  }
}
/* end mydg_echo */

int getLongest(char left[], char right[]) {

  char bitPtrLeft[33];
  char bitPtrRight[33];

  getBitString(left, bitPtrLeft);
  bitPtrLeft[32] = '\0';
  getBitString(right, bitPtrRight);
  bitPtrRight[32] = '\0';

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

void get_cwnd(int *congestion_state, int *cwnd, int *ssthresh,
              int *prev_ack_seg_num, int *arrive_ack_seg, int *num_dup_count) {
  // printf("get_cwnd: prev_ack: %d, arrive_ack: %d\n", *prev_ack_seg_num,
  // *arrive_ack_seg);
  if (*congestion_state == 1) {
    if (*prev_ack_seg_num == *arrive_ack_seg) {
      *num_dup_count = *num_dup_count + 1;
    } else {
      *cwnd = *cwnd * 2;
      *num_dup_count = 0;
      if (*cwnd >= *ssthresh) {
        *cwnd = *ssthresh;
        *congestion_state = 2;
      }
    }
    if (*num_dup_count == 3) {
      *congestion_state = 3;
      *ssthresh = *cwnd / 2;
      if (*ssthresh == 0) {
        *ssthresh = *ssthresh + 1;
      }
      *cwnd = *ssthresh;
      if (*cwnd == 0) {
        *cwnd = *cwnd + 1;
      }
    }
  } else if (*congestion_state == 2) {
    if (*prev_ack_seg_num == *arrive_ack_seg) {
      *num_dup_count = *num_dup_count + 1;
    } else {
      *cwnd = *cwnd + 1;
      *num_dup_count = 0;
    }
    if (*num_dup_count == 3) {
      *congestion_state = 3;
      *ssthresh = *cwnd / 2;
      if (*ssthresh == 0) {
        *ssthresh = *ssthresh + 1;
      }
      *cwnd = *ssthresh;
      if (*cwnd == 0) {
        *cwnd = *cwnd + 1;
      }
    }
  } else if (*congestion_state == 3) {
    if (*prev_ack_seg_num == *arrive_ack_seg) {
      *cwnd = *cwnd + 1;
    } else {
      *congestion_state = 2;
      *cwnd = *ssthresh;
      *num_dup_count = 0;
    }
  }

  printf("prev_ack_seg_num: %d, arrive_ack_seg: %d, congestion_state: %d\n",
         *prev_ack_seg_num, *arrive_ack_seg, *congestion_state);
}
