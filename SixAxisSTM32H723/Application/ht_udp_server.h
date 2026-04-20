#ifndef __HT_UDP_SERVER_H__
#define __HT_UDP_SERVER_H__


#include "main.h"

typedef struct _ServerData ServerData;
struct _ServerData {
	uint32_t len;
  uint8_t* buf;
};


#define UDP_SERVER_CONTROL_PORT 6001

int8_t 
ht_udp_server_init(void);

int8_t
ht_udp_server_data_new_and_send (uint8_t *buf, uint16_t len);

int8_t
ht_udp_server_data_send (ServerData *server);

ServerData*
ht_udp_server_data_new (uint16_t len);

ServerData*
ht_udp_server_data_new_with_data (uint8_t *buf, uint16_t len);

void
ht_udp_server_data_free (ServerData *server);


#endif // !__HT_UDP_SERVER_SOCKET_H__

