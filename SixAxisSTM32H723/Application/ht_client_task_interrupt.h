#ifndef  __HT_CLIENT_TASK_INTERRUPT_H__

#include "ht_udp_server.h"

int8_t
ht_client_interrupt_task_motor_stop(ServerData *t);

int8_t
ht_client_interrupt_task_window(ServerData *t);

int8_t
ht_client_interrupt_task_stop_and_close_window(ServerData *t);

#endif // ! __HT_CLIENT_
