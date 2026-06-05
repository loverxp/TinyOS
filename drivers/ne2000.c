#include "../include/ne2000.h"
#include "../include/pci.h"
#include "../include/io.h"
#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/interrupts.h"

/* NE2000 state */
static uint16_t io_base = 0;
static uint8_t  mac_addr[6];
static uint8_t  rx_curr_page;  /* Current page pointer in receive ring */
static int      ne2k_ready = 0;
static ne2k_recv_callback_t recv_callback = NULL;

/* QEMU NE2000 PCI: vendor=0x10EC, device=0x8029 */
#define NE2K_VENDOR_ID  0x10EC
#define NE2K_DEVICE_ID  0x8029

/* Helper: read NE2000 register */
static inline uint8_t ne2k_in(uint8_t reg) {
    return inb(io_base + reg);
}

/* Helper: write NE2000 register */
static inline void ne2k_out(uint8_t reg, uint8_t val) {
    outb(io_base + reg, val);
}

/* Helper: read word from data port */
static inline uint16_t ne2k_inw_data(void) {
    return inw(io_base + NE2K_DATA);
}

/* Helper: write word to data port */
static inline void ne2k_outw_data(uint16_t val) {
    outw(io_base + NE2K_DATA, val);
}

/* Select page (0, 1, or 2) */
static void ne2k_set_page(uint8_t page) {
    uint8_t cmd = ne2k_in(NE2K_CMD);
    cmd = (cmd & 0x3F) | ((page & 0x03) << 6);
    ne2k_out(NE2K_CMD, cmd);
}

/* Remote DMA read: read 'len' bytes from NIC memory at 'addr' */
static void ne2k_dma_read(uint16_t addr, void* buf, uint16_t len) {
    uint8_t* dst = (uint8_t*)buf;

    /* Set remote DMA address */
    ne2k_out(NE2K_RSAR0, addr & 0xFF);
    ne2k_out(NE2K_RSAR1, (addr >> 8) & 0xFF);

    /* Set remote byte count */
    ne2k_out(NE2K_RBCR0, len & 0xFF);
    ne2k_out(NE2K_RBCR1, (len >> 8) & 0xFF);

    /* Start remote read: page 0, no DMA abort, remote read */
    ne2k_out(NE2K_CMD, NE2K_CR_RD0 | NE2K_CR_STA);

    /* Read words (NIC memory is 16-bit; each byte stored in a 16-bit word) */
    uint16_t words = (len + 1) / 2;
    for (uint16_t i = 0; i < words; i++) {
        uint16_t w = ne2k_inw_data();
        if (i * 2 < len) dst[i * 2] = w & 0xFF;
        if (i * 2 + 1 < len) dst[i * 2 + 1] = (w >> 8) & 0xFF;
    }

    /* Wait for RDC (Remote DMA Complete) */
    for (int i = 0; i < 10000; i++) {
        if (ne2k_in(NE2K_ISR) & NE2K_ISR_RDC) break;
    }
    ne2k_out(NE2K_ISR, NE2K_ISR_RDC);  /* Clear RDC */
}

/* Remote DMA write: write 'len' bytes to NIC memory at 'addr' */
static void ne2k_dma_write(uint16_t addr, const void* buf, uint16_t len) {
    const uint8_t* src = (const uint8_t*)buf;

    /* Set remote DMA address */
    ne2k_out(NE2K_RSAR0, addr & 0xFF);
    ne2k_out(NE2K_RSAR1, (addr >> 8) & 0xFF);

    /* Set remote byte count */
    ne2k_out(NE2K_RBCR0, len & 0xFF);
    ne2k_out(NE2K_RBCR1, (len >> 8) & 0xFF);

    /* Start remote write: page 0, no DMA abort, remote write */
    ne2k_out(NE2K_CMD, NE2K_CR_RD1 | NE2K_CR_STA);

    /* Write words */
    uint16_t words = (len + 1) / 2;
    for (uint16_t i = 0; i < words; i++) {
        uint16_t w = 0;
        if (i * 2 < len) w |= src[i * 2];
        if (i * 2 + 1 < len) w |= (uint16_t)src[i * 2 + 1] << 8;
        ne2k_outw_data(w);
    }

    /* Wait for RDC */
    for (int i = 0; i < 10000; i++) {
        if (ne2k_in(NE2K_ISR) & NE2K_ISR_RDC) break;
    }
    ne2k_out(NE2K_ISR, NE2K_ISR_RDC);
}

