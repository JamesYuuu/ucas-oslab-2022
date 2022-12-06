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