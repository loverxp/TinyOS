#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"

/* Debug log levels */
#define KLOG_ERROR   0
#define KLOG_WARN    1
#define KLOG_INFO    2
#define KLOG_DEBUG   3

/* Compile-time minimum log level (messages below this are compiled out).
   Override by defining KLOG_LEVEL at compile time. */
#ifndef KLOG_LEVEL
#define KLOG_LEVEL KLOG_DEBUG
#endif

/* kprintf: prints to serial with log level prefix.
   Only outputs if level <= KLOG_LEVEL. */
void kprintf(int level, const char* module, const char* fmt, ...);

/* Convenience macros — usage: KINFO("NET", "initialized on port %u", port); */
#define KERROR(mod, fmt, ...)   do { if (KLOG_ERROR <= KLOG_LEVEL) kprintf(KLOG_ERROR, mod, fmt, ##__VA_ARGS__); } while(0)
#define KWARN(mod, fmt, ...)    do { if (KLOG_WARN  <= KLOG_LEVEL) kprintf(KLOG_WARN,  mod, fmt, ##__VA_ARGS__); } while(0)
#define KINFO(mod, fmt, ...)    do { if (KLOG_INFO  <= KLOG_LEVEL) kprintf(KLOG_INFO,  mod, fmt, ##__VA_ARGS__); } while(0)
#define KDBG(mod, fmt, ...)     do { if (KLOG_DEBUG <= KLOG_LEVEL) kprintf(KLOG_DEBUG, mod, fmt, ##__VA_ARGS__); } while(0)

/* Kernel backtrace: walk the EBP chain and print return addresses */
void kernel_backtrace(void);

#endif /* DEBUG_H */