/* Process received packets from the ring buffer.
 *
 * NE2000 ring buffer stores consecutive bytes in 16-bit words.
 * With DCR bit3=1 (little-endian), byte 0 goes to low half, byte 1 to high half.
 * So the 4-byte logical header (RSR, NextPage, LenL, LenH) occupies 2 words:
 *   Word 0: [NextPage | RSR]   → read as: status=b[0], next_page=b[1]
 *   Word 1: [LenH | LenL]      → length = b[2] | (b[3] << 8)
 *
 * First packet starts at page BNDRY+1, not at BNDRY! */
static void ne2k_process_rx(void) {
    ne2k_set_page(1);
    uint8_t curr = ne2k_in(NE2K_CURR);
    ne2k_set_page(0);

    uint8_t bnry = ne2k_in(NE2K_BNDRY);

    serial_printf("[NE2K] process_rx: CURR=0x%02X BNDRY=0x%02X\n", curr, bnry);

    while (bnry != curr) {
        /* Packet starts at BNDRY+1 (page after boundary) */
        uint8_t page = bnry + 1;
        if (page >= NE2K_RX_STOP) page = NE2K_RX_START;
        if (page < NE2K_RX_START) page = NE2K_RX_START;

        /* If the next page to read equals CURR, the NIC hasn't written
         * any more data yet — bail out */
        if (page == curr) break;

        /* Read 4 bytes (2 words) — the logical receive header */
        ne2k_rx_header_t hdr;
        uint16_t hdr_addr = (uint16_t)page * NE2K_PAGE_SIZE;
        ne2k_dma_read(hdr_addr, &hdr, sizeof(hdr));

        serial_printf("[NE2K]   RX page=0x%02X: rsr=0x%02X next=0x%02X pkt_len=%u\n",
                      page, hdr.status, hdr.next_page, hdr.length);

        /* Validate: RSR must have PRX bit; length (incl. 4-byte hdr) must
         * be 64..ETH_MAX_FRAME+4; next must be within ring buffer range */
        if (!(hdr.status & 0x01) || hdr.length < 64 || hdr.length > ETH_MAX_FRAME + 4 ||
            hdr.next_page < NE2K_RX_START || hdr.next_page >= NE2K_RX_STOP) {
            serial_printf("[NE2K]   Bad packet header, stopping RX\n");
            break;
        }

        /* Read packet data after the 4-byte header.
         * NIC reports length including the 4-byte header, so subtract it. */
        uint8_t pkt_buf[ETH_MAX_FRAME];
        uint16_t data_len = hdr.length - 4;
        if (data_len > sizeof(pkt_buf)) data_len = sizeof(pkt_buf);

        uint16_t data_addr = hdr_addr + 4;
        if (data_addr + data_len > (uint16_t)NE2K_RX_STOP * NE2K_PAGE_SIZE) {
            /* Packet wraps around ring boundary */
            uint16_t first_part = (uint16_t)NE2K_RX_STOP * NE2K_PAGE_SIZE - data_addr;
            ne2k_dma_read(data_addr, pkt_buf, first_part);
            uint16_t wrap_addr = (uint16_t)NE2K_RX_START * NE2K_PAGE_SIZE;
            ne2k_dma_read(wrap_addr, pkt_buf + first_part, data_len - first_part);
        } else {
            ne2k_dma_read(data_addr, pkt_buf, data_len);
        }

        serial_printf("[NE2K]   Delivering packet: data_len=%u\n", data_len);
        if (recv_callback) {
            recv_callback(pkt_buf, data_len);
        }

        /* Update BNDRY to next-1 (mark pages as read) */
        bnry = hdr.next_page - 1;
        if (bnry < NE2K_RX_START) bnry = NE2K_RX_STOP - 1;
        ne2k_out(NE2K_BNDRY, bnry);

        /* Re-read CURR in case more packets arrived during processing */
        ne2k_set_page(1);
        curr = ne2k_in(NE2K_CURR);
        ne2k_set_page(0);
    }
}

/* IRQ handler — called from interrupt dispatcher */
void ne2000_handler(void) {
    if (!ne2k_ready) return;

    uint8_t isr = ne2k_in(NE2K_ISR);

    if (isr & NE2K_ISR_PRX) {
        ne2k_process_rx();
    }
    if (isr & NE2K_ISR_PTX) {
        /* Transmit complete — nothing to do for now */
    }
    if (isr & (NE2K_ISR_RXE | NE2K_ISR_TXE)) {
        serial_printf("[NE2K] Error: ISR=0x%02x\n", isr);
    }
    if (isr & NE2K_ISR_OVW) {
        serial_printf("[NE2K] Ring overflow!\n");
    }

    /* Clear all pending interrupts */
    ne2k_out(NE2K_ISR, isr);
}

