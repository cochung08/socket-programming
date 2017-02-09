#define SOURCE_PORT 1234
#define DEST_PORT 5678
#define UNIXDG_CLIENT "/tmp/unix123.dg"
#define UNIXDG_SERVER "/tmp/unix456.dg"

#define ETH_FRAME_LEN1 1370
#define DATA_SIZE 1300

#define SELF_PROTOCOL 0x5403
#define IPAddr_SIZE 20
#define ETH_ALEN 6
#define INTERFACE_SIZE 40
#define MAP_SIZE 100

typedef struct interface {
  char macAddr[ETH_ALEN];
  int interfaceNum;
} Interface;

typedef struct map_port_path {
  int portNum;
  char sun_path[108];

} Map_port_path;

typedef struct sELF_DEFINED_PF_Packet {
  int type;                             // 4 byte
  char source_ipAddr[IPAddr_SIZE];      // 20 byte
  int source_port;                      // 4 byte
  char destination_ipAddr[IPAddr_SIZE]; // 20 byte
  int destination_port;                 // 4 byte
  int hop_count;                        // 4 byte
  char application_msg[DATA_SIZE];
} SELF_DEFINED_PF_Packet;
