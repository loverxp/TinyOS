# TinyOS 架构与主流程详解

## 系统启动流程

```
+-------------+     +-------------+     +-------------+     +-------------+
|  QEMU/GRUB  | --> |  boot.asm   | --> |  kernel.c   | --> |  Main Loop  |
|  (Bootloader)|     | (Multiboot) |     | kernel_main |     |  (Running)  |
+-------------+     +-------------+     +-------------+     +-------------+
```

### 第一阶段：引导加载 (boot/boot.asm)

```
1. QEMU/GRUB 加载内核到内存 0x100000 (1MB)
2. 检查 Multiboot 魔数 0x1BADB002
3. 设置栈指针 ESP = 0x108000
4. 调用 kernel_main()
5. 如果 kernel_main 返回，执行 cli; hlt（停机）
```

**关键代码：**
```nasm
; Multiboot 头
MAGIC    equ  0x1BADB002
FLAGS    equ  MBALIGN | MEMINFO
CHECKSUM equ -(MAGIC + FLAGS)

start:
    mov esp, stack_top      ; 设置栈
    call kernel_main        ; 进入内核
    cli                     ; 禁用中断
.hang:
    hlt                     ; 停机
    jmp .hang
```

---

### 第二阶段：内核初始化 (kernel/kernel.c)

```
kernel_main()
│
├─> 1. 初始化串口 (用于调试输出)
│
├─> 2. 初始化 VGA 显示
│   └─> vga_initialize()
│       └─> 清屏、设置颜色、重置光标
│
├─> 3. 初始化中断描述符表 (IDT)
│   └─> idt_initialize()
│       └─> 设置 256 个中断门
│           ├─> ISR 0-31: CPU 异常处理
│           └─> IRQ 0-15: 硬件中断 (映射到向量 32-47)
│
├─> 4. 初始化可编程中断控制器 (PIC)
│   └─> pic_initialize()
│       └─> 重映射 PIC: 主 PIC -> 0x20, 从 PIC -> 0x28
│       └─> 默认屏蔽所有中断
│
├─> 5. 初始化定时器
│   └─> timer_initialize(50)
│       └─> 设置 PIT 频率为 50Hz
│   └─> register_interrupt_handler(32, timer_handler)
│       └─> 注册定时器中断处理函数
│   └─> pic_unmask_irq(0)
│       └─> 启用 IRQ0 (定时器中断)
│
├─> 6. 初始化键盘
│   └─> keyboard_initialize()
│       └─> 清空键盘缓冲区
│   └─> register_interrupt_handler(33, keyboard_handler)
│       └─> 注册键盘中断处理函数
│   └─> pic_unmask_irq(1)
│       └─> 启用 IRQ1 (键盘中断)
│
├─> 7. 启用中断
│   └─> enable_interrupts()  // sti 指令
│
└─> 8. 进入主循环
    └─> while(1) { ... }
```

---

## 中断处理流程

### 中断发生时

```
+-------------+     +------------------+     +------------------+
|  硬件中断    | --> |  汇编 Stub       | --> |  C 处理函数      |
| (如按键)    |     | (interrupts.asm) |     | (interrupts.c)   |
+-------------+     +------------------+     +------------------+
```

**详细流程：**

```
1. 键盘按下 -> 触发 IRQ1

2. CPU 查找 IDT 中向量 33 的门描述符
   └─> 跳转到 irq1 汇编 stub

3. irq1 stub (interrupts.asm):
   ├─> cli              ; 禁用中断（防止嵌套）
   ├─> push 0           ; 压入错误码（虚拟）
   ├─> push 33          ; 压入中断向量号
   └─> jmp irq_common_stub

4. irq_common_stub:
   ├─> pusha            ; 保存所有寄存器
   ├─> mov ax, ds       ; 保存数据段
   ├─> push eax
   ├─> mov ax, 0x10     ; 加载内核数据段
   ├─> mov ds, ax       ; 设置 ds, es, fs, gs
   │
   ├─> mov ebx, [esp+36]; 获取中断向量号
   ├─> sub ebx, 32      ; 转换为 IRQ 号 (0-15)
   ├─> push ebx         ; 压入参数
   ├─> call irq_handler ; 调用 C 处理函数！
   ├─> add esp, 4       ; 清理参数
   │
   ├─> pop eax          ; 恢复数据段
   ├─> mov ds, ax
   ├─> popa             ; 恢复所有寄存器
   ├─> add esp, 8       ; 清理错误码和向量号
   ├─> sti              ; 启用中断
   └─> iret             ; 中断返回

5. irq_handler(interrupts.c):
   ├─> 获取 irq_no 参数
   ├─> 调用注册的 handler: isr_handlers[irq_no + 32]
   │   └─> 例如: keyboard_handler()
   └─> pic_send_eoi(irq_no)  ; 发送中断结束信号

6. keyboard_handler(drivers/keyboard.c):
   ├─> inb(0x60)        ; 读取键盘扫描码
   ├─> 解析扫描码 -> ASCII 字符
   └─> 存入键盘缓冲区
```

