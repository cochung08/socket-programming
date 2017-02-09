#include "tour.h"
#include "hw_addrs.h"
#include "unp.h"
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>


#include "ping.h"

static int previous_index_static = -1;
static int previous_vmNum_static = -1;

static int if_last_node = -1;
static int ping_counter = 0;

static int if_ping[PING_SIZE];
static int ping_counter_array[PING_SIZE];
IpAddrMacAddr itemArray[4];
static int ping_switch_ = -1;
int datalen = 16; /* data that goes with ICMP echo request */

static void recvfrom_alarm(int signo) {

  printf("\n\ngracefully exit its Tour process\n\n");
  exit(1);
}

static void recv_alarm(int signo) {

  ping_counter_array[previous_vmNum_static] = 5;
  printf("\n\ntime out! stop pinging\n\n");
}

////////////////////////////sun lin code

typedef struct interface {
  char macAddr[ETH_ALEN];
  char ipAddr[IPADDR_SIZE];
  int interfaceNum;
} Interface;

typedef struct arp_entry {
  char ipAddr[IPADDR_SIZE];
  char macAddr[ETH_ALEN];
  unsigned short sll_hatype;
  int sll_ifindex;
  int conn_fd;
} ArpEntry;

typedef struct arp_message {
  unsigned char id[2];
  unsigned char hatype[2];
  unsigned char prottype[2];
  unsigned char hardsize;
  unsigned char protsize;
  unsigned char op[2];
  char sender_mac[ETH_ALEN];
  unsigned char sender_ip[IP_ALEN];
  char target_mac[ETH_ALEN];
  unsigned char target_ip[IP_ALEN];
} ArpMessage;

static Interface interfaceArray[INTERFACE_SIZE];
static ArpEntry arpTable[ARP_TABLE_SIZE];
static int numEntries;

static struct sockaddr_ll socket_address;

void printPacketBytes(char *buffer, int size) {
  int i;
  for (i = 0; i < size; i++) {
    printf("%.2x.", buffer[i] & 0xFF);
  }
}

void printArpTable() {
  printf("\n****************************\n");
  printf("printing arp table:\n\n");
  int i;
  for (i = 0; i < numEntries; i++) {
    ArpEntry arpEntry = arpTable[i];
    printf("ip addr: %-15s, mac address: ", arpEntry.ipAddr);
    int j;
    for (j = 0; j < ETH_ALEN; j++) {
      printf("%.2x%s", arpEntry.macAddr[j] & 0xFF,
             (j == ETH_ALEN - 1) ? "\n" : ":");
    }
    printf("sll_hatype: %hu, sll_ifindex: %d, conn_fd: %d\n",
           arpEntry.sll_hatype, arpEntry.sll_ifindex, arpEntry.conn_fd);
  }
  printf("\ndone printing arp table\n");
  printf("****************************\n\n\n");
}

void getAllMacAddress() {

  memset(interfaceArray, 0, sizeof interfaceArray);
  char local_CanonicalIP[IPADDR_SIZE];

  int k;
  for (k = 0; k < INTERFACE_SIZE; k++) {
    interfaceArray[k].interfaceNum = -1;
  }

  struct hwa_info *hwa, *hwahead;
  struct sockaddr *sa;
  char *ptr;
  int i, prflag;

  printf("\n");

  for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {

    prflag = 0;
    i = 0;
    do {
      if (hwa->if_haddr[i] != '\0') {
        prflag = 1;
        break;
      }
    } while (++i < IF_HADDR);

    if (strcmp(hwa->if_name, "eth0") == 0) {
      struct sockaddr_in *tmp = (struct sockaddr_in *)hwa->ip_addr;
      memset(local_CanonicalIP, 0, sizeof(local_CanonicalIP));
      inet_ntop(AF_INET, &(tmp->sin_addr), local_CanonicalIP, 20);
    }

    if (strcmp(hwa->if_name, "eth0") == 0) {

      int p = 0;
      for (p = 0; p < INTERFACE_SIZE; p++) {
        if (interfaceArray[p].interfaceNum == -1) {
          interfaceArray[p].interfaceNum = hwa->if_index;
          // printf("debug:: %d mac: %s\n", __LINE__, hwa->if_haddr);
          memcpy(interfaceArray[p].ipAddr, local_CanonicalIP, IPADDR_SIZE);
          memcpy(interfaceArray[p].macAddr, hwa->if_haddr, ETH_ALEN);
          break;
        }
      }
    }
  }
  free_hwa_info(hwahead);
}

