#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

#include <os/list.h>
#include <type.h>

#define PKT_NUM 32

void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);

void unblock_net_queue(void);
void unblock_send_queue(void);
void unblock_recv_queue(void);

#endif  // !__INCLUDE_NET_H__