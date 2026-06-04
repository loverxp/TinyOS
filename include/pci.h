#ifndef PCI_H
#define PCI_H

#include "types.h"

/* PCI configuration space I/O ports */
#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

/* PCI device info (from config space scan) */
typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  irq_line;
    uint32_t bar[6];        /* Base Address Registers */
} pci_device_t;

#define PCI_MAX_DEVICES 32

/* API */
void pci_scan(void);
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value);
const pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id);
int pci_get_device_count(void);
const pci_device_t* pci_get_device(int index);
void pci_list_devices(void);

#endif /* PCI_H */