void printAllInterfaces() {
  printf("IP/HW pairs:\n\n");
  int i;
  char *ptr;
  for (i = 0; i < INTERFACE_SIZE; i++) {
    if (interfaceArray[i].interfaceNum != -1) {
      // printf("interface num: %d\n",
      //		interfaceArray[i].interfaceNum);
      ptr = interfaceArray[i].macAddr;
      printf("IP addr: ");
      printf("%s\n", interfaceArray[i].ipAddr);
      printf("HW addr: ");
      int j = IF_HADDR;
      do {
        printf("%.2x%s", *ptr++ & 0xff, (j == 1) ? " " : ":");
      } while (--j > 0);
      printf("\n");
      printf("interface num: %d\n\n", interfaceArray[i].interfaceNum);
    }
  }
}

int indexOfIP(char *ipAddr) {
  int i;
  for (i = 0; i < numEntries; i++) {
    if (strcmp(ipAddr, arpTable[i].ipAddr) == 0) {
      return i;
    }
  }
  return -1;
}

void initArpMessage(ArpMessage *arpMesssage) {
  arpMesssage->id[0] = 0x54;
  arpMesssage->id[1] = 0x03;
  arpMesssage->hatype[0] = 0x00;
  arpMesssage->hatype[1] = 0x01;
  arpMesssage->prottype[0] = 0x08;
  arpMesssage->prottype[1] = 0x00;
  arpMesssage->hardsize = 0x06;
  arpMesssage->protsize = 0x04;
}

void setIPAddr(unsigned char *ipLoc, char *ipString) {
  int ip1;
  int ip2;
  int ip3;
  int ip4;
  sscanf(ipString, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);
  ipLoc[0] = (char)ip1;
  ipLoc[1] = (char)ip2;
  ipLoc[2] = (char)ip3;
  ipLoc[3] = (char)ip4;
}

void sendArpMessage(int pfSockfd, char *src_mac, char *dest_mac, int if_index,
                    ArpMessage *arpMessage) {

  printf("\n***********************\n");
  printf("\nStartSendArpMessage\n");

  // printPacketBytes(arpMessage, sizeof(ArpMessage));

  //	struct sockaddr_ll socket_address;

  /*buffer for ethernet frame*/
  char buffer[ARP_LEN];
  memset(buffer, 0, ARP_LEN);
  /*pointer to ethenet header*/
  unsigned char *etherhead = buffer;
  /*userdata in ethernet frame*/
  unsigned char *data = buffer + 14;
  /*another pointer to ethernet header*/
  struct ethhdr *eh = (struct ethhdr *)etherhead;
  socket_address.sll_family = PF_PACKET;
  socket_address.sll_protocol = htons(SELF_PROTOCOL);
  socket_address.sll_ifindex = if_index;
  socket_address.sll_hatype = ARPHRD_ETHER;
  socket_address.sll_pkttype = PACKET_OTHERHOST;
  socket_address.sll_halen = ETH_ALEN;
  /*MAC - begin*/
  socket_address.sll_addr[0] = dest_mac[0];
  socket_address.sll_addr[1] = dest_mac[1];
  socket_address.sll_addr[2] = dest_mac[2];
  socket_address.sll_addr[3] = dest_mac[3];
  socket_address.sll_addr[4] = dest_mac[4];
  socket_address.sll_addr[5] = dest_mac[5];
  /*MAC - end*/
  socket_address.sll_addr[6] = 0x00; /*not used*/
  socket_address.sll_addr[7] = 0x00; /*not used*/
  memcpy((void *)buffer, (void *)dest_mac, ETH_ALEN);
  memcpy((void *)(buffer + ETH_ALEN), (void *)src_mac, ETH_ALEN);
  eh->h_proto = htons(SELF_PROTOCOL);
  memcpy((void *)data, (void *)arpMessage, sizeof(ArpMessage));
  printf("debug:: %d ethernet header: ", __LINE__);
  int l;
  char *buf = buffer;
  for (l = 0; l < 12; l++) {
    printf("%.2x%s", buf[l] & 0xff, (l == 5 || l == 11) ? " " : ":");
  }
  // printf("\nline:%d, sizeof buffer: %ld\n", __LINE__, sizeof(buffer));
  // printf("line: %d, %d:%d", __LINE__, (int) arpMessage->op[0], (int)
  // arpMessage->op[1]);
  // printf("arp message content: %s", data);
  // printPacketBytes(buffer, sizeof(buffer));
  int result =
      sendto(pfSockfd, buffer, ARP_LEN, 0, (struct sockaddr *)&socket_address,
             sizeof(socket_address));
  if (result == -1) {
    printf("cannot sendwhat\n");
    err_sys("cannot sendhow");
  }

  // free(buffer);

  printf("\nEndSendMessage\n");
  printf("***********************\n\n\n\n");
}

