# Project5 Device Driver

## 任务一：实现网卡初始化与轮询发送数据帧的功能

### IOremap的实现

对外设的访问，我们通过直接访问其物理地址实现，但由于在`kernel`中开启的虚存机制，访问任何物理地址都需要通过内核虚地址实现，因此我们需要使用`ioremap`函数将物理地址映射到内核虚地址中。

我先实现了一个函数`map_page_net`，用于将物理地址通过指定的页表映射到指定的虚拟地址上。

```c
static void map_page_net(uint64_t va, uint64_t pa, PTE* pgdir)
{
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;

    if (pgdir[vpn2]==0)
    {
        set_pfn(&pgdir[vpn2], kva2pa(allocPage()->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pgdir[vpn2])));
    }
    set_attribute(&pgdir[vpn2], _PAGE_PRESENT);

    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
    if (pmd[vpn1]==0)
    {
        set_pfn(&pmd[vpn1], kva2pa(allocPage()->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    set_attribute(&pmd[vpn1], _PAGE_PRESENT);

    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);

    return;
}
```

然后我实现了`ioremap`和`iounmap`函数，分别是建立映射和取消映射。

```c
void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    uintptr_t vaddr = io_base;
    while (size > 0)
    {
        map_page_net(io_base, phys_addr, (PTE*)PGDIR_KVA);
        io_base += NORMAL_PAGE_SIZE;
        phys_addr += NORMAL_PAGE_SIZE;
        size -= NORMAL_PAGE_SIZE;
    }
    local_flush_tlb_all();
    return vaddr;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
    uint64_t va = (uint64_t)io_addr;
    PTE* pgdir = (PTE*)PGDIR_KVA;

    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;

    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    del_attribute(&pte[vpn0], _PAGE_PRESENT);

    local_flush_tlb_all();
}
```

### 初始化E1000网卡

我们分别需要初始化发送，即初始化描述符、缓冲区和寄存器。具体的初始化工作罗列如下：

1. 初始化发送描述符的`addr`和`cmd`位，其中`addr`为缓冲区的物理地址，`cmd`为控制位。
2. 初始化`TDBAL`和`TDBAH`寄存器，代表描述符的地址。
3. 初始化`TDLEN`寄存器，代表描述符的数量。
4. 初始化`TDH`和`TDT`寄存器，代表描述符的头和尾。
5. 初始化`TCTL`寄存器，代表发送控制。

```c
/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for (int i=0;i<TXDESCS;i++)
    {
        tx_desc_array[i].addr = kva2pa(tx_pkt_buffer[i]);
        tx_desc_array[i].cmd  = E1000_TXD_CMD_RS & ~E1000_TXD_CMD_DEXT;
    }
    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    // note that our physical addr is 32bit
    e1000_write_reg(e1000, E1000_TDBAL, kva2pa(tx_desc_array));
    e1000_write_reg(e1000, E1000_TDBAH, 0);
    e1000_write_reg(e1000, E1000_TDLEN, TXDESCS * sizeof(struct e1000_tx_desc));
	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);
    /* TODO: [p5-task1] Program the Transmit Control Register */
    e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD);
}
``` 

### 完成发送数据帧的功能

我们通过`e1000_transmit`函数来完成发送数据帧，需要做如下工作。

1. 拷贝数据到缓冲区。
2. 设置描述符的`length`和`cmd`，其中`length`为数据帧的长度，`cmd`为发送控制。
3. 更新`TDT`寄存器，代表描述符的尾。

```c 
/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    // find the next tx descriptor
    // FIXME: what if length > RX_PKT_SIZE?
    uint32_t index = e1000_read_reg(e1000, E1000_TDT);
    memcpy(tx_pkt_buffer[index], txpacket, length);
    tx_desc_array[index].length = length;
    tx_desc_array[index].cmd |= E1000_TXD_CMD_EOP;
    e1000_write_reg(e1000, E1000_TDT, (index + 1) % TXDESCS);
    local_flush_dcache();
    return length;
}
```

通过系统调用`do_net_send`为用户程序提供接口，当发送队列满时，进行阻塞。

