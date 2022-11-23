#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

#define E1000_RAL E1000_RA
#define E1000_RAH E1000_RA + 4
#define MAC_ADDR_LOW  0x00350a00
#define MAC_ADDR_HIGH 0x0000531e

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    local_flush_dcache();
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
    local_flush_dcache();
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    local_flush_dcache();
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
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048 & ~E1000_RCTL_BSEX);
    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
    // e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);
    local_flush_dcache();
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    local_flush_dcache();
    // find the next tx descriptor
    // FIXME: what if length > RX_PKT_SIZE?
    uint32_t index = e1000_read_reg(e1000, E1000_TDT);
    memcpy(tx_pkt_buffer[index], txpacket, length);
    tx_desc_array[index].length = length;
    tx_desc_array[index].cmd |= E1000_TXD_CMD_EOP;
    e1000_write_reg(e1000, E1000_TDT, (index + 1) % TXDESCS);
    local_flush_dcache();
    // wait for the packet to be sent
    while (!(tx_desc_array[index].status & E1000_TXD_STAT_DD)) ;
    return length;
}

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
    while ((rx_desc_array[index].status & (E1000_RXD_STAT_DD | E1000_RXD_STAT_EOP)) != (E1000_RXD_STAT_DD | E1000_RXD_STAT_EOP)) ;
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