void recvArpMessage(int PFsockFd, ArpMessage *arpMessage) {
  char buffer[sizeof(ArpMessage) + 14]; /*Buffer for ethernet frame*/
  memset(buffer, 0, sizeof(ArpMessage) + 14);
  int length = 0; /*length of the received frame*/

  // struct sockaddr_ll recvfromAddr;

  int len = sizeof(socket_address);

  length = recvfrom(PFsockFd, buffer, sizeof(ArpMessage) + 14, 0,
                    (struct sockaddr *)&socket_address, &len);
  // printPacketBytes(buffer, sizeof(buffer));
  // printf("\n\nline: %d, sizeof buf: %ld, sizeof message: %ld\n", __LINE__,
  // sizeof(buffer), sizeof(ArpMessage));
  memcpy(arpMessage, buffer + 14, sizeof(ArpMessage));
  // printf("\nline: %d, %d:%d\n", __LINE__, (int) arpMessage->op[0], (int)
  // arpMessage->op[1]);
}

void ip_ntop(unsigned char *ip, char *ipPresentation) {
  sprintf(ipPresentation, "%d.%d.%d.%d", (int)ip[0], (int)ip[1], (int)ip[2],
          (int)ip[3]);
}

int checkEntryExist(char *ipAddr, char *macAddr) {
  char ipPresentation[IPADDR_SIZE];
  memset(ipPresentation, 0, IPADDR_SIZE);
  ip_ntop(ipAddr, ipPresentation);
  int i;
  for (i = 0; i < numEntries; i++) {
    ArpEntry arpEntry = arpTable[i];
    if (strcmp(arpEntry.ipAddr, ipPresentation) == 0 &&
        strcmp(arpEntry.macAddr, macAddr) == 0) {
      return i;
    }
  }
  return -1;
}

int checkIsDestNode(unsigned char *ipAddr) {
  char ipPresentation[IPADDR_SIZE];
  memset(ipPresentation, 0, IPADDR_SIZE);
  ip_ntop(ipAddr, ipPresentation);

  int i;
  for (i = 0; i < INTERFACE_SIZE; i++) {
    Interface interface = interfaceArray[i];
    if (interface.interfaceNum == -1) {
      break;
    } else {
      if (strcmp(interface.ipAddr, ipPresentation) == 0) {
        return 1;
      }
    }
  }
  return 0;
}

int checkAddressNotEmpty(char *mac) {
  int i;
  for (i = 0; i < 6; i++) {
    if (*mac++ & 0xff != 0) {
      return 1;
    }
  }
  return 0;
}

//////////////////////////sun lin code

void printMacAddr(char *mac) {
  char *ptr = mac;
  int i = IF_HADDR;
  do {
    printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
  } while (--i > 0);
}

uint32_t getUint32_tFromName(char *vmStrFull) {

  struct hostent *hptr = gethostbyname(vmStrFull);
  char **pptr = hptr->h_addr_list;
  char src_ip[INET_ADDRSTRLEN];
  uint32_t sip;

  for (; *pptr != NULL; pptr++) {
    Inet_ntop(hptr->h_addrtype, *pptr, src_ip, sizeof(src_ip));
    sip = ((struct in_addr *)hptr->h_addr_list[0])->s_addr;
  }
  return sip;
}

char LOCAL_MAC_ADDR[ETH_ALEN];
uint32_t LOCAL_ADDRESS_uint32_t;

void Get_LOCAL_ADDR() {

  struct hwa_info *hwa, *hwahead;
  struct sockaddr *sa;
  char *ptr;
  int i, prflag;

  printf("\n");

  for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {

    prflag = 0;
    i = 0;
    do {
      if (hwa->if_haddr[i] != '\0') {
        prflag = 1;
        break;
      }
    } while (++i < IF_HADDR);

    if (strcmp(hwa->if_name, "eth0") == 0) {

      memcpy(LOCAL_MAC_ADDR, hwa->if_haddr, ETH_ALEN);
    }
  }
  free_hwa_info(hwahead);

  char localHost[VMNAME_SIZE];
  gethostname(localHost, sizeof(localHost));
  struct hostent *hp = (struct hostent *)gethostbyname((const char *)localHost);
  LOCAL_ADDRESS_uint32_t = getUint32_tFromName(localHost);
}