```c
int do_net_send(void *txpacket, int length)
{
    while (is_send_full())
    {
        int cpu_id = get_current_cpu_id();
        do_block(&current_running[cpu_id]->list, &send_block_queue);
    }
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    int len = e1000_transmit(txpacket, length);

    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full

    return len;  // Bytes it has transmitted
}
```

发送队列满的判断逻辑为，当`TDH - TDT = 1`时，认为发送队列已满。

```c
int is_send_full(void)
{
    return ((e1000_read_reg(e1000, E1000_TDH) - e1000_read_reg(e1000, E1000_TDT)) == 1);
}
```

## 任务二：实现网卡初始化与轮询接收数据帧的功能

与发送类似，我们也需要初始化接收，具体步骤如下：

1. 初始化接收描述符的`addr`并清空其余位，其中`addr`为缓冲区的物理地址。
2. 初始化`RDBAL`和`RDBAH`寄存器，代表描述符的地址。
3. 初始化`RDLEN`寄存器，代表描述符的数量。
4. 初始化`RDH`和`RDT`寄存器，代表描述符的头和尾。
5. 初始化`RCTL`寄存器，代表接收控制。

```c
/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    e1000_write_reg(e1000, E1000_RAL, MAC_ADDR_LOW);
    e1000_write_reg(e1000, E1000_RAH, MAC_ADDR_HIGH | E1000_RAH_AV);
    /* TODO: [p5-task2] Initialize rx descriptors */
    for (int i=0;i<RXDESCS;i++)
    {
        rx_desc_array[i].addr    = kva2pa(rx_pkt_buffer[i]);
        rx_desc_array[i].length  = 0;
        rx_desc_array[i].csum    = 0;
        rx_desc_array[i].errors  = 0;
        rx_desc_array[i].status  = 0;
        rx_desc_array[i].special = 0;
    }
    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    e1000_write_reg(e1000, E1000_RDBAL, kva2pa(rx_desc_array));
    e1000_write_reg(e1000, E1000_RDBAH, 0);
    e1000_write_reg(e1000, E1000_RDLEN, RXDESCS * sizeof(struct e1000_rx_desc));
    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);
    /* TODO: [p5-task2] Program the Receive Control Register */
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_RDMTS_HALF | E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048 & ~E1000_RCTL_BSEX);
}
```

同样我们通过`e1000_poll`函数来完成接收数据帧，需要做如下工作。

1. 读取`RDT`寄存器，代表描述符的尾，找到需要接收的描述符。
2. 拷贝缓冲区到目标地址
3. 读取描述符中的长度
4. 清空已接收的描述符
5. 更新`RDT`寄存器，代表描述符的尾

```c
/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    local_flush_dcache();
    // find the next rx descriptor
    uint32_t index = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;
    // FIXME: what if length > RX_PKT_SIZE?
    int len = rx_desc_array[index].length;
    memcpy(rxbuffer, rx_pkt_buffer[index], len);
    // reset the descriptor
    rx_desc_array[index].length  = 0;
    rx_desc_array[index].csum    = 0;
    rx_desc_array[index].errors  = 0;
    rx_desc_array[index].status  = 0;
    rx_desc_array[index].special = 0;
    e1000_write_reg(e1000, E1000_RDT, index);
    local_flush_dcache();
    return len;
}
```

同样，我们提供系统调用`do_net_recv`为用户程序提供接收数据帧的功能，当没有数据帧时，我们需要阻塞进程。

```c
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
```

接收队列空的逻辑为当前描述符`status`的`DD`位不为1，为了避免频繁的读取`E1000_RDT`寄存器，我们通过`index`来保存之前的`RDT`值，直到完成拷贝后，再更新`index`。

```c
static uint32_t index;
static uint32_t is_copied;

int is_recv_empty(void)
{
    local_flush_dcache();
    if (is_copied) 
    {
        index = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;
        is_copied = 0;
    }
    if (rx_desc_array[index].status & E1000_RXD_STAT_DD)
    {
        is_copied = 1;      
        return 0;
    }
    return 1;
}
```

## 任务三：利用时钟中断实现有阻塞的网卡收发包功能

在有阻塞的基础上，我们每次进入时钟中断，都释放所有的被阻塞进程，让它们重新请求，即在`do_scheduler`中执行函数`unblock_net_queue`。

```c
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
```

## 任务四：有网卡中断的收发包

