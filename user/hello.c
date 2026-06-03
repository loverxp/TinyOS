/* hello.c - Sample user program */
/* Programmed with only syscalls, no stdlib dependence */

/* Syscall wrappers */
static void sys_exit(int code) {
    (void)code;  /* code unused in raw syscall */
    asm volatile("mov $0, %%eax; int $0x80" : : : "eax", "memory");
}

static void sys_print(void) {
    asm volatile("mov $1, %%eax; int $0x80" : : : "eax", "memory");
}

/* Simple print with loop - avoids stdlib */
static void delay(void) {
    volatile int i;
    for (i = 0; i < 500000; i++);
}

int main(void) {
    sys_print();
    delay();
    sys_print();
    delay();
    sys_exit(0);
    return 0;
}