int areq(int k, char *dst_mac) {
  ///////////////////sunlin
  printf("\nareq is called\n");

  int sockfd;
  struct sockaddr_un cliaddr;

  sockfd = Socket(AF_LOCAL, SOCK_STREAM, 0);

  bzero(&cliaddr, sizeof(cliaddr));
  cliaddr.sun_family = AF_LOCAL;

  char *tmpArray = malloc(sizeof(char) * 1024);
  memset(tmpArray, 0, sizeof(tmpArray));

  strcpy(tmpArray, "/tmp/unix5403.dg");
  // mkstemp(tmpArray);
  // unlink(tmpArray);

  strcpy(cliaddr.sun_path, tmpArray);

  Connect(sockfd, (SA *)&cliaddr, sizeof(cliaddr));

  char previousvmStr[15];
  sprintf(previousvmStr, "%d", k);
  char prev_vmStrFull[80];
  strcpy(prev_vmStrFull, "vm");
  strcat(prev_vmStrFull, previousvmStr);

  char dst_ip[20];
  hostname_to_ip(prev_vmStrFull, dst_ip);

  printf("the IP address  :: %s\n", dst_ip);

  //	printf("yoyoy");
  Writen(sockfd, dst_ip, 20);

  //	sigset_t sigset_alrm;
  //	Sigemptyset(&sigset_alrm);
  //	Sigaddset(&sigset_alrm, SIGALRM);
  //	Signal(SIGALRM, recv_alarm);
  //	alarm(2);

  char buf[6];

  int tmp = Readable_timeo(sockfd, 5);
  if (tmp == 0) {
    printf("time out\n\n");

  } else {
    int n = read(sockfd, buf, 6);
  }

  // unlink(tmpArray);
  memcpy(dst_mac, buf, 6);
  printf("HW address from areq::  ");
  printMacAddr(buf);
  printf("\n");

  free(tmpArray);
  return 1;
  ///////////////////sunlin
}

int send_v4(int pfsockfd, int pgsockfd, int dst_vmNum) {

  char dst_mac[6];

  areq(dst_vmNum, dst_mac);

  char previousvmStr[15];
  sprintf(previousvmStr, "%d", dst_vmNum);
  char prev_vmStrFull[80];
  strcpy(prev_vmStrFull, "vm");
  strcat(prev_vmStrFull, previousvmStr);

  char localHost[VMNAME_SIZE];
  gethostname(localHost, sizeof(localHost));

  char src_ip[20];
  char dst_ip[20];
  hostname_to_ip(localHost, src_ip);
  hostname_to_ip(prev_vmStrFull, dst_ip);

  uint32_t sip;

  int send_result = 0;
  struct sockaddr_ll device;
  void *buffer = (void *)malloc(ETHR_FRAME_LEN);
  memset(buffer, 0, ETHR_FRAME_LEN);

  unsigned char *ethHeader = buffer;
  unsigned char *ipHeader = buffer + 14;

  struct ethhdr *eh = (struct ethhdr *)ethHeader;
  device.sll_family = PF_PACKET;
  device.sll_protocol = htons(ETH_P_IP);
  device.sll_ifindex = 2;

  device.sll_hatype = ARPHRD_ETHER;

  device.sll_pkttype = PACKET_OTHERHOST;

  device.sll_halen = 6;

  memcpy(device.sll_addr, dst_mac, 6);

  memcpy((void *)buffer, (void *)dst_mac, ETH_ALEN);
  memcpy((void *)(buffer + ETH_ALEN), (void *)LOCAL_MAC_ADDR, ETH_ALEN);
  eh->h_proto = htons(ETH_P_IP);

  struct icmp *icmp = buffer + 14 + sizeof(struct ip);

  icmp->icmp_type = ICMP_ECHO;
  icmp->icmp_code = 0;
  icmp->icmp_id = htons(IP_IDENTIFICATION);
  icmp->icmp_seq = ping_counter_array[dst_vmNum];
  memset(icmp->icmp_data, 0xa5, datalen); /* fill with pattern */
  Gettimeofday((struct timeval *)icmp->icmp_data, NULL);

  //	icmp->icmp_cksum = 0;
  icmp->icmp_cksum = in_cksum((u_short *)icmp, sizeof(struct icmp) + datalen);

  struct ip *ip = (struct ip *)ipHeader;

  int status;
  if ((status = inet_pton(AF_INET, src_ip, &(ip->ip_src))) != 1) {
    fprintf(stderr, "inet_pton() failed.\nError message: %s", strerror(status));
    exit(EXIT_FAILURE);
  }

  if ((status = inet_pton(AF_INET, dst_ip, &(ip->ip_dst))) != 1) {
    fprintf(stderr, "inet_pton() failed.\nError message: %s", strerror(status));
    exit(EXIT_FAILURE);
  }

  ip->ip_v = IPVERSION;
  ip->ip_hl = sizeof(struct ip) >> 2;
  ip->ip_tos = 0;

  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct icmp) + datalen);
  ip->ip_id = htons(IP_IDENTIFICATION);
  ip->ip_off = 0;
  ip->ip_ttl = 255;
  ip->ip_p = IPPROTO_ICMP;
  ip->ip_sum = in_cksum((u_short *)ip,
                        sizeof(struct ip) + sizeof(struct icmp) + datalen);

  send_result = sendto(pfsockfd, buffer, ETHR_FRAME_LEN, 0,
                       (struct sockaddr *)&device, sizeof(device));
}

