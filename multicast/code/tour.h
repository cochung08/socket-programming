#define INET_ADDRSTRLEN 16
#define VMNAME_SIZE 40
#define RT_PROTOCAL 201
#define TOTAL_VM_SIZE 70
#define IP_IDENTIFICATION 5914
#define IP_WHOLE_PACK_SIZE 2000

#define PING_SIZE 11
#define MSG_SIZE 200
#define ETH_ALEN 6
#define ETHR_FRAME_LEN 1510

#define IP_ALEN 4
#define INTERFACE_SIZE 20
#define IPADDR_SIZE 20
#define ARP_TABLE_SIZE 20
#define SELF_PROTOCOL 0x5403
#define ARP_LEN 50
#define UNIXDG_ARP "/tmp/unix5403.dg"

typedef struct payload {
  int next_ip_idx;
  char multi_port[10];
  char multi_ip[INET_ADDRSTRLEN];
  int *hostArray[TOTAL_VM_SIZE];
} PayLoad;

typedef struct hwaddr {
  int sll_ifindex;
  unsigned short sll_hatype;
  unsigned char sll_halen;
  unsigned char sll_addr[8];
};

typedef struct ipAddrMacAddr {
  char ipAddr[INET_ADDRSTRLEN];
  char macAddr[6];
} IpAddrMacAddr;
