#include "hw_addrs.h"
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/socket.h>


#include "odr.h"
#include "unp.h"


#include "function.h"

#define NODES 10

typedef struct RoutingEntry {
  char next_hop[ETH_ALEN];
  int out_interface;
  int hops_to_dest;
  time_t timestamp;
  int highest_broadcast_id[NODES + 1];
} RoutingEntry;

static Interface interfaceArray[INTERFACE_SIZE];
static Map_port_path MapArray[MAP_SIZE];
static struct sockaddr_ll socket_address;
// static RoutingEntry table[NODES + 1];
// static SELF_DEFINED_PF_Packet packet_buffer[100];
// static int num_packets_buffered = 0;
static int interface_num = -1;
static char recvAddr[ETH_ALEN];
static RoutingEntry *table;
static int stale_time;

char local_CanonicalIP[IPAddr_SIZE];

SELF_DEFINED_PF_Packet *createPF_Packet(int type, char *source_ipAddr,
                                        int source_port,
                                        char *destination_ipAddr,
                                        int destination_port, int hop_count,
                                        char *msg) {
  SELF_DEFINED_PF_Packet *packet =
      (SELF_DEFINED_PF_Packet *)malloc(sizeof(SELF_DEFINED_PF_Packet));

  packet->type = type;
  packet->source_port = source_port;
  packet->destination_port = destination_port;
  packet->hop_count = hop_count;

  strcpy(packet->source_ipAddr, source_ipAddr);
  strcpy(packet->destination_ipAddr, destination_ipAddr);
  strcpy(packet->application_msg, msg);

  return packet;
}