int ne2000_init(void) {
    /* Find NE2000 PCI device */
    const pci_device_t* dev = pci_find_device(NE2K_VENDOR_ID, NE2K_DEVICE_ID);
    if (!dev) {
        serial_printf("[NE2K] No NE2000 PCI device found\n");
        return -1;
    }

    serial_printf("[NE2K] Found PCI device at bus=%u dev=%u func=%u\n",
                  dev->bus, dev->device, dev->func);
    serial_printf("[NE2K] Vendor=0x%04X Device=0x%04X Class=0x%02X Subclass=0x%02X\n",
                  dev->vendor_id, dev->device_id, dev->class_code, dev->subclass);

    /* Step 1: Read BAR0 */
    uint32_t bar0_raw = dev->bar[0];
    serial_printf("[NE2K] BAR0 raw = 0x%08X\n", bar0_raw);

    /* Get I/O base from BAR0 (mask out I/O space indicator bit) */
    io_base = bar0_raw & 0xFFFC;
    serial_printf("[NE2K] I/O base = 0x%04x (from BAR0), IRQ = %u\n", io_base, dev->irq_line);

    /* Step 2: Enable I/O space in PCI Command Register (offset 0x04, bit 0).
     * PCI spec: after reset, I/O and memory decoding are disabled by default.
     * Without this, all I/O port writes/reads to the NIC are silently dropped. */
    uint32_t pci_cmd = pci_read_config(dev->bus, dev->device, dev->func, 0x04);
    serial_printf("[NE2K] PCI Command Register before = 0x%08X\n", pci_cmd);
    pci_cmd |= 0x00000001;  /* Set bit 0: I/O Space Enable */
    pci_write_config(dev->bus, dev->device, dev->func, 0x04, pci_cmd);
    pci_cmd = pci_read_config(dev->bus, dev->device, dev->func, 0x04);
    serial_printf("[NE2K] PCI Command Register after  = 0x%08X (bit0=1 means I/O enabled)\n", pci_cmd);

    /* Step 3: Reset the NIC */
    serial_printf("[NE2K] Resetting NIC...\n");
    uint8_t reset_val = inb(io_base + NE2K_RESET);
    outb(io_base + NE2K_RESET, reset_val);

    /* Wait for reset to complete */
    int reset_ok = 0;
    for (int i = 0; i < 100000; i++) {
        if (ne2k_in(NE2K_ISR) & NE2K_ISR_RST) {
            reset_ok = 1;
            break;
        }
    }
    serial_printf("[NE2K] Reset %s (ISR after reset = 0x%02X)\n",
                  reset_ok ? "OK" : "TIMEOUT", ne2k_in(NE2K_ISR));

    /* Stop the NIC */
    ne2k_out(NE2K_CMD, NE2K_CR_STP | NE2K_CR_RD2);  /* Stop + abort DMA */
    serial_printf("[NE2K] Stopped NIC\n");

    /* Data configuration: word-wide DMA, little-endian */
    ne2k_out(NE2K_DCR, 0x49);
    serial_printf("[NE2K] DCR set to 0x49 (word-wide DMA, little-endian)\n");

    /* Clear remote byte count */
    ne2k_out(NE2K_RBCR0, 0);
    ne2k_out(NE2K_RBCR1, 0);

    /* Transmit config: normal operation */
    ne2k_out(NE2K_TCR, 0x00);

    /* Receive config: AB (accept broadcast) + PRO (promiscuous for simplicity) */
    ne2k_out(NE2K_RCR, 0x14);  /* PRO=bit4, AB=bit2 */

    /* Set page boundaries */
    ne2k_out(NE2K_TPSR, NE2K_TX_PAGE);
    ne2k_out(NE2K_STARTPG, NE2K_RX_START);
    ne2k_out(NE2K_STOPPG, NE2K_RX_STOP);
    ne2k_out(NE2K_BNDRY, NE2K_RX_START);
    serial_printf("[NE2K] Pages: TX=0x%02X RX=0x%02X-0x%02X BNDRY=0x%02X\n",
                  NE2K_TX_PAGE, NE2K_RX_START, NE2K_RX_STOP, NE2K_RX_START);

    rx_curr_page = NE2K_RX_START + 1;

    /* Clear ISR */
    ne2k_out(NE2K_ISR, 0xFF);
    serial_printf("[NE2K] ISR cleared\n");

    /* Read MAC address from NIC memory (stored at page 0x0000, word-packed) */
    uint8_t mac_raw[12];
    ne2k_dma_read(0x0000, mac_raw, 12);
    serial_printf("[NE2K] MAC raw bytes:");
    for (int i = 0; i < 12; i++) {
        serial_printf(" %02x", mac_raw[i]);
    }
    serial_printf("\n");
    for (int i = 0; i < 6; i++) {
        mac_addr[i] = mac_raw[i * 2];  /* Each byte is stored in a 16-bit word */
    }

    serial_printf("[NE2K] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);

    /* Set physical address in page 1 */
    ne2k_set_page(1);
    for (int i = 0; i < 6; i++) {
        ne2k_out(NE2K_PAR0 + i, mac_addr[i]);
    }
    /* Set multicast to accept nothing */
    for (int i = 0; i < 8; i++) {
        ne2k_out(NE2K_MAR0 + i, 0x00);
    }
    /* Set current page pointer */
    ne2k_out(NE2K_CURR, rx_curr_page);
    ne2k_set_page(0);
    serial_printf("[NE2K] Physical address set\n");

    /* Enable interrupts: PRX + PTX + errors */
    ne2k_out(NE2K_IMR, NE2K_ISR_PRX | NE2K_ISR_PTX |
                       NE2K_ISR_RXE | NE2K_ISR_TXE | NE2K_ISR_OVW);

    /* Start the NIC */
    ne2k_out(NE2K_CMD, NE2K_CR_STA | NE2K_CR_RD2);  /* Start + abort DMA */

    /* Read back status after start */
    uint8_t cmd_status = ne2k_in(NE2K_CMD);
    serial_printf("[NE2K] CMD register after start = 0x%02X\n", cmd_status);

    /* Enable normal transmit */
    ne2k_out(NE2K_TCR, 0x00);

    ne2k_ready = 1;
    serial_printf("[NE2K] Initialized and running\n");
    return 0;
}

int ne2000_send(const void* data, uint16_t len) {
    if (!ne2k_ready || len > ETH_MAX_FRAME) return -1;

    uint16_t orig_len = len;
    /* Pad to minimum Ethernet frame size (60 bytes) */
    if (len < 60) len = 60;

    /* Debug: dump first 16 bytes of frame */
    const uint8_t* d = (const uint8_t*)data;
    serial_printf("[NE2K] Send: len=%u (padded=%u) dst=%02x:%02x:%02x:%02x:%02x:%02x ethertype=0x%04x\n",
                  orig_len, len,
                  d[0], d[1], d[2], d[3], d[4], d[5],
                  (uint16_t)d[12] << 8 | d[13]);
    serial_printf("[NE2K]   SRC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  d[6], d[7], d[8], d[9], d[10], d[11]);

    /* Write data to NIC transmit buffer */
    uint16_t tx_addr = (uint16_t)NE2K_TX_PAGE * NE2K_PAGE_SIZE;
    ne2k_dma_write(tx_addr, data, len);

    /* Set transmit page start */
    ne2k_out(NE2K_TPSR, NE2K_TX_PAGE);

    /* Set transmit byte count */
    ne2k_out(NE2K_TBCR0, len & 0xFF);
    ne2k_out(NE2K_TBCR1, (len >> 8) & 0xFF);

    /* Trigger transmit */
    ne2k_out(NE2K_CMD, NE2K_CR_STA | NE2K_CR_TXP | NE2K_CR_RD2);

    return 0;
}

int ne2000_is_ready(void) {
    return ne2k_ready;
}

const uint8_t* ne2000_get_mac(void) {
    return mac_addr;
}

void ne2000_set_recv_callback(ne2k_recv_callback_t cb) {
    recv_callback = cb;
}

/* Poll the receive ring buffer without relying on IRQ.
 * Call this after sending a packet if you need to wait for a reply
 * (e.g. ARP resolution). */
void ne2000_poll_recv(void) {
    if (!ne2k_ready) return;

    /* Check ISR for PRX bit first (fast path) */
    uint8_t isr = ne2k_in(NE2K_ISR);
    serial_printf("[NE2K] poll_recv: ISR=0x%02X\n", isr);
    if (isr & NE2K_ISR_PRX) {
        serial_printf("[NE2K] poll_recv: PRX set, processing RX\n");
        ne2k_process_rx();
        ne2k_out(NE2K_ISR, NE2K_ISR_PRX);  /* Clear PRX */
        return;
    }

    /* Also check CURR vs BNDRY directly (packet may have arrived
     * but ISR PRX not yet set in some QEMU versions) */
    ne2k_set_page(1);
    uint8_t curr = ne2k_in(NE2K_CURR);
    ne2k_set_page(0);
    uint8_t bnry = ne2k_in(NE2K_BNDRY);

    if (curr != bnry) {
        serial_printf("[NE2K] poll_recv: CURR=0x%02X != BNDRY=0x%02X, processing\n", curr, bnry);
        ne2k_process_rx();
        /* Clear any PRX that may have been set during processing */
        ne2k_out(NE2K_ISR, NE2K_ISR_PRX);
    }
}