---

## 主循环流程

```c
while (1) {
    // 1. 检查定时器
    uint32_t current_ticks = timer_get_ticks();
    if (current_ticks != last_ticks) {
        // 每秒输出一次调试信息
    }
    
    // 2. 检查键盘输入
    if (keyboard_has_input()) {
        char c = keyboard_read_char();
        // 显示字符到屏幕
        vga_putchar(c);
    }
}
```

**注意：** 这是一个轮询循环，不是多任务系统。真正的操作系统会使用进程调度。

---

## 内存布局

```
地址范围              内容
+------------------+
| 0x00000000       |  中断向量表 (IVT) - 实模式
| 0x00000400       |  BIOS 数据区
| 0x00007C00       |  引导扇区加载地址
| ...              |
| 0x0009FC00       |  640KB 常规内存上限
+------------------+
| 0x000A0000       |  VGA 图形缓冲区
| 0x000B0000       |  VGA 文本缓冲区 (0xB8000)
| 0x000C0000       |  VGA BIOS
| 0x000F0000       |  系统 BIOS
+------------------+
| 0x00100000       |  <-- 内核加载地址 (1MB)
|                  |  .text (代码段)
|                  |  .rodata (只读数据)
|                  |  .data (已初始化数据)
|                  |  .bss (未初始化数据)
| 0x00108000       |  <-- 栈顶 (1MB + 32KB)
+------------------+
```

---

## 关键数据结构

### IDT 门描述符 (8字节)

```
 31                  16 15 14 13 12 11 10 9 8 7             0
+--------------------+--+-----+--+-+-+-+-+-+------------------+
| 基地址高16位        | P| DPL |0 |D|1|1|0|0| 基地址低16位      |
+--------------------+--+-----+--+-+-+-+-+-+------------------+

P   = Present (1 = 有效)
DPL = Descriptor Privilege Level (0 = 内核)
D   = 门类型 (1 = 32位)
```

### PIC 初始化流程 (ICW)

```
ICW1: 0x11  -> 开始初始化，级联模式，需要 ICW4
ICW2: 0x20  -> 主 PIC 向量偏移 (32)
      0x28  -> 从 PIC 向量偏移 (40)
ICW3: 0x04  -> 主 PIC: IRQ2 连接从 PIC
      0x02  -> 从 PIC: 连接到主 PIC 的 IRQ2
ICW4: 0x01  -> 8086 模式
OCW1: 0xFF  -> 屏蔽所有中断（初始化时）
      0x00  -> 取消屏蔽（运行时）
```

---

## 文件依赖关系

```
boot.asm
    └─> kernel_main() [kernel.c]
        ├─> vga_initialize() [vga.c]
        ├─> idt_initialize() [interrupts.c]
        │   └─> idt_load() [interrupts.asm]
        ├─> pic_initialize() [interrupts.c]
        ├─> timer_initialize() [timer.c]
        │   └─> outb() [io.asm]
        ├─> register_interrupt_handler() [interrupts.c]
        ├─> keyboard_initialize() [keyboard.c]
        │   └─> inb() [io.asm]
        ├─> pic_unmask_irq() [interrupts.c]
        └─> enable_interrupts() [io.h - inline]

中断发生时:
    interrupts.asm (irq1 stub)
        └─> irq_handler() [interrupts.c]
            ├─> keyboard_handler() [keyboard.c]
            │   ├─> inb() [io.asm]
            │   └─> outb() [io.asm] (serial debug)
            └─> pic_send_eoi() [interrupts.c]
```

---

## 调试技巧

### 1. 串口输出调试

```c
// 写入 COM1 串口
static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);  // 等待发送就绪
    outb(0x3F8, c);                     // 发送字符
}
```

启动 QEMU 时添加 `-serial stdio` 即可在终端看到输出。

### 2. 常见中断向量

| 向量 | 名称 | 说明 |
|------|------|------|
| 0 | Divide Error | 除零错误 |
| 8 | Double Fault | 双重故障 |
| 13 | General Protection Fault | 通用保护故障 |
| 14 | Page Fault | 页故障 |
| 32 | IRQ0 | 定时器 |
| 33 | IRQ1 | 键盘 |
| 46 | IRQ14 | 主 IDE |

### 3. 寄存器查看

使用 QEMU 调试：`qemu-system-i386 -kernel tinyos.bin -d int,cpu_reset`