void sendMessage(int pfSockfd, char *src_mac, char *dest_mac, int if_index,
                 SELF_DEFINED_PF_Packet *packet) {

  printf("\n***********************\n");
  printf("\nStartSendMessage\n");

  char vmName[40];
  gethostname(vmName, sizeof(vmName));

  printf("\nODR at node %s : sending - frame hdr - src: %s  dest: ", vmName,
         vmName);

  int i = 6;
  char *ptr = dest_mac;
  do {
    printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
  } while (--i > 0);

  struct in_addr source_ipv4addr;
  inet_pton(AF_INET, packet->source_ipAddr, &source_ipv4addr);
  struct hostent *heSource =
      gethostbyaddr(&source_ipv4addr, sizeof source_ipv4addr, AF_INET);
  char srcHostName[20];
  strcpy(srcHostName, heSource->h_name);

  struct in_addr dest_ipv4addr;
  inet_pton(AF_INET, packet->destination_ipAddr, &dest_ipv4addr);
  struct hostent *heDest =
      gethostbyaddr(&dest_ipv4addr, sizeof dest_ipv4addr, AF_INET);
  char destHostName[20];
  strcpy(destHostName, heDest->h_name);

  printf("\nODR msg payload - message: msg type: %d src: %s dest: %s\n",
         packet->type, srcHostName, destHostName);

  packet->type = htonl(packet->type);
  packet->source_port = htonl(packet->source_port);
  packet->destination_port = htonl(packet->destination_port);
  packet->hop_count = htonl(packet->hop_count);

  //	struct sockaddr_ll socket_address;

  /*buffer for ethernet frame*/
  void *buffer = (void *)malloc(ETH_FRAME_LEN1);

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
  memcpy((void *)data, (void *)packet, sizeof(SELF_DEFINED_PF_Packet));
  printf("debug:: %d ethernet header: ", __LINE__);
  int l;
  char *buf = buffer;
  for (l = 0; l < 12; l++) {
    printf("%.2x%s", buf[l] & 0xff, (l == 5 || l == 11) ? " " : ":");
  }
  int result =
      sendto(pfSockfd, buffer, ETH_FRAME_LEN, 0,
             (struct sockaddr *)&socket_address, sizeof(socket_address));
  if (result == -1) {
    printf("cannot sendwhat\n");
    err_sys("cannot sendhow");
  }

  free(buffer);

  printf("\nEndSendMessage\n");
  printf("***********************\n\n\n\n");
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

void broadcast(int PFsockFd, SELF_DEFINED_PF_Packet *sendPF_Packet) {

  char vmName[40];
  gethostname(vmName, sizeof(vmName));

  printf("\n***********************************\n");
  printf("\nstart broadcasting in %s\n\n", vmName);

  int j;
  char *ptr;

  printf("\n");

  for (j = 0; j < INTERFACE_SIZE; j++) {

    if (interfaceArray[j].interfaceNum != -1) {

      unsigned char src_mac[ETH_ALEN];
      memcpy(src_mac, interfaceArray[j].macAddr, ETH_ALEN);
      unsigned char dest_mac[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      int if_index = interfaceArray[j].interfaceNum;

      //			(int type, char* source_ipAddr,
      //					int source_port, char*
      //destination_ipAddr, int destination_port,
      //					int hop_count, char* msg)

      SELF_DEFINED_PF_Packet *copy_Packet = createPF_Packet(
          sendPF_Packet->type, sendPF_Packet->source_ipAddr,
          sendPF_Packet->source_port, sendPF_Packet->destination_ipAddr,
          sendPF_Packet->destination_port, sendPF_Packet->hop_count,
          sendPF_Packet->application_msg);
      sendMessage(PFsockFd, src_mac, dest_mac, if_index, copy_Packet);
      free(copy_Packet);
    }
  }

  printf("\nfinish broadcasting in %s\n", vmName);
  printf("***********************************\n\n\n\n");
}

void getAllMacAddress() {

  memset(interfaceArray, 0, sizeof interfaceArray);

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

    //		printf("%d\n", "hwa->if_index");

    if (strcmp(hwa->if_name, "eth0") == 0) {
      struct sockaddr_in *tmp = (struct sockaddr_in *)hwa->ip_addr;
      inet_ntop(AF_INET, &(tmp->sin_addr), local_CanonicalIP, 20);
    }

    if ((strcmp(hwa->if_name, "lo") != 0) &&
        (strcmp(hwa->if_name, "eth0") != 0)) {

      int p = 0;
      for (p = 0; p < INTERFACE_SIZE; p++) {
        if (interfaceArray[p].interfaceNum == -1) {
          interfaceArray[p].interfaceNum = hwa->if_index;
          // printf("debug:: %d mac: %s\n", __LINE__, hwa->if_haddr);
          memcpy(interfaceArray[p].macAddr, hwa->if_haddr, ETH_ALEN);
          break;
        }
      }
    }
  }
  free_hwa_info(hwahead);
}

void printAllInterfaces() {
  printf("interfaces:\n");
  int i;
  for (i = 0; i < INTERFACE_SIZE; i++) {
    if (interfaceArray[i].interfaceNum != -1) {
      printf("interface num: %d, name: %s\n", interfaceArray[i].interfaceNum,
             interfaceArray[i].macAddr);
    }
  }
}

char *getMacAddrByInferFaceNum(int ifaceNum) {

  int q = 0;
  for (q = 0; q < INTERFACE_SIZE; q++) {
    if (interfaceArray[q].interfaceNum == ifaceNum) {
      // printf("debug:: %d::in getMac() mac: %d\n", __LINE__, ifaceNum);
      // printf("debug:: %d::in getMac() mac: %s\n", __LINE__,
      // interfaceArray[q].macAddr);
      return interfaceArray[q].macAddr;
    }
  }
}

int getNodeByIP(char *source_ipAddr) {
  struct in_addr source_ipv4addr;
  inet_pton(AF_INET, source_ipAddr, &source_ipv4addr);
  struct hostent *heSource =
      gethostbyaddr(&source_ipv4addr, sizeof source_ipv4addr, AF_INET);
  char srcHostName[20];
  memset(srcHostName, 0, 20);
  strcpy(srcHostName, heSource->h_name);
  // printf("hostname:%s: \n", srcHostName);

  if (strcmp(srcHostName, "vm10") == 0)
    return 10;
  else {
    char arr[20];
    memset(arr, 0, 20);
    arr[0] = srcHostName[2];
    return atoi(arr);
  }
}

void showPackageInfo(SELF_DEFINED_PF_Packet *packet) {
  int type = packet->type;
  int source_port = packet->source_port;
  int dest_port = packet->destination_port;
  int hop_count = packet->hop_count;
  char source_ipAddr[IPAddr_SIZE];
  char destination_ipAddr[IPAddr_SIZE];
  char application_msg[DATA_SIZE];

  strcpy(source_ipAddr, packet->source_ipAddr);
  strcpy(destination_ipAddr, packet->destination_ipAddr);
  strcpy(application_msg, packet->application_msg);

  printf("\n***********************\n");
  printf("\nshow received_package_info:	\n");

  printf("%s::%d\n\n", "packet->packet_type", type);
  printf("%s::%s\n\n", "canonical’ IP address of source node ", source_ipAddr);
  printf("%s::%d\n\n", "packet->source_port ", source_port);
  printf("%s::%s\n\n", "canonical’ IP address of destination node ",
         packet->destination_ipAddr);
  printf("%s::%d\n\n", "packet->dest_port ", dest_port);
  printf("%s::%d\n\n", "packet->hop_count ", hop_count);
  printf("%s::%s\n\n", "packet->application_msg ", application_msg);
  printf("\n***********************\n");
}

SELF_DEFINED_PF_Packet *receivePacket(int PFsockFd)

{
  void *buffer = (void *)malloc(ETH_FRAME_LEN1); /*Buffer for ethernet frame*/
  memset(buffer, 0, ETH_FRAME_LEN1);
  int length = 0; /*length of the received frame*/

  struct sockaddr_ll recvfromAddr;

  int len = sizeof(recvfromAddr);

  length = recvfrom(PFsockFd, buffer, ETH_FRAME_LEN1, 0,
                    (struct sockaddr *)&recvfromAddr, &len);

  if (length == -1) {
    printf("cannot receive ....");
  }

  unsigned char src_mac[ETH_ALEN];
  unsigned char dest_mac[ETH_ALEN];
  char *ptr;
  int if_index = recvfromAddr.sll_ifindex;
  interface_num = if_index;

  ptr = getMacAddrByInferFaceNum(if_index);
  int i;
  i = IF_HADDR;

  char vmName[40];
  gethostname(vmName, sizeof(vmName));
  printf("\nreceive packet in vmMachine: %s\n", vmName);

  printf("receive packet at address: ");

  do {
    printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
  } while (--i > 0);
  // printf("debug:: %d. mac: %s\n", __LINE__, ptr);

  ptr = buffer + ETH_ALEN;
  i = IF_HADDR;
  printf("from  address : ");
  memcpy(recvAddr, ptr, ETH_ALEN);
  do {
    printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
  } while (--i > 0);
  printf("\n");

  SELF_DEFINED_PF_Packet *packet = (SELF_DEFINED_PF_Packet *)(buffer + 14);

  packet->type = ntohl(packet->type);
  packet->source_port = ntohl(packet->source_port);
  packet->destination_port = ntohl(packet->destination_port);
  packet->hop_count = ntohl(packet->hop_count);

  return packet;
}

void initialMap() {
  int k;
  for (k = 0; k < MAP_SIZE; k++) {
    MapArray[k].portNum = -1;
  }
}

void addMap(int portNum, char *sun_path) {
  int k;
  for (k = 0; k < MAP_SIZE; k++) {
    if (MapArray[k].portNum == -1) {
      MapArray[k].portNum = portNum;
      strcpy(MapArray[k].sun_path, sun_path);
      break;

      //			printf("\naddSunpath :%s\n",
      //MapArray[k].sun_path);
    }
  }
}

char *searchMap(int port) {
  int k;
  for (k = 0; k < MAP_SIZE; k++) {
    if (MapArray[k].portNum == port) {
      return MapArray[k].sun_path;
      //			printf("\nfoundSunpath :%s\n",
      //MapArray[k].sun_path);
      break;
    }
  }
  return NULL;
}

printRoutingTable(RoutingEntry table[]) {
  printf("************************\nprinting routing table\n\n");
  int i;
  for (i = 1; i <= NODES; i++) {
    printf("%d%s: ", i, (i == 10) ? "" : " ");
    int l;
    char *temp = table[i].next_hop;
    printf("next_hop_mac: ");
    for (l = 0; l < 6; l++) {
      printf("%.2x%s", *temp++ & 0xff, (l == 5) ? " " : ":");
    }
    printf("hops_to_dest: %d ", table[i].hops_to_dest);
    printf("timestamp: %ld\n", table[i].timestamp);
    printf("broadcast_id: ");
    for (l = 1; l <= 10; l++) {
      printf("%d ", table[i].highest_broadcast_id[l]);
    }
    printf("\n");
  }
  printf("\n\ndone printing routing table\n***************************\n\n");
}

void removeFromBuffer(SELF_DEFINED_PF_Packet *buffer, int *buffer_count,
                      int remove_index) {
  int i;
  for (i = *buffer_count; i > remove_index; i--) {
    buffer[i - 1] = buffer[i];
  }
  *buffer_count = *buffer_count - 1;
}

void sendMessageFromBuffer(int sockfd, SELF_DEFINED_PF_Packet *buffer,
                           int *buffer_count) {
  time_t curr_time;
  int iterations = *buffer_count;
  int i;
  for (i = 0; i < iterations; i++) {
    int dest_node = getNodeByIP(buffer[i].destination_ipAddr);
    time(&curr_time);
    if (checkAddressNotEmpty(table[dest_node].next_hop) != 0 &&
        table[dest_node].timestamp + stale_time > curr_time) {
      SELF_DEFINED_PF_Packet *packet = &buffer[i];
      sendMessage(
          sockfd, getMacAddrByInferFaceNum(table[dest_node].out_interface),
          table[dest_node].next_hop, table[dest_node].out_interface, packet);
      // free(packet);
      removeFromBuffer(buffer, buffer_count, i);
    }
  }
}

printBufferContent(SELF_DEFINED_PF_Packet *buffer, int buffer_count) {
  int i;
  for (i = 0; i < buffer_count; i++) {
    int source_node = getNodeByIP(buffer[i].source_ipAddr);
    int dest_node = getNodeByIP(buffer[i].destination_ipAddr);
    printf("we have packet: from vm%d to vm%d\n", source_node, dest_node);
  }
}

int main(int argc, char **argv) {
  int broadcast_id = 0;
  int sourcePort = 1;
  time_t curr_time = 0;

  if (argc < 2) {
    printf("usage: <program> <staleness>");
    exit(1);
  }

  stale_time = atoi(argv[1]);
  if (stale_time == 0) {
    printf("positive stale value needed\n");
    exit(2);
  }

  initialMap();

  printf("%s::line %d:\n\n", "ODRSUN_start", __LINE__);

  getAllMacAddress();
  // printAllInterfaces();
  // RoutingEntry table[NODES + 1];
  table = (RoutingEntry *)malloc((NODES + 1) * sizeof(RoutingEntry));
  printf("debug:: %d, size of table: %ld\n", __LINE__,
         sizeof(RoutingEntry) * (NODES + 1));
  memset(table, 0, sizeof(RoutingEntry) * (NODES + 1));

  int num_packets_buffered = 0;
  SELF_DEFINED_PF_Packet send_buffer[100];
  memset(send_buffer, 0, 100 * sizeof(SELF_DEFINED_PF_Packet));
  // int broadcast_id_buffer[20];
  // memset(broadcast_id_buffer, 0, 20 * sizeof(int));

  int PFsockFd;

  PFsockFd = socket(PF_PACKET, SOCK_RAW, htons(SELF_PROTOCOL));
  if (PFsockFd == -1) {
    printf("cannot open socket ....");
  }

  int clientUnixSockfd;
  struct sockaddr_un servaddr, cliaddr;

  clientUnixSockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);

  unlink(UNIXDG_CLIENT);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, UNIXDG_CLIENT);

  Bind(clientUnixSockfd, (SA *)&servaddr, sizeof(servaddr));

  // server unix_sock

  // select
  int nready, client[FD_SETSIZE];
  ssize_t n;
  fd_set rset, allset;

  int maxfd;
  if (clientUnixSockfd > PFsockFd)
    maxfd = clientUnixSockfd;
  else
    maxfd = PFsockFd;

  int i;
  for (i = 0; i < FD_SETSIZE; i++)
    client[i] = -1;

  FD_ZERO(&allset);
  FD_SET(clientUnixSockfd, &allset);
  FD_SET(PFsockFd, &allset);

  // int counter = 0;

  for (;;) {
    printRoutingTable(table);
    rset = allset;
    nready = Select(maxfd + 1, &rset, NULL, NULL, NULL);

    if (FD_ISSET(clientUnixSockfd, &rset)) {

      printf("\n***********************************\n");
      printf("\nstart receving client message\n");

      int n;
      socklen_t len;
      char mesg[MAXLINE];
      memset(mesg, 0, MAXLINE);
      char *ch;

      len = sizeof(cliaddr);
      n = Recvfrom(clientUnixSockfd, mesg, MAXLINE, 0, (SA *)&cliaddr, &len);

      addMap(sourcePort, cliaddr.sun_path);

      printf("%s\n", mesg);

      //			Sendto(clientUnixSockfd, "wgat", strlen("wgat"),
      //0, (SA *) &cliaddr,
      //						sizeof(cliaddr));

      char splitMsg[100];
      memset(splitMsg, 0, 100);
      strcpy(splitMsg, mesg);

      char *destinationAddr = strtok(splitMsg, ";");
      printf("destinationAddr: %s\n", destinationAddr);

      char *destinationPortStr = strtok(NULL, ";");
      //			printf("destinationPortStr: %s\n",
      //destinationPortStr);

      int destinationPortNum = atoi(destinationPortStr);
      printf("destinationPortINT: %d\n", destinationPortNum);

      char *flagStr = strtok(NULL, ";");
      //			printf("flagStr: %s\n", flagStr);
      int flagInt = atoi(flagStr);
      printf("flagInt: %d\n", flagInt);

      char *msg = strtok(NULL, ";");
      printf("msg: %s\n", msg);

      /////////////////////////
      char localVmName[40];
      gethostname(localVmName, sizeof(localVmName));

      struct hostent *hptr = gethostbyname(localVmName);
      char **pptr = hptr->h_addr_list;
      char str[INET_ADDRSTRLEN];

      const char *current_addr;
      for (; *pptr != NULL; pptr++)
        current_addr = Inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str));
      ////////

      printf("source_CanonicalIP: %s\n", current_addr);
      printf("SOURCE_PORT: %d\n", sourcePort);

      printf("stop receving client message\n");
      printf("***********************************\n\n\n\n");

      SELF_DEFINED_PF_Packet *new_packet =
          createPF_Packet(2, (char *)current_addr, sourcePort, destinationAddr,
                          destinationPortNum, 0, msg);
      send_buffer[num_packets_buffered++] = *new_packet;

      broadcast_id++;
      char rreq_msg[30];
      rreq_msg[0] = '0';
      rreq_msg[1] = (char)flagInt + 48;
      char string_id[40];
      sprintf(string_id, "%d", broadcast_id);
      int temp = 0;
      int copyTo = 2;
      while (temp < 30) {
        if (string_id[temp] != '\0') {
          rreq_msg[copyTo++] = string_id[temp];
          temp++;
        } else {
          break;
        }
      }
      rreq_msg[copyTo] = '\0';
      // XXX: put packet in buffer

      int dest = getNodeByIP(destinationAddr);
      if (flagInt == 1) {
        memset(table[dest].next_hop, 0, ETH_ALEN);
      }
      time(&curr_time);
      if (checkAddressNotEmpty(table[dest].next_hop) == 0 ||
          table[dest].timestamp + stale_time < curr_time) {
        printf("buffering new packet. trying to find route.\n\n");
        // create rreq message if we don't have a path
        SELF_DEFINED_PF_Packet *packet =
            createPF_Packet(0, local_CanonicalIP, sourcePort, destinationAddr,
                            destinationPortNum, 0, rreq_msg);
        // printf("debug: %d\n", __LINE__);
        broadcast(PFsockFd, packet);
        // printf("debug: %d\n", __LINE__);
        free(packet);
      }

      sendMessageFromBuffer(PFsockFd, send_buffer, &num_packets_buffered);

      sourcePort++;
    }

    if (FD_ISSET(PFsockFd, &rset)) {
      printf("\n***********************************\n");
      printf("\nstart receving ODR message\n");

      printf("debug:arrive packet detail: %d\n", __LINE__);

      SELF_DEFINED_PF_Packet *packet = receivePacket(PFsockFd);
      int source_node = getNodeByIP(packet->source_ipAddr);
      int dest_node = getNodeByIP(packet->destination_ipAddr);
      int seen_source = (checkAddressNotEmpty(table[source_node].next_hop));
      int incoming_interface = interface_num;
      char currentvm[10];
      memset(currentvm, 0, 10);
      gethostname(currentvm, 10);
      currentvm[0] = '0';
      currentvm[1] = '0';
      int curr_node = atoi(currentvm);
      int hop_count = packet->hop_count + 1;
      int message_type = packet->type;

      printf("source_node: %d\n", source_node);
      printf("dest_node: %d\n", dest_node);
      printf("seen_source: %d\n", seen_source);
      printf("incoming_interface: %d\n", incoming_interface);
      printf("curr_node: %d\n", curr_node);
      printf("hop_count: %d\n", hop_count);
      printf("message_type: %d\n\n", message_type);

      if (message_type == 0) {
        printf("we got an RREQ\n\n");
        int rrep_already_sent = packet->application_msg[0] - 48;
        int forced_discovery = packet->application_msg[1] - 48;
        char received_bid[2000];
        strncpy(received_bid, packet->application_msg, 30);
        received_bid[0] = '0';
        received_bid[1] = '0';
        int id = atoi(received_bid);
        int update_reverse = 1;

        printf("rrep_already_sent flag: %d\n", rrep_already_sent);
        printf("forced_discovery flag: %d\n", forced_discovery);
        printf("id: %d\n\n", id);

        // RoutingEntry source_entry = table[source_node];
        int original_hops_to_source = table[source_node].hops_to_dest;

        if (source_node == curr_node) {
          printf("we sent the rreq. we should ignore this one.\n\n");
          continue;
        }

        time(&curr_time);
        if (hop_count > table[source_node].hops_to_dest ||
            table[source_node].timestamp + stale_time > curr_time) {
          printf("debug:: no update: %d\n", __LINE__);
          update_reverse = 0;
        }
        // printf("debug:: %d, id: %d, highest_id: %d\n", __LINE__, id,
        // source_entry.highest_broadcast_id[source_node]);
        if (id > table[source_node].highest_broadcast_id[source_node] &&
            forced_discovery == 1) {
          printf("debug:: update: %d\n", __LINE__);
          update_reverse = 1;
        }
        if (id > table[source_node].highest_broadcast_id[source_node]) {
          table[source_node].highest_broadcast_id[source_node] = id;
        }
        if (checkAddressNotEmpty(table[source_node].next_hop) == 0) {
          printf("debug:: update: %d\n", __LINE__);
          update_reverse = 1;
        }
        if (update_reverse > 0) {
          printf("updating reverse path for node: %d\n\n", source_node);
          // memcpy(table[source_node].next_hop,
          // getMacAddrByInferFaceNum(incoming_interface), ETH_ALEN);
          memcpy(table[source_node].next_hop, recvAddr, ETH_ALEN);
          // printf("debug:: %d, empty mac: %d\n", __LINE__,
          // checkAddressNotEmpty(source_entry.next_hop));
          // printf("debug:: %d, empty mac: %d\n", __LINE__,
          // checkAddressNotEmpty(table[source_node].next_hop));
          table[source_node].out_interface = incoming_interface;
          // printf("debug:: %d, interface: %d\n", __LINE__,
          // source_entry.out_interface);
          table[source_node].hops_to_dest = hop_count;
          time(&table[source_node].timestamp);
        } else {
          // continue;
        }
        printRoutingTable(table);
        // RoutingEntry dest_entry = table[dest_node];
        if (id > table[dest_node].highest_broadcast_id[source_node]) {
          table[dest_node].highest_broadcast_id[source_node] = id;
        } else {
          continue;
        }
        if (curr_node != dest_node) {
          time(&curr_time);
          if (checkAddressNotEmpty(table[dest_node].next_hop) == 0 ||
              table[dest_node].timestamp + stale_time < curr_time) {
            printf("no route to dest: flooding message\n\n");
            packet->hop_count = packet->hop_count + 1;
            broadcast(PFsockFd, packet);
          } else if (checkAddressNotEmpty(table[dest_node].next_hop) != 0 &&
                     table[dest_node].timestamp + stale_time > curr_time) {
            if (forced_discovery == 0) {
              time(&table[dest_node].timestamp);
              if (rrep_already_sent != 1) {
                printf("we have a route to dest (intermediate node). sending "
                       "rrep to node: %d\n\n",
                       source_node);
                // XXX: create rrep
                char rrep_msg[30];
                memset(rrep_msg, 0, 30);
                strncpy(rrep_msg, packet->application_msg, 30);
                SELF_DEFINED_PF_Packet *out_packet = createPF_Packet(
                    1, packet->destination_ipAddr, packet->destination_port,
                    packet->source_ipAddr, packet->source_port, 0, rrep_msg);
                // XXX: send rrep
                // printf("debug:: %d\n", __LINE__);
                sendMessage(PFsockFd, getMacAddrByInferFaceNum(
                                          table[source_node].out_interface),
                            table[source_node].next_hop,
                            table[source_node].out_interface, out_packet);
                free(out_packet);
                if (seen_source == 0 || original_hops_to_source > hop_count) {
                  printf("first time we saw the source node. flooding.\n\n");
                  packet->hop_count = packet->hop_count + 1;
                  packet->application_msg[0] = '1';
                  broadcast(PFsockFd, packet);
                }
              }
            } else if (forced_discovery == 1) {
              printf("we are intermediate node, but its forced discovery. "
                     "flooding..\n\n");
              packet->hop_count = packet->hop_count + 1;
              broadcast(PFsockFd, packet);
            }
          }
        } else {
          // current is destination node
          if (rrep_already_sent != 1) {
            printf("we are the dest node. sending rrep to node: %d\n\n",
                   source_node);
            // XXX: create rrep
            char rrep_msg[30];
            memset(rrep_msg, 0, 30);
            strncpy(rrep_msg, packet->application_msg, 30);
            SELF_DEFINED_PF_Packet *out_packet = createPF_Packet(
                1, packet->destination_ipAddr, packet->destination_port,
                packet->source_ipAddr, packet->source_port, 0, rrep_msg);
            if (forced_discovery == 1) {
              // XXX: set rrep with forced discovery
              printf("rreq was forced discovery. this rrep should also be "
                     "forced discovery.\n\n");
              out_packet->application_msg[1] = '1';
            }
            // XXX: send rrep
            // printf("debug:: %d\n", __LINE__);
            sendMessage(PFsockFd, getMacAddrByInferFaceNum(
                                      table[source_node].out_interface),
                        table[source_node].next_hop,
                        table[source_node].out_interface, out_packet);
            free(out_packet);
            if (seen_source == 0 || original_hops_to_source > hop_count) {
              printf("first time we've seen source. flooding..\n\n");
              packet->hop_count = packet->hop_count + 1;
              packet->application_msg[0] = '1';
              broadcast(PFsockFd, packet);
            }
          }
        }
      } else if (message_type == 1) {
        printf("we got an rrep\n\n");
        int forced_discovery = packet->application_msg[1] - 48;
        // RoutingEntry source_entry = table[source_node];
        char received_bid[2000];
        memset(received_bid, 0, 2000);
        strncpy(received_bid, packet->application_msg, 30);
        received_bid[0] = '0';
        received_bid[1] = '0';
        int id = atoi(received_bid);
        time(&curr_time);
        if (forced_discovery == 1 ||
            hop_count < table[source_node].hops_to_dest ||
            table[source_node].timestamp + stale_time < curr_time ||
            checkAddressNotEmpty(table[source_node].next_hop) == 0) {
          printf("updating forward path for node: %d\n\n", source_node);
          memcpy(table[source_node].next_hop, recvAddr, ETH_ALEN);
          table[source_node].out_interface = incoming_interface;
          table[source_node].hops_to_dest = hop_count;
          time(&table[source_node].timestamp);
          table[source_node].highest_broadcast_id[source_node] = id;
        }
        printRoutingTable(table);

        if (dest_node == curr_node) {
          printf("this rrep is for us. we got a route!\n\n");

          // printf("sending back to client!\n\n");
          // send to client
          /*
           msg_send2(clientUnixSockfd, packet->destination_ipAddr,
           packet->destination_port, "we found a route",
           forced_discovery, searchMap(SOURCE_PORT));
           */
          printf("debug:: %d, num packets buffered: %d\n", __LINE__,
                 num_packets_buffered);
          sendMessageFromBuffer(PFsockFd, send_buffer, &num_packets_buffered);
          printf("debug:: %d, num packets buffered: %d\n", __LINE__,
                 num_packets_buffered);
          // printBufferContent(table, num_packets_buffered);

        } else {
          printf("we are intermediate node. we should send rrep back to "
                 "source.\n\n");
          // RoutingEntry dest_entry = table[dest_node];
          packet->hop_count = packet->hop_count + 1;
          // XXX: send packet (rrep)
          // printf("debug:: %d\n", __LINE__);
          sendMessage(PFsockFd,
                      getMacAddrByInferFaceNum(table[dest_node].out_interface),
                      table[dest_node].next_hop, table[dest_node].out_interface,
                      packet);
        }
      } else {
        int forced_discovery = packet->application_msg[1] - 48;
        // application message
        // RoutingEntry dest_entry = table[dest_node];
        // printf("debug:: %d\n", __LINE__);
        memcpy(table[source_node].next_hop, recvAddr, ETH_ALEN);
        // printf("debug:: %d\n", __LINE__);
        table[source_node].out_interface = incoming_interface;
        // printf("debug:: %d\n", __LINE__);
        table[source_node].hops_to_dest = hop_count;
        // printf("debug:: %d\n", __LINE__);
        time(&table[source_node].timestamp);
        if (dest_node == curr_node) {
          // if the message was for us

          if (strcmp(packet->application_msg, "request time") == 0) {
            // if request message, we contact server to get response
            printf("detected time request for server. passing to server "
                   "application\n\n");
            int serverSockfd;
            struct sockaddr_un cliaddr, servaddr;

            serverSockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);

            bzero(&cliaddr, sizeof(cliaddr)); /* bind an address for us */
            cliaddr.sun_family = AF_LOCAL;

            char *tmpArray = malloc(sizeof(char) * 1024);

            strcpy(tmpArray, "/tmp/myTmpabcd-XXXXXX");
            mkstemp(tmpArray);
            unlink(tmpArray);

            strcpy(cliaddr.sun_path, tmpArray);

            Bind(serverSockfd, (SA *)&cliaddr, sizeof(cliaddr));

            bzero(&servaddr, sizeof(servaddr)); /* fill in server's address */
            servaddr.sun_family = AF_LOCAL;
            strcpy(servaddr.sun_path, UNIXDG_SERVER);

            int n;
            char sendline[MAXLINE], recvline[MAXLINE + 1];

            char msgSequence[MAXLINE];
            sprintf(msgSequence, "%s;%d;%s", packet->source_ipAddr,
                    packet->source_port, packet->application_msg);

            Sendto(serverSockfd, msgSequence, strlen(msgSequence), 0,
                   (SA *)&servaddr, sizeof(servaddr));

            n = Recvfrom(serverSockfd, recvline, MAXLINE, 0, NULL, NULL);

            recvline[n] = 0; /* null terminate */
            Fputs(recvline, stdout);

            printf("\nfinish receving time info from server\n");

            char response[30];
            memset(response, 0, 30);
            strncpy(response, "response message",
                    30); // we need string from server
            SELF_DEFINED_PF_Packet *out_packet = createPF_Packet(
                2, packet->destination_ipAddr, packet->destination_port,
                packet->source_ipAddr, packet->source_port, 0, recvline);
            send_buffer[num_packets_buffered++] = *out_packet;
          } else {
            printf("detected time response from server. passing to client "
                   "application\n\n");
            //						printf("5678testport\ntestport\ntestport\ntestport\n
            //%d",packet->destination_port);
            msg_send2(clientUnixSockfd, packet->source_ipAddr,
                      packet->source_port, packet->application_msg,
                      forced_discovery, searchMap(packet->destination_port));
          }
        } else {
          printf("we are intermediate router. passing along message.\n\n");
          packet->hop_count = hop_count;
          send_buffer[num_packets_buffered++] = *packet;
        }

        printf("debug:: %d, num packets buffered: %d\n", __LINE__,
               num_packets_buffered);
        sendMessageFromBuffer(PFsockFd, send_buffer, &num_packets_buffered);
        printf("debug:: %d, num packets buffered: %d\n", __LINE__,
               num_packets_buffered);
        if (num_packets_buffered > 0) {
          /////////////////////////
          char localVmName[40];
          gethostname(localVmName, sizeof(localVmName));

          struct hostent *hptr = gethostbyname(localVmName);
          char **pptr = hptr->h_addr_list;
          char str2[INET_ADDRSTRLEN];

          const char *current_addr;
          for (; *pptr != NULL; pptr++) {
            // sourceIP
            current_addr =
                Inet_ntop(hptr->h_addrtype, *pptr, str2, sizeof(str2));
          }

          broadcast_id++;
          char rreq_msg[30];
          rreq_msg[0] = '0';
          rreq_msg[1] = '1';
          char string_id[40];
          sprintf(string_id, "%d", broadcast_id);
          int temp = 0;
          int copyTo = 2;
          while (temp < 30) {
            if (string_id[temp] != '\0') {
              rreq_msg[copyTo++] = string_id[temp];
              temp++;
            } else {
              break;
            }
          }
          rreq_msg[copyTo] = '\0';
          ////////
          SELF_DEFINED_PF_Packet *rreq_packet =
              createPF_Packet(0, (const char *)current_addr, sourcePort,
                              packet->destination_ipAddr,
                              packet->destination_port, 0, rreq_msg);
          // printf("debug: %d\n", __LINE__);
          broadcast(PFsockFd, rreq_packet);
          // printf("debug: %d\n", __LINE__);
          free(rreq_packet);
        }
      }
    }
  }
}