void proc_v4(char *ptr, ssize_t len, struct msghdr *msg,
             struct timeval *tvrecv) {
  int hlen1, icmplen;
  double rtt;
  struct ip *ip;
  struct icmp *icmp;
  struct timeval *tvsend;

  ip = (struct ip *)ptr;  /* start of IP header */
  hlen1 = ip->ip_hl << 2; /* length of IP header */
  if (ip->ip_p != IPPROTO_ICMP)
    return; /* not ICMP */

  icmp = (struct icmp *)(ptr + hlen1); /* start of ICMP header */
  if ((icmplen = len - hlen1) < 8)
    return; /* malformed packet */

  if (icmp->icmp_type == ICMP_ECHOREPLY) {
    if (ntohs(icmp->icmp_id) != IP_IDENTIFICATION)
      return; /* not a response to our ECHO_REQUEST */
              //		if (icmplen < 16)
              //			return;			/* not enough data to use */

    tvsend = (struct timeval *)icmp->icmp_data;
    tv_sub(tvrecv, tvsend);
    rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;

    struct in_addr iaddr;
    iaddr.s_addr = ip->ip_src.s_addr;
    struct hostent *he = gethostbyaddr(&iaddr, sizeof(iaddr), AF_INET);

    char hName[20];
    strcpy(hName, he->h_name);
    hName[0] = '0';
    hName[1] = '0';
    int ping_node = atoi(hName);

    //		printf("\nping_node :%d\n", ping_node);
    ping_counter_array[ping_node]++;

    //		ping_switch_ = 1;
    printf("ping: %d bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n", icmplen,
           he->h_name, icmp->icmp_seq, ip->ip_ttl, rtt);
  }
}

void read_ping(int pgsockfd) {

  char recvbuf[ETHR_FRAME_LEN];

  int n = Recvfrom(pgsockfd, (void *)recvbuf, ETHR_FRAME_LEN, 0, NULL, NULL);
  struct timeval tvrecv;

  Gettimeofday(&tvrecv, NULL);

  proc_v4(recvbuf, n, NULL, &tvrecv);
}

void send_multicast(int sendfd, SA *sadest, socklen_t salen, char *msg) {

  Sendto(sendfd, msg, strlen(msg), 0, sadest, salen);

  char localHost[VMNAME_SIZE];
  gethostname(localHost, sizeof(localHost));

  char msgSequence2[MAXLINE];
  sprintf(msgSequence2, "%s %s %s %s", "Node", localHost, ". Sending: ", msg);

  printf("%s \n", msgSequence2);
}

void recv_multicast(int recvfd, int sendfd, SA *sadest, socklen_t salen) {

  int n;
  char line[MAXLINE + 1];
  socklen_t len;
  struct sockaddr *safrom;

  safrom = Malloc(salen);

  //	for (;;) {
  len = salen;
  n = Recvfrom(recvfd, line, MAXLINE, 0, safrom, &len);
  line[n] = 0; /* null terminate */

  char localHost[VMNAME_SIZE];
  gethostname(localHost, sizeof(localHost));

  char msgSequence2[MAXLINE];
  sprintf(msgSequence2, "%s %s %s %s", "Node", localHost, ". receiving: ",
          line);

  printf("%s \n", msgSequence2);

  if (line[6] == 'T') {
    char localHost[VMNAME_SIZE];
    gethostname(localHost, sizeof(localHost));

    char msgSequence[MAXLINE];
    sprintf(msgSequence, "%s %s %s", "<<<<< Node ", localHost,
            " . I am a member of the group. >>>>>\n");

    send_multicast(sendfd, sadest, salen, msgSequence); /* parent -> sends */
  } else if (line[6] == 'N') {
    sigset_t sigset_alrm;

    Sigemptyset(&sigset_alrm);
    Sigaddset(&sigset_alrm, SIGALRM);
    Signal(SIGALRM, recvfrom_alarm);
    alarm(5);
  }
  //}
}

