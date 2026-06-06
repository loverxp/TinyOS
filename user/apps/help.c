/* help.c - User-space help command
 * Shows all available commands in the TinyOS shell.
 */

#include "../libc/stdio.h"

int main(void) {
    printf("Commands:\n");
    printf("  help       - Show this help\n");
    printf("  clear      - Clear screen\n");
    printf("  uptime     - Show system uptime\n");
    printf("  meminfo    - Show memory usage\n");
    printf("  alloc [N]  - Allocate N pages (default: 1)\n");
    printf("  free 0xADDR- Free a page by address\n");
    printf("  except     - Trigger Division By Zero\n");
    printf("  kmtest     - Run kmalloc/kfree test\n");
    printf("  echo <txt> - Echo text\n");
    printf("  testuser   - Switch to Ring 3 and return\n");
    printf("  runuser    - Load and run external user program\n");
    printf("  pageinfo   - Show page table info\n");
    printf("  snake      - Play Snake game (text mode)\n");
    printf("  gfxsnake   - Play Snake game (pixel graphics mode)\n");
    printf("  hello      - Run hello user program\n");
    printf("  schedtest [N]- Start scheduler test for N seconds\n");
    printf("  ipctest      - Run IPC test (Pipe, MQ, SharedMem)\n");
    printf("  gui        - Start graphical UI (VBE mode)\n");
    printf("  ls [path]  - List files/directories (default: root)\n");
    printf("  cat <file> - Print file contents\n");
    printf("  diskinfo   - Show disk/filesystem info\n");
    printf("  mkdir <dir> - Create directory\n");
    printf("  rmdir <dir> - Remove empty directory\n");
    printf("  pci        - List PCI devices\n");
    printf("  net        - Show network config\n");
    printf("  ping <ip|hostname>  - Send ICMP echo request\n");
    printf("  send <ip|hostname> <port> <msg> - Send UDP packet\n");
    printf("  recv <port> - Listen for UDP packets (5s)\n");
    printf("  tcp-recv <port> - Listen for TCP connections (10s)\n");
    printf("  dhcp       - Request IP via DHCP\n");
    printf("  arp        - Show ARP cache\n");
    printf("  arp -c     - Clear ARP cache\n");
    printf("  netstat    - Show network statistics\n");
    printf("  netstat -r - Reset network statistics\n");
    printf("  rand       - Show a random number (PRNG)\n");
    printf("  write <file> <text> - Write text to file (FAT16)\n");
    printf("  rm <file>  - Delete a file from disk\n");
    printf("  webserver  - Start HTTP server (port 80, hostfwd :8088)\n");
    printf("  webserver stop - Stop HTTP server\n");
    printf("  date       - Show current date/time (CMOS RTC)\n");
    return 0;
}