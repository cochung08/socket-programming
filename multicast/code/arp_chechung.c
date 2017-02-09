#include "hw_addrs.h"
#include "unp.h"
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

#define ETH_ALEN 6
#define IP_ALEN 4
#define INTERFACE_SIZE 20
#define IPADDR_SIZE 20
#define ARP_TABLE_SIZE 20
#define SELF_PROTOCOL 0x5403
#define ARP_LEN 50
#define UNIXDG_ARP "/tmp/unix5403.dg"

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

    //		printf("%d\n", "hwa->if_index");

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

void printArpMessage(char *buffer) {
  printf("\n****************************************\n");
  printf("Arp Message:\n\n");
  printf("ethernet header:\n");
  printf("dest mac: ");
  int l;
  char *buf = buffer;
  for (l = 0; l < 12; l++) {
    printf("%.2x%s", buf[l] & 0xff, (l == 5) ? " \nsource mac: " : ":");
  }
  printf("\nprototype number: 0x");
  for (l = 12; l < 14; l++) {
    printf("%.2x", buf[l] & 0xff);
  }
  printf("\n\narp content:\n");
  printf("group unique arp id: 0x");
  for (; l < 16; l++) {
    printf("%.2x", buf[l]);
  }
  printf("\nhatype: 0x");
  for (; l < 18; l++) {
    printf("%.2x", buf[l]);
  }
  printf("\nprottype: 0x");
  for (; l < 20; l++) {
    printf("%.2x", buf[l]);
  }
  printf("\nhardsize: 0x");
  for (; l < 21; l++) {
    printf("%.2x", buf[l]);
  }
  printf("\nprotsize: 0x");
  for (; l < 22; l++) {
    printf("%.2x", buf[l]);
  }
  int op = ((int)((unsigned char)buf[22])) * 2 + (int)((unsigned char)buf[23]);
  printf("\nmessage type: %d", op);
  printf("\nsender mac: ");
  for (l = 24; l < 30; l++) {
    printf("%.2x%s", buf[l] & 0xff, (l == 29) ? " " : ":");
  }
  printf("\nsender ip: ");
  for (; l < 34; l++) {
    printf("%d%s", (int)((unsigned char)buf[l]), (l == 33) ? " " : ".");
  }
  printf("\ntarget mac: ");
  for (; l < 40; l++) {
    printf("%.2x%s", buf[l] & 0xff, (l == 39) ? " " : ":");
  }
  printf("\ntarget ip: ");
  for (; l < 44; l++) {
    printf("%d%s", (int)((unsigned char)buf[l]), (l == 43) ? "\n" : ".");
  }
  printf("\n\n****************************************\n\n\n");
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

  printArpMessage(buffer);

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
  printArpMessage(buffer);
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
  // printf("line: %d, ip string: %s\n", __LINE__, ipPresentation);
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

int main(int argc, char *argv[]) {

  numEntries = 0;
  getAllMacAddress();
  printAllInterfaces();

  // create PF_PACKET socket
  int PFsockFd = Socket(PF_PACKET, SOCK_RAW, htons(SELF_PROTOCOL));

  // create Unix Domain Socket
  int arpUnixSockFd = Socket(AF_LOCAL, SOCK_STREAM, 0);
  unlink(UNIXDG_ARP);
  struct sockaddr_un servaddr, cliaddr;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, UNIXDG_ARP);
  Bind(arpUnixSockFd, (SA *)&servaddr, sizeof(servaddr));
  Listen(arpUnixSockFd, LISTENQ);

  socklen_t clilen = sizeof(cliaddr);
  // int conn_fd = Accept(arpUnixSockFd, (SA *) &cliaddr, &clilen);

  // monitor both sockets
  int nready;
  fd_set rset, allset;
  int maxfd;
  maxfd = ((arpUnixSockFd > PFsockFd) ? arpUnixSockFd : PFsockFd) + 1;
  FD_ZERO(&allset);
  FD_SET(arpUnixSockFd, &allset);
  FD_SET(PFsockFd, &allset);
  for (;;) {
    // printArpTable();
    // socklen_t clilen = sizeof(cliaddr);
    rset = allset;
    if ((nready = Select(maxfd, &rset, NULL, NULL, NULL)) < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        err_sys("Select error");
      }
    }
    printf("got a packet!\n");
    if (FD_ISSET(arpUnixSockFd, &rset)) {
      // request from tour client
      int conn_fd = Accept(arpUnixSockFd, (SA *)&cliaddr, &clilen);
      printf("got a packet from domain socket\n");
      char ip[IPADDR_SIZE];
      memset(ip, 0, IPADDR_SIZE);
      // strcpy(ip, "130.245.156.21"); // hard code for now, comment out later
      int n;

      // this code is used to get ip from tour client
      // uncomment this when we integrate the 2 parts

      while (1) {
        if ((n = Read(conn_fd, ip, IPADDR_SIZE)) > 0) {
          break;
        }
        if (n < 0 && errno != EINTR) {
          err_sys("read error");
        }
      }

      // checks if we have a response for the requested ip
      int index = indexOfIP(ip);
      if (index == -1) {
        // we got a request for an ip that we don't have a response to
        // cache this request and broadcast for the hw addr
        printf("got a request for ip we don't have response for\n");
        ArpEntry arpEntry;
        memset(&arpEntry, 0, sizeof(arpEntry));
        strcpy(arpEntry.ipAddr, ip);
        arpEntry.conn_fd = conn_fd;
        arpTable[numEntries++] = arpEntry;

        unsigned char dest_mac[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        ArpMessage arpMessage;
        memset(&arpMessage, 0, sizeof(ArpMessage));
        initArpMessage(&arpMessage);
        setIPAddr((char *)&arpMessage.target_ip, ip);
        setIPAddr((char *)&arpMessage.sender_ip, interfaceArray[0].ipAddr);
        memcpy((void *)&arpMessage.target_mac, (void *)dest_mac, ETH_ALEN);
        memcpy((void *)&arpMessage.sender_mac,
               (void *)&interfaceArray[0].macAddr, ETH_ALEN);
        arpMessage.op[1] = 0xFF;
        // printPacketBytes(&arpMessage, sizeof(ArpMessage));
        printf("sending broadcast arp message\n");
        sendArpMessage(PFsockFd, (char *)&arpMessage.sender_mac,
                       (char *)&arpMessage.target_mac,
                       interfaceArray[0].interfaceNum, &arpMessage);
      } else {
        // we have a response for the request
        // let's send it back to the tour client
        if (checkAddressNotEmpty(arpTable[index].macAddr) == 0) {
          printf("we have the record of ip, but not mac\n");
        } else {
          printf("we already have the response. no need to broadcast\n");
          // printf("sending to conn_fd: %d\n", conn_fd);
          Writen(conn_fd, arpTable[index].macAddr, ETH_ALEN);
          Close(conn_fd);
        }
      }
    }

    if (FD_ISSET(PFsockFd, &rset)) {
      // request/response from another arp
      ArpMessage arpMessage;
      recvArpMessage(PFsockFd, &arpMessage);
      printf("we got an arp message from pf packet socket\n");
      // printf("line: %d, message type: %d.%d\n", __LINE__, (int)
      // arpMessage.op[0], (int) arpMessage.op[1]);
      if (arpMessage.op[0] == 0x00 && arpMessage.op[1] == 0xFF) {
        // this is a request message
        printf("we got a request arp message\n");
        int foundIndex;
        if ((foundIndex = checkEntryExist(arpMessage.sender_ip,
                                          arpMessage.sender_mac)) > 0) {
          // if entry exists, we modify it
          printf("we have an entry. we will update it\n");
          ArpEntry arpEntry = arpTable[foundIndex];
          arpEntry.sll_ifindex = socket_address.sll_ifindex;
          arpEntry.sll_hatype = socket_address.sll_hatype;
        }
        // printf("line: %d, we are dest: %d\n", __LINE__,
        // checkIsDestNode(arpMessage.target_ip));
        if (checkIsDestNode(arpMessage.target_ip)) {
          // if we are destination node, we create an entry if it doesn't exist
          printf("we are the destination node\n");
          if (foundIndex == -1) {
            printf("we don't have an entry. we will create a new entry\n");
            ArpEntry arpEntry;
            memset(&arpEntry, 0, sizeof(ArpEntry));
            char ipPresentation[IPADDR_SIZE];
            memset(ipPresentation, 0, IPADDR_SIZE);
            ip_ntop(arpMessage.sender_ip, ipPresentation);
            memcpy(arpEntry.ipAddr, ipPresentation, IPADDR_SIZE);
            memcpy(arpEntry.macAddr, arpMessage.sender_mac, ETH_ALEN);
            arpEntry.sll_hatype = socket_address.sll_hatype;
            arpEntry.sll_ifindex = socket_address.sll_ifindex;
            arpEntry.conn_fd = -1;
            arpTable[numEntries++] = arpEntry;

            ArpMessage replyMessage;
            memset(&replyMessage, 0, sizeof(ArpMessage));
            initArpMessage(&replyMessage);
            replyMessage.op[0] = 0xFF;
            memcpy((void *)replyMessage.sender_ip, (void *)arpMessage.target_ip,
                   IP_ALEN);
            memcpy((void *)replyMessage.sender_mac,
                   (void *)interfaceArray[0].macAddr, ETH_ALEN);
            memcpy((void *)replyMessage.target_ip, (void *)arpMessage.sender_ip,
                   IP_ALEN);
            memcpy((void *)replyMessage.target_mac,
                   (void *)arpMessage.sender_mac, ETH_ALEN);
            printf("we will send the reply back to the sender\n");
            sendArpMessage(PFsockFd, (char *)&replyMessage.sender_mac,
                           (char *)&replyMessage.target_mac,
                           interfaceArray[0].interfaceNum, &replyMessage);
          } else {
            ArpMessage replyMessage;
            memset(&replyMessage, 0, sizeof(ArpMessage));
            initArpMessage(&replyMessage);
            replyMessage.op[0] = 0xFF;
            memcpy((void *)replyMessage.sender_ip, (void *)arpMessage.target_ip,
                   IP_ALEN);
            memcpy((void *)replyMessage.sender_mac,
                   (void *)interfaceArray[0].macAddr, ETH_ALEN);
            memcpy((void *)replyMessage.target_ip, (void *)arpMessage.sender_ip,
                   IP_ALEN);
            memcpy((void *)replyMessage.target_mac,
                   (void *)arpMessage.sender_mac, ETH_ALEN);
            printf("we will send the reply back to the sender\n");
            sendArpMessage(PFsockFd, (char *)&replyMessage.sender_mac,
                           (char *)&replyMessage.target_mac,
                           interfaceArray[0].interfaceNum, &replyMessage);
          }
        }
      } else if (arpMessage.op[0] == 0xFF && arpMessage.op[1] == 0x00) {
        // this is a reply message
        printf("we got a reply arp message\n");
        char ipPresentation[IPADDR_SIZE];
        memset(ipPresentation, 0, IPADDR_SIZE);
        ip_ntop(arpMessage.sender_ip, ipPresentation);
        int foundIndex;
        // printf("line:%d, index of ip: %d\n", __LINE__,
        // indexOfIP(ipPresentation));
        if ((foundIndex = indexOfIP(ipPresentation)) >= 0) {
          printf("updating entry using the reply\n");
          // if entry exists, we modify it
          // ArpEntry arpEntry = arpTable[foundIndex];
          memcpy((void *)arpTable[foundIndex].macAddr,
                 (void *)arpMessage.sender_mac, ETH_ALEN);
          arpTable[foundIndex].sll_ifindex = socket_address.sll_ifindex;
          arpTable[foundIndex].sll_hatype = socket_address.sll_hatype;
          if (arpTable[foundIndex].conn_fd > -1) {
            printf(
                "we have a tour client who wants the reply. let's send it\n");
            Writen(arpTable[foundIndex].conn_fd, arpTable[foundIndex].macAddr,
                   ETH_ALEN);
            Close(arpTable[foundIndex].conn_fd);
            arpTable[foundIndex].conn_fd = -1;
          }
        }
      }
    }
    printArpTable();
  }

  printf("\n");
  return 0;
}