int *hostNameToNumberArray(int argc, char *argv[]) {

  int vmNumber[TOTAL_VM_SIZE];
  int k;
  for (k = 0; k < TOTAL_VM_SIZE; k++) {
    vmNumber[k] = 0;
  }

  int i;

  char hostname[VMNAME_SIZE], ip_dest[INET_ADDRSTRLEN];
  gethostname(hostname, sizeof(hostname));
  //	printf("Local Machine Hostname is :  %s\n", hostname);

  hostname[0] = '0';
  hostname[1] = '0';
  int curr_node = atoi(hostname);
  vmNumber[0] = curr_node;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], argv[i - 1]) == 0) {
      printf("Invalid tour ");
      exit(0);
    } else {
      char *host = argv[i];
      host[0] = '0';
      host[1] = '0';
      int curr_node = atoi(host);
      vmNumber[i] = curr_node;
    }
  }

  return vmNumber;
}

void sendRTpacket(int rtsockfd, PayLoad *packet, int sizeOfPayload) {

  struct in_addr addr;
  struct sockaddr_in local;
  struct ip *ip;

  int index = packet->next_ip_idx;
  int vmInt = packet->hostArray[index];
  char vmStr[15];
  sprintf(vmStr, "%d", vmInt);

  char vmStrFull[80];
  strcpy(vmStrFull, "vm");
  strcat(vmStrFull, vmStr);

  struct hostent *hptr = gethostbyname(vmStrFull);
  char **pptr = hptr->h_addr_list;
  char src_ip[INET_ADDRSTRLEN];
  uint32_t sip;

  for (; *pptr != NULL; pptr++) {
    Inet_ntop(hptr->h_addrtype, *pptr, src_ip, sizeof(src_ip));
    sip = ((struct in_addr *)hptr->h_addr_list[0])->s_addr;
  }

  int vmInt2 = packet->hostArray[index + 1];
  char vmStr2[15];
  sprintf(vmStr2, "%d", vmInt2);

  char vmStrFull2[80];
  strcpy(vmStrFull2, "vm");
  strcat(vmStrFull2, vmStr2);

  struct hostent *hptr2 = gethostbyname(vmStrFull2);
  char **pptr2 = hptr2->h_addr_list;
  char dst_ip[INET_ADDRSTRLEN];
  uint32_t dip;

  for (; *pptr2 != NULL; pptr2++) {
    Inet_ntop(hptr2->h_addrtype, *pptr2, dst_ip, sizeof(dst_ip));
    dip = ((struct in_addr *)hptr2->h_addr_list[0])->s_addr;
  }

  //	printf("\n4444444444\n");
  int sizeOfRawPacket = sizeOfPayload + sizeof(struct ip);
  char buf[IP_WHOLE_PACK_SIZE];
  memset(buf, 0, sizeOfRawPacket);

  ip = (struct ip *)buf;

  ip->ip_src.s_addr = sip;
  ip->ip_dst.s_addr = dip;
  ip->ip_v = IPVERSION;
  ip->ip_hl = sizeof(struct ip) >> 2;
  ip->ip_tos = 0;

#if defined(linux) || defined(__OpenBSD__)
  ip->ip_len = htons(sizeOfRawPacket);
#else
  ip->ip_len = userlen; /* host byte order */
#endif
  ip->ip_id = htons(IP_IDENTIFICATION);
  ip->ip_off = 0;
  ip->ip_ttl = 1;
  ip->ip_p = RT_PROTOCAL;

  char *payload = buf + sizeof(struct ip);
  packet->next_ip_idx++;
  memcpy((void *)payload, (void *)packet, sizeof(PayLoad));

  struct sockaddr_in dest;
  bzero(&dest, sizeof(dest));
  dest.sin_addr.s_addr = dip;
  dest.sin_family = AF_INET;

  Sendto(rtsockfd, buf, sizeOfPayload, 0, (SA *)&dest, sizeof(dest));
}

