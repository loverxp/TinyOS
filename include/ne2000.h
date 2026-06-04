#ifndef NE2000_H
#define NE2000_H

#include "types.h"

/* NE2000 register offsets (relative to I/O base) */
#define NE2K_CMD        0x00    /* Command register */
#define NE2K_STARTPG    0x01    /* Start page (write) */
#define NE2K_STOPPG     0x02    /* Stop page (write) */
#define NE2K_BNDRY      0x03    /* Boundary pointer */
#define NE2K_TSR        0x04    /* Transmit status (read) */
#define NE2K_TPSR       0x04    /* Transmit page start (write) */
#define NE2K_NCR        0x05    /* Collision count (read) */
#define NE2K_TBCR0      0x05    /* Transmit byte count low (write) */
#define NE2K_FIFO       0x06    /* FIFO (read) */
#define NE2K_TBCR1      0x06    /* Transmit byte count high (write) */
#define NE2K_ISR        0x07    /* Interrupt status */
#define NE2K_CRDA0      0x08    /* Current remote DMA addr low (read) */
#define NE2K_RSAR0      0x08    /* Remote start addr low (write) */
#define NE2K_CRDA1      0x09    /* Current remote DMA addr high (read) */
#define NE2K_RSAR1      0x09    /* Remote start addr high (write) */
#define NE2K_RBCR0      0x0A    /* Remote byte count low (write) */
#define NE2K_RBCR1      0x0B    /* Remote byte count high (write) */
#define NE2K_RSR        0x0C    /* Receive status (read) */
#define NE2K_RCR        0x0C    /* Receive config (write) */
#define NE2K_CNTR0      0x0D    /* Tally counter 0 (read) */
#define NE2K_TCR        0x0D    /* Transmit config (write) */
#define NE2K_DCR        0x0E    /* Data config (write) */
#define NE2K_IMR        0x0F    /* Interrupt mask (write) */
#define NE2K_DATA       0x10    /* Data port (DMA) */
#define NE2K_RESET      0x1F    /* Reset port */

/* Page 1 registers (selected via CMD register) */
#define NE2K_PAR0       0x01    /* Physical address reg 0 */
#define NE2K_MAR0       0x08    /* Multicast address reg 0 */
#define NE2K_CURR       0x07    /* Current page (receive ring) */

/* Command register bits */
#define NE2K_CR_STP     0x01    /* Stop */
#define NE2K_CR_STA     0x02    /* Start */
#define NE2K_CR_TXP     0x04    /* Transmit packet */
#define NE2K_CR_RD0     0x08    /* Remote DMA command bit 0 */
#define NE2K_CR_RD1     0x10    /* Remote DMA command bit 1 */
#define NE2K_CR_RD2     0x20    /* Remote DMA command bit 2 */
#define NE2K_CR_PS0     0x40    /* Page select bit 0 */
#define NE2K_CR_PS1     0x80    /* Page select bit 1 */

/* ISR bits */
#define NE2K_ISR_PRX    0x01    /* Packet received */
#define NE2K_ISR_PTX    0x02    /* Packet transmitted */
#define NE2K_ISR_RXE    0x04    /* Receive error */
#define NE2K_ISR_TXE    0x08    /* Transmit error */
#define NE2K_ISR_OVW    0x10    /* Overwrite warning */
#define NE2K_ISR_CNT    0x20    /* Counter overflow */
#define NE2K_ISR_RDC    0x40    /* Remote DMA complete */
#define NE2K_ISR_RST    0x80    /* Reset status */

/* Memory layout (NE2000 16-bit: 32 pages of 256 bytes each) */
#define NE2K_PAGE_SIZE  256
#define NE2K_TX_PAGE    0x40    /* Transmit buffer starts at page 0x40 */
#define NE2K_TX_PAGES   6       /* Use 6 pages (1536 bytes) for TX */
#define NE2K_RX_START   0x46    /* Receive ring starts after TX buffer */
#define NE2K_RX_STOP    0x80    /* Receive ring ends at page 0x80 (32KB NIC) */

/* Ethernet constants */
#define ETH_HEADER_SIZE 14
#define ETH_MAX_FRAME   1518

/* MAC address */
#define MAC_ADDR_SIZE   6

/* Receive ring header (prepended to each received packet in NIC memory) */
typedef struct {
    uint8_t  status;
    uint8_t  next_page;
    uint16_t length;    /* Total length including this header */
} __attribute__((packed)) ne2k_rx_header_t;

/* API */
int ne2000_init(void);
int ne2000_send(const void* data, uint16_t len);
void ne2000_handler(void);
int ne2000_is_ready(void);
const uint8_t* ne2000_get_mac(void);

/* Callback for received packets */
typedef void (*ne2k_recv_callback_t)(const uint8_t* data, uint16_t len);
void ne2000_set_recv_callback(ne2k_recv_callback_t cb);

#endif /* NE2000_H */