不同于任务三，我们实现网卡中断，首先在`e1000_configure_rx`中开启`E1000_IMS_RXDMT0`中断，使得硬件接收一定数量的包后会发送中断。在发送队列满的时候开启`E1000_IMS_TXQE`中断，使得硬件发送队列空时发送中断。

```c
/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    ...
    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);
}

int do_net_send(void *txpacket, int length)
{
    while (is_send_full())
    {
        e1000_write_reg(e1000,E1000_IMS,E1000_IMS_TXQE | E1000_IMS_RXDMT0);
        int cpu_id = get_current_cpu_id();
        do_block(&current_running[cpu_id]->list, &send_block_queue);
    }
    ...
}
```

当收到硬件中断时，我们进入函数`handle_irq_ext`，通过`plic_claim()`读取中断号，如果是网卡中断，我们调用`net_handle_irq`进行处理，处理完成后，调用`plic_complete()`，重新写回中断号，告诉`plic`我们处理完成。

```c
void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task4] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...
    uint32_t hwirq = plic_claim();
    if (hwirq == PLIC_E1000_PYNQ_IRQ) 
        net_handle_irq();
    plic_complete(hwirq);
}
```

在`net_handle_irq`函数中，我们根据`E1000_ICR`寄存器的值，判断是发送中断还是接收中断，分别释放相应的阻塞队列，如果是发送中断，则清除中断标志位。

```c
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
```

## 任务五：网卡同时收发

在之前的基础上，我们编写`echo.c`测试程序，启动两个线程，分别用于发送和接收，实现同时收发的功能,示例代码如下:

```c
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>

#define MAX_RECV_CNT 32
#define RX_PKT_SIZE 200

#define PRINT_LOCATION_SENDER 2
#define PRINT_LOCATION_RECEIVER 8

#define Ethernet_Header 14
#define IP_Header 20
#define TCP_Header 20
#define Data_Offset Ethernet_Header + IP_Header + TCP_Header

static uint32_t recv_buffer[MAX_RECV_CNT][RX_PKT_SIZE];
static int recv_length[MAX_RECV_CNT];
static int is_recv[MAX_RECV_CNT];

void send2recv(int id)
{
    static uint8_t mac[6]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static char res[] = "Response";
    memcpy(recv_buffer[id], mac, 6);
    memcpy((uint8_t*)recv_buffer[id] + Data_Offset , res, strlen(res));
    return;
}

void printpacket(int id , int mode)
{
    if (mode == 1)
    {
        sys_move_cursor(0,PRINT_LOCATION_SENDER);
        printf("> [Info]: Receive packet %d, length = %d\n", id , recv_length[id]);
    }
    else
    {
        sys_move_cursor(0,PRINT_LOCATION_RECEIVER);
        printf("> [Info]: Send packet %d, length = %d\n", id , recv_length[id]);
    }
    char *curr = (char *)recv_buffer[id];
    for (int j = 0; j < (recv_length[id] + 15) / 16; ++j) 
    {
        for (int k = 0; k < 16 && (j * 16 + k < recv_length[id]); ++k) 
        {
            printf("%02x ", (uint32_t)(*(uint8_t*)curr));
            ++curr;
        }
        printf("\n");
    }
}

void send_thread(void)
{
    int index = 0;
    while (1)
    {
        if (is_recv[index] == 1)
        {
            send2recv(index);
            printpacket(index,2);
            int ret = sys_net_send(recv_buffer + index , recv_length[index]);
            index = (index + 1) % MAX_RECV_CNT;
        }
    }
}

void recv_thread(void)
{
    int index = 0;
    while (1)
    {   
        int ret = sys_net_recv(recv_buffer + index, 1, recv_length + index);
        printpacket(index,1);
        is_recv[index] = 1;
        index = (index + 1) % MAX_RECV_CNT;
    }
}

int main(int argc, char *argv[])
{
    pthread_t send_tid, recv_tid;
    sys_move_cursor(0, 1);
    printf("> [Info] Start testing echo...");
    pthread_create(&send_tid, send_thread, NULL);
    pthread_create(&recv_tid, recv_thread, NULL);
    // note that we will never arrive here
    pthread_join(send_tid);
    pthread_join(recv_tid);
    return 0;
}
```