PayLoad receiveRTpacket(int rtsockfd) {

  struct sockaddr_in addr;
  int len = sizeof(addr);
  char buff[IP_WHOLE_PACK_SIZE];
  Recvfrom(rtsockfd, (void *)buff, IP_WHOLE_PACK_SIZE, 0, (SA *)&addr, &len);

  struct ip *ipHead = (struct ip *)buff;
  PayLoad *payload = (PayLoad *)(buff + sizeof(struct ip));

  if (ntohs(ipHead->ip_id) != IP_IDENTIFICATION) {
    printf("Invalid Packet identifier.\n");
  }

  time_t ticks = time(NULL);

  struct in_addr iaddr;
  iaddr.s_addr = ipHead->ip_src.s_addr;
  struct hostent *he = gethostbyaddr(&iaddr, sizeof(iaddr), AF_INET);
  printf("%.24s: received source routing packet from %s\n",
         (char *)ctime(&ticks), he->h_name);

  char vm_name[VMNAME_SIZE];
  gethostname(vm_name, sizeof(vm_name));

  PayLoad payloadCopy;

  int q;
  for (q = 0; q < TOTAL_VM_SIZE; q++) {

    payloadCopy.hostArray[q] = payload->hostArray[q];
  }
  payloadCopy.next_ip_idx = payload->next_ip_idx;
  strcpy(payloadCopy.multi_port, payload->multi_port);
  strcpy(payloadCopy.multi_ip, payload->multi_ip);

  return payloadCopy;
}

int hostname_to_ip(char *hostname, char *ip) {
  struct hostent *he;
  struct in_addr **addr_list;
  int i;

  if ((he = gethostbyname(hostname)) == NULL) {

    herror("gethostbyname");
    return 1;
  }

  addr_list = (struct in_addr **)he->h_addr_list;

  for (i = 0; addr_list[i] != NULL; i++) {

    strcpy(ip, inet_ntoa(*addr_list[i]));
    return 0;
  }

  return 1;
}

int main(int argc, char *argv[]) {

  int k;
  for (k = 1; k < PING_SIZE; k++) {
    ping_counter_array[k] = 0;
    if_ping[k] = 0;
  }

  char dst_mac1[6] = {0x00, 0x0c, 0x29, 0x49, 0x3f, 0x5b};

  char dst_mac2[6] = {0x00, 0x0c, 0x29, 0xd9, 0x08, 0xec};
  char dst_mac3[6] = {0x00, 0x0c, 0x29, 0xa3, 0x1f, 0x19};
  char ipAddr1[INET_ADDRSTRLEN] = "130.245.156.21";
  char ipAddr2[INET_ADDRSTRLEN] = "130.245.156.22";
  char ipAddr3[INET_ADDRSTRLEN] = "130.245.156.23";

  IpAddrMacAddr item1;
  strcpy(item1.ipAddr, ipAddr1);
  memcpy(item1.macAddr, dst_mac1, ETH_ALEN);

  IpAddrMacAddr item2;
  strcpy(item2.ipAddr, ipAddr2);
  memcpy(item2.macAddr, dst_mac2, ETH_ALEN);

  IpAddrMacAddr item3;
  strcpy(item3.ipAddr, ipAddr3);
  memcpy(item3.macAddr, dst_mac3, ETH_ALEN);

  itemArray[1] = item1;
  itemArray[2] = item2;
  itemArray[3] = item3;

  Get_LOCAL_ADDR();
  pid = getpid() & 0xffff; /* ICMP ID field is 16 bits */

  const int on1 = 1;
  int ifVisit = 0;

  struct sockaddr_in mcast_addr;

  int pfsockfd, rtsockfd, pgsockfd;

  if ((pfsockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) == -1) {
    printf("Error   pfsockfd\n");
    exit(1);
  }

  if ((pgsockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
    printf("Error  pgsockfd\n");
    exit(1);
  }

  if ((rtsockfd = socket(AF_INET, SOCK_RAW, RT_PROTOCAL)) == -1) {
    printf("Error  rtsockfd\n");
    exit(1);
  }

  Setsockopt(rtsockfd, IPPROTO_IP, IP_HDRINCL, &on1, sizeof(on1));

  int multiSendSockfd, multiRecvSockfd;
  const int on = 1;

  if ((multiRecvSockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    printf("Error multiRecvSockfd\n");
    perror("multiRecvSockfd");
    return -1;
  }
  Setsockopt(multiRecvSockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  socklen_t salen;
  struct sockaddr *sasend, *sarecv;

  if (argc > 1) {

    printf("\n\nstart  tour!! waiting for response\n\n");

    ifVisit = 1;

    // multicast
    char *multicastPort = "9821";
    char *multicastAddr = "234.245.209.121";

    multiSendSockfd =
        Udp_client(multicastAddr, multicastPort, (void **)&sasend, &salen);

    unsigned char multicastTTL = 1;
    setsockopt(multiSendSockfd, IPPROTO_IP, IP_MULTICAST_TTL,
               (void *)&multicastTTL, sizeof(multicastTTL));

    sarecv = Malloc(salen);
    memcpy(sarecv, sasend, salen);
    Bind(multiRecvSockfd, sarecv, salen);

    Mcast_join(multiRecvSockfd, sasend, salen, NULL, 0);
    // multicast
    int *hostNumberArray = hostNameToNumberArray(argc, argv);

    PayLoad packet;
    packet.next_ip_idx = 0;
    strcpy(packet.multi_port, multicastPort);
    strcpy(packet.multi_ip, multicastAddr);

    int i;
    for (i = 0; i < TOTAL_VM_SIZE; i++) {
      packet.hostArray[i] = hostNumberArray[i];
    }

    sendRTpacket(rtsockfd, &packet, sizeof(PayLoad));
  }

  int nready;
  fd_set rset, allset;
  struct timeval tv;

  FD_ZERO(&allset);
  FD_ZERO(&rset);

  FD_SET(rtsockfd, &allset);
  FD_SET(pgsockfd, &allset);
  FD_SET(multiRecvSockfd, &allset);

  int maxfd = max(max(rtsockfd, pgsockfd), multiRecvSockfd);

  for (;;) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    rset = allset;

    nready = select(maxfd + 1, &rset, NULL, NULL, &tv);

    if (nready < 0) {
      if (errno == EINTR) {
        printf("EINTR Error!! !\n");
        continue;
      } else {
        perror("EINTR other Error");
        exit(0);
      }
    }

    else if (nready == 0) {

      //		printf("!!!!\n\n");
      if (ping_switch_ == 1) {
        // ping_counter_array[previous_index_static]++;

        if (ping_counter_array[previous_vmNum_static] >= 5) {
          if (if_last_node == 1) {
            char localHost[VMNAME_SIZE];
            gethostname(localHost, sizeof(localHost));

            char msgSequence[MAXLINE];
            sprintf(msgSequence, "%s %s %s", "<<<<< This is node ", localHost,
                    " . Tour has ended . Group members please identify "
                    "yourselves. >>>>>\n");

            send_multicast(multiSendSockfd, sasend, salen,
                           msgSequence); /* parent -> sends */

            ping_switch_ = -1;
            continue;
          }
        }
        int k = 1;
        for (; k < PING_SIZE; k++) {
          if (if_ping[k] != 0) {

            send_v4(pfsockfd, pgsockfd, k);
          }
        }
      }
    }
    if (FD_ISSET(rtsockfd, &rset)) {

      ping_counter = 0;

      PayLoad payload = receiveRTpacket(rtsockfd);

      int previous_index = payload.next_ip_idx - 1;
      int previous_vmNum = payload.hostArray[previous_index];

      if (if_ping[previous_vmNum] == 0) {
        ping_switch_ = 1;
        if_ping[previous_vmNum] = 1;
        previous_index_static = previous_index;
        previous_vmNum_static = previous_vmNum;
        //				send_v4(pfsockfd, pgsockfd,
        //previous_vmNum);
        //				ping_counter_array[previous_vmNum]++;

        char dst_ip[20];
        char previousvmStr[15];
        sprintf(previousvmStr, "%d", previous_vmNum);
        char prev_vmStrFull[80];
        strcpy(prev_vmStrFull, "vm");
        strcat(prev_vmStrFull, previousvmStr);

        hostname_to_ip(prev_vmStrFull, dst_ip);

        printf("\n\nPING1 %s (%s): %d data bytes\n\n", prev_vmStrFull, dst_ip,
               datalen);
        //

      } else {
      }

      if (ifVisit == 0) {
        ifVisit = 1;

        // multicast
        char *multicastPort = payload.multi_port;
        char *multicastAddr = payload.multi_ip;

        multiSendSockfd =
            Udp_client(multicastAddr, multicastPort, (void **)&sasend, &salen);

        sarecv = Malloc(salen);
        memcpy(sarecv, sasend, salen);
        Bind(multiRecvSockfd, sarecv, salen);

        Mcast_join(multiRecvSockfd, sasend, salen, NULL, 0);
        // multicast

      } else {
      }

      int q;
      for (q = 0; q < TOTAL_VM_SIZE; q++) {

        if (payload.hostArray[q] == 0)
          break;
      }
      int lastIndex = q - 1;
      if (payload.next_ip_idx == lastIndex) {

        if_last_node = 1;

      } else {

        sendRTpacket(rtsockfd, &payload, sizeof(PayLoad));
      }
    }
    if (FD_ISSET(pgsockfd, &rset)) {
      read_ping(pgsockfd);
    }
    if (FD_ISSET(multiRecvSockfd, &rset)) {

      ping_switch_ = -1;

      recv_multicast(multiRecvSockfd, multiSendSockfd, sasend, salen);
    }
  }
  return 0;
}
