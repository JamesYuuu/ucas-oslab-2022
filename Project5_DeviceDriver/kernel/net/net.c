#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <printk.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    while (is_send_full())
    {
        e1000_write_reg(e1000,E1000_IMS,E1000_IMS_TXQE | E1000_IMS_RXDMT0);
        int cpu_id = get_current_cpu_id();
        do_block(&current_running[cpu_id]->list, &send_block_queue);
    }
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    int len = e1000_transmit(txpacket, length);

    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full

    return len;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    int len = 0;
    for (int i=0;i<pkt_num;i++)
    {
        while (is_recv_empty())
        {
            int cpu_id = get_current_cpu_id();
            do_block(&current_running[cpu_id]->list, &send_block_queue);
        }
        pkt_lens[i] = e1000_poll(rxbuffer + len);
        len += pkt_lens[i];
    }
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    return len;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
    uint32_t mask = e1000_read_reg(e1000,E1000_ICR);
    if (mask & E1000_ICR_TXQE)
    {
        e1000_write_reg(e1000,E1000_IMC,E1000_IMC_TXQE);
        unblock_send_queue();
    }
    if (mask & E1000_ICR_RXDMT0)
    {
        unblock_recv_queue();
    }
}

void unblock_send_queue(void)
{
    while (!list_empty(&send_block_queue)) 
        do_unblock(send_block_queue.prev);  
}

void unblock_recv_queue(void)
{
    while (!list_empty(&recv_block_queue)) 
        do_unblock(recv_block_queue.prev);  
}

void unblock_net_queue(void)
{
    unblock_send_queue();
    unblock_recv_queue();
}
    