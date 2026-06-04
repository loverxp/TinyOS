# TinyOS 架构与主流程详解

## 系统启动流程

```
+-------------+     +-------------+     +-------------+     +-----------------+
|  QEMU/GRUB  | --> |  boot.asm   | --> | kernel_main | --> |  事件驱动主循环  |
|  (Bootloader)|    | (Multiboot) |     |   初始化...   |     |  halt 等待中断  |
+-------------+     +-------------+     +-------------+     +-----------------+
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
├─> 1. 初始化串口 (COM1, 用于调试输出)
│
├─> 2. 初始化 GDT
│   └─> gdt_init()
│       └─> 设置 6 个描述符：
│           ├─> Null 段 (GDT[0])
│           ├─> 内核代码段 (GDT[1], DPL=0)
│           ├─> 内核数据段 (GDT[2], DPL=0)
│           ├─> 用户代码段 (GDT[3], DPL=3)
│           ├─> 用户数据段 (GDT[4], DPL=3)
│           └─> TSS 段 (GDT[5], DPL=0)
│
├─> 3. 初始化 VGA 显示
    │   └─> vga_initialize()
    │       └─> 清屏、设置颜色、重置光标
    │   └─> vga_save_font()
    │       └─> 从 VGA plane 2 读取 4096 字节字模数据并保存到内核缓冲区
    │       └─> 用于 Mode 13h→文本模式切换时恢复被破坏的字模
│
├─> 4. 初始化物理内存管理器 (PMM)
│   └─> pmm_init(multiboot_info_addr)
│       └─> 从 GRUB 获取内存映射
│       └─> 初始化位图，标记已用/空闲页
│
├─> 5. 初始化堆分配器 (MM)
│   └─> mm_init()
│       └─> 从 PMM 分配内存作为堆空间
│       └─> 初始化块式分配器
│
├─> 6. 初始化分页
│   └─> paging_init()
│       └─> 分配页目录和 2 个页表
│       └─> Identity map 前 8MB 物理内存
│       └─> 设置 CR3 = 页目录基址
│       └─> 设置 CR0.PG 启用分页
│
├─> 7. 初始化 TSS
│   └─> tss_init(kernel_stack_top)
│       └─> 设置 ESP0 为专用内核栈顶
│       └─> 加载 TSS 到 GDT
│       └─> ltr 指令加载任务寄存器
│
├─> 7. 初始化中断描述符表 (IDT)
│   └─> idt_initialize()
│       ├─> 设置 32 个异常门 (DPL=0, flags=0x8E)
│       ├─> 设置 16 个 IRQ 门 (DPL=3, flags=0xEE)
│       └─> 设置 1 个系统调用门 (int 0x80, DPL=3, flags=0xEE)
│
├─> 8. 初始化可编程中断控制器 (PIC)
│   └─> pic_initialize()
│       └─> 重映射 PIC: 主 PIC -> 0x20, 从 PIC -> 0x28
│       └─> 默认屏蔽所有中断 (0xFF)
│
├─> 9. 初始化定时器
│   └─> timer_initialize(50)
│       └─> 设置 PIT 频率为 50Hz
│   └─> register_interrupt_handler(32, timer_handler)
│   └─> pic_unmask_irq(0)  ──> 启用 IRQ0 (定时器中断)
│   └─> timer_register_second_callback(on_timer_second)
│
├─> 10. 初始化键盘
│   └─> keyboard_initialize()
│       └─> 清空键盘缓冲区
│   └─> register_interrupt_handler(33, keyboard_handler)
│   └─> pic_unmask_irq(1)  ──> 启用 IRQ1 (键盘中断)
│
├─> 11. 初始化 Shell
│   └─> shell_init()
│       └─> keyboard_register_char_callback(shell_char_callback)
│       └─> 显示提示符 "TinyOS> "
│
├─> 12. 启用中断
│   └─> enable_interrupts()  // sti 指令
│
└─> 13. 进入事件驱动主循环
    └─> while(1) { halt(); }  // 等待中断
```

---

## 内核初始化顺序图

```
时序
│
├─ [1] 串口初始化         调试输出通道
├─ [2] GDT 初始化 ────────── 保护模式段管理
│      ├─ GDT[0]: Null               (必备)
│      ├─ GDT[1]: 内核代码段  DPL=0  (0x08)
│      ├─ GDT[2]: 内核数据段  DPL=0  (0x10)
│      ├─ GDT[3]: 用户代码段  DPL=3  (0x1B)
│      ├─ GDT[4]: 用户数据段  DPL=3  (0x23)
│      └─ GDT[5]: TSS 段     DPL=0  (0x28)
│
├─ [3] VGA 初始化          屏幕显示
├─ [4] PMM 初始化 ────────── 物理内存页帧分配
├─ [5] MM 初始化  ────────── kmalloc/kfree 堆分配器
├─ [6] 分页初始化 ────────── 页目录/页表, identity map 前 8MB
├─ [7] TSS 初始化 ────────── Ring 3→Ring 0 栈切换
├─ [8] IDT 初始化 ────────── 中断/异常/系统调用
├─ [9] PIC 初始化          中断控制器
├─[10] 定时器初始化         IRQ0, 50Hz
├─[11] 键盘初始化           IRQ1
├─[12] Shell 初始化         键盘回调绑定
├─[13] 启用中断             sti
│
└─[14] 主循环              halt() 等待中断
```

---

## 中断处理流程

### 中断/异常发生时

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
   ├─> pusha            ; 保存所有寄存器 (8×4=32字节)
   ├─> mov ax, ds       ; 保存数据段
   ├─> push eax
   ├─> mov ax, 0x10     ; 加载内核数据段
   ├─> mov ds, es, fs, gs
   │
   ├─> mov ebx, [esp+36]; 获取中断向量号
   ├─> sub ebx, 32      ; 转换为 IRQ 号 (0-15)
   ├─> mov eax, esp     ; 寄存器帧指针
   ├─> push eax         ; 压入 regs 指针
   ├─> push ebx         ; 压入 IRQ 号
   ├─> call irq_handler ; 调用 C 处理函数！
   ├─> add esp, 8       ; 清理参数
   │
   ├─> pop eax          ; 恢复数据段
   ├─> popa             ; 恢复所有寄存器
   ├─> add esp, 8       ; 清理错误码和向量号
   ├─> sti              ; 启用中断
   └─> iret             ; 中断返回

5. irq_handler(interrupts.c):
   ├─> 调用注册的 handler: isr_handlers[irq_no + 32]
   │   └─> 例如: keyboard_handler()
   └─> pic_send_eoi(irq_no)  ; 发送中断结束信号

6. keyboard_handler(drivers/keyboard.c):
   ├─> inb(0x60)        ; 读取键盘扫描码
   ├─> 解析扫描码 -> ASCII 字符
   └─> 触发字符回调函数
```

### 异常处理流程 (以用户态 GPF 为例)

```
用户态执行 hlt (特权指令)
  │
  ▼
CPU 触发 General Protection Fault (#GP, 向量 13)
  │
  ▼
CPU 自动切换到 TSS.ESP0 (内核栈)
  │
  ▼
CPU 压入 SS, ESP, EFLAGS, CS, EIP, Error Code
  │
  ▼
isr13 stub → isr_common_stub
  │
  ├─ pusha (保存所有寄存器)
  ├─ push ds
  ├─ 设置内核段寄存器
  ├─ push regs_ptr, err_code, int_no
  └─ call isr_handler
       │
       ▼
     isr_handler(int_no=13, err_code=0, regs)
       │
       ├─ exception_handler(13, 0)
       │   └─ 打印 "*** EXCEPTION: General Protection Fault ***"
       │
       ├─ 检查: int_no==13 && (regs[12] & 3)==3
       │        (CS.RPL=3 表示来自用户态)
       │   └─ regs[11] += 1  ← 跳过 hlt 指令 (1字节)
       │   └─ 打印 "Skipped faulty instruction"
       │
       └─ 返回 → 恢复寄存器 → iret
                     │
                     ▼
               回到用户态，执行 syscall 0
```

### 系统调用流程 (int 0x80)

```
用户态: mov eax, 0  ; syscall number
        int 0x80    ; 触发系统调用

CPU 切换到 TSS.ESP0 (内核栈)
  │
  ▼
isr128 stub (interrupts.asm):
  ├─ cli
  ├─ push 0          ; 虚拟错误码
  ├─ push 128        ; 向量号
  ├─ pusha           ; 保存寄存器
  ├─ push ds         ; 保存数据段
  ├─ 设置内核段
  ├─ push regs_ptr
  └─ call syscall_handler(regs)
       │
       ▼
     syscall_handler(regs)
       │
       ├─ regs[8] = EAX = syscall number
       │
       ├─ syscall 0: 返回内核态
       │   ├─ regs[11] = (uint32_t)user_exit_handler  ← EIP
       │   └─ regs[12] = 0x08                         ← CS (内核代码段)
       │
       └─ syscall 1: 打印消息
           └─ vga_writestring("[Syscall] #1 from user mode\n")
               
返回 → pop ds → popa → add esp,8 → iret
```

---

## 用户态切换流程

### 内核态 → 用户态

```
test_user_mode()  (kernel.c)
  │
  └─ run_user_task(user_main)  (user.asm)
       │
       ├─ 保存内核栈指针 [saved_kernel_esp]
       ├─ 加载用户数据段: DS/ES/FS/GS = 0x23
       │
       └─ 构建 IRET 帧并执行 iret:
            +------------------+
            | SS  = 0x23       |  ← 用户数据段 + RPL=3
            | ESP = user_stack |  ← 用户栈
            | EFLAGS           |
            |     IF=1, IOPL=3 |  ← 启用中断
            | CS  = 0x1B       |  ← 用户代码段 + RPL=3
            | EIP = user_main  |  ← 用户程序入口
            +------------------+
              │
              ▼
            CPU 执行 iret → 切换到 Ring 3
```

### 用户态 → 内核态 (syscall 0)

```
user_main (Ring 3)
  │
  └─ mov eax, 0
     int 0x80
       │
       ▼
     syscall_handler  (Ring 0, 通过 TSS.ESP0 切换栈)
       │
       ├─ regs[11] = user_exit_handler  (修改 EIP)
       └─ regs[12] = 0x08              (修改 CS = 内核段)
       
       iret → 同级切换 (RPL=0 == CPL=0)
         │
         ▼
       user_exit_handler  (Ring 0)
         ├─ mov ds/es/fs/gs = 0x10  (内核数据段)
         ├─ mov esp = [saved_kernel_esp]
         └─ ret → 回到 run_user_task 调用者
```

---

## 用户程序加载流程

### 嵌入式用户程序的加载与执行

```
runuser 命令
  │
  └─ run_loaded_user()  [kernel/loader.c]
       │
       ├─ 获取 incbin 嵌入的二进制: embedded_user_start ~ embedded_user_end
       │   └─ 定义于 kernel/embedded_user.asm (使用 incbin 指令)
       │
       ├─ memcpy(0x400000, embedded_user_start, size)  ← 复制到目标地址
       │
       ├─ pmm_alloc_page()  ← 分配 4KB 用户栈
       │
       ├─ user_esp = stack_page + 4096  ← 栈顶（栈向下增长）
       │
       └─ run_user_task_ex(entry=0x400000, user_esp)  [kernel/user.asm]
            │
            ├─ 保存内核栈指针
            ├─ 设置用户段寄存器 (DS/ES/FS/GS = 0x23)
            │
            └─ 构建 IRET 帧并执行 iret → Ring 3
                 │
                 ▼
               0x400000 (_start, crt0.s)  (Ring 3)
                 │
                 ├─ 清理 BSS 段
                 ├─ 调用 main()
                 │   └─ (用户程序逻辑，如 hello.c)
                 └─ 调用 exit() → syscall 0 → 返回内核态
                       │
                       ▼
                 user_exit_handler (Ring 0)
                       ├─ 恢复内核段寄存器
                       ├─ 恢复内核栈
                       ├─ pmm_free_page(user_stack) ← 释放用户栈
                       └─ ret → 回到 loader.c
```

### 用户程序构建流程

```
user/hello.c  +  user/crt0.s
       │
       ├─ i686-elf-gcc (编译为 .o)
       ├─ i686-elf-ld -T user.ld (链接为 ELF)
       └─ i686-elf-objcopy -O binary (转为纯二进制)
              │
              ▼
       build/user/hello.bin
              │
              ▼ (incbin 嵌入)
       kernel/embedded_user.asm
              │
              ▼ (编译)
       build/embedded_user_asm.o
              │
              ▼ (链接)
       tinyos.bin
```

---

## 贪吃蛇游戏流程

### 旧版：内核态 Snake（字符模式）

```
Shell 命令 "snake"
  │
  └─ shell_handle_command()
       │
       └─ snake_start(0)              [kernel/shell.c]
            │
            ├─ 初始化游戏状态 (蛇位置、方向、食物)
            ├─ outb(0x20, 0x20)      ← 发送 IRQ1 EOI！
            │   └─（解释同上）
            ├─ 注册键盘/定时器回调
            └─ while (running) { hlt; ... }
```

### 新版：gfxsnake（VGA Mode 13h 用户态用户程序）

```
Shell 命令 "gfxsnake"
  │
  └─ runuser → run_loaded_user()
       │
       └─ 加载 gfxsnake.bin 到 0x400000
          └─ iret → Ring 3 → _start → main()
               │
               ├─ sys_set_video_mode(1) → 切换到 VGA Mode 13h
               ├─ init_game() → 初始化蛇、食物
               ├─ render_frame() → 全屏渲染
               │
               └─ while (running) {
                    ├─ 轮询键盘 (sys_read_key)
                    ├─ 游戏逻辑 (计时器 tick 驱动)
                    └─ render_frame() (每 tick)
                  }
                    │
                    └─ sys_exit(0)
                         └─ 内核恢复文本模式
                            ├─ vga_set_mode03h()
                            │   ├─ 设置寄存器
                            │   ├─ 初始化 Attribute Controller 调色板
                            │   ├─ vga_restore_font() ← 恢复字模
                            │   └─ 清屏
                            └─ 返回 loader → Shell
```

### VGA Mode 13h 要点

- **分辨率**: 320×200, 256 色
- **帧缓冲**: 线性地址 `0xA0000`，64KB 窗口
- **Chain-4 模式**: 每 4 个像素字节交织到 VGA 的 4 个位平面
- **字模破坏**: 写入 `0xA0000` 时第 2、6、10… 字节写入 plane 2（即字模平面），破环字体数据
- **恢复方案**: 开机 `vga_save_font()` 读取 plane 2 保存字模，切换回文本模式时 `vga_restore_font()` 写回

### 关键设计要点

1. **PIC EOI**: `snake_start` 在中断处理链中运行，必须手动发送 IRQ1 EOI
2. **渲染分离**: `snake_tick`（中断上下文）只更新逻辑，主循环负责渲染
3. **双重渲染保护**: 使用 `needs_render` 标志避免中断中做大量 VGA 写入

### 为什么正常 Shell 不需要手动 EOI？

正常 Shell 操作时，`shell_char_callback` 处理完一个字符后**立即返回**，
IRQ 处理链正常结束，`irq_handler` 末尾的 `pic_send_eoi()` 会被执行。

但 `snake_start` 在回调中启动了一个**永不返回的游戏主循环**，劫持了整个
IRQ1 处理流程。`irq_handler` 永远到不了 `pic_send_eoi()`，PIC 的 IRQ1
In-Service 位始终置位，后续所有键盘中断被阻塞。

---

## 主循环流程

```c
while (1) {
    halt();  // 等待中断
}
```

**事件驱动模型：**
- 系统不主动轮询任何设备
- CPU 执行 `halt` 指令进入低功耗状态
- 中断发生时 CPU 被唤醒，执行对应的中断处理函数
- 中断返回后回到 `halt`，继续等待下一个中断

**中断事件处理：**
- **定时器中断 (IRQ0)**：每 20ms 触发一次，累计 ticks
  - 每秒触发 `on_timer_second()` 更新状态栏
- **键盘中断 (IRQ1)**：按键触发
  - 读取扫描码，转换为 ASCII
  - 触发注册的字符回调函数（Shell 回调）
- **系统调用 (int 0x80)**：用户态主动触发
  - 执行对应的系统调用功能

---

## GDT (全局描述符表)

### 布局

| 索引 | 选择子 | 段 | DPL | 类型 | 基址 | 大小 |
|------|--------|-----|-----|------|------|------|
| GDT[0] | 0x00 | Null 段 | - | - | 0 | 0 |
| GDT[1] | 0x08 | 内核代码段 | 0 | 代码, 可读, 非一致 | 0 | 4GB |
| GDT[2] | 0x10 | 内核数据段 | 0 | 数据, 可读写 | 0 | 4GB |
| GDT[3] | 0x1B | 用户代码段 | 3 | 代码, 可读, 非一致 | 0 | 4GB |
| GDT[4] | 0x23 | 用户数据段 | 3 | 数据, 可读写 | 0 | 4GB |
| GDT[5] | 0x28 | TSS 段 | 0 | TSS (可用) | TSS 地址 | sizeof(TSS) |

### 段描述符结构 (8字节)

```
  字节 0,1: 段限长低 16 位
  字节 2,3: 基址低 16 位
  字节 4:   基址 16-23 位
  字节 5:   标志位 (Type, S, DPL, P)
  字节 6:   标志位 (G, D/B, L, AVL) + 段限长高 4 位
  字节 7:   基址 24-31 位
```

### 选择子格式

```
Bit  15-3: 索引 (GDT 中的位置)
Bit  2:    TI (0=GDT, 1=LDT)
Bit  1-0:  RPL (请求特权级, 0=内核, 3=用户)

示例:
0x08 = 索引=1, TI=0, RPL=0  → GDT[1], 内核态
0x1B = 索引=3, TI=0, RPL=3  → GDT[3], 用户态
```

---

## TSS (任务状态段)

### 结构

```c
struct tss_entry {
    uint32_t prev_tss;   // 上一个 TSS 的链接
    uint32_t esp0;       // Ring 0 栈指针
    uint32_t ss0;        // Ring 0 栈段
    uint32_t esp1;       // Ring 1 栈指针 (未使用)
    uint32_t ss1;        // Ring 1 栈段
    uint32_t esp2;       // Ring 2 栈指针 (未使用)
    uint32_t ss2;        // Ring 2 栈段
    uint32_t cr3;        // 页目录基址
    uint32_t eip;
    // ... 其他寄存器保存字段 ...
    uint32_t iomap_base; // I/O 位图基址
};
```

### TSS 的关键作用

当 CPU 在较低特权级 (Ring 3) 运行时发生中断或异常，需要切换到较高特权级 (Ring 0) 处理：

1. CPU 从 **TSS** 读取 `SS0` 和 `ESP0`
2. 将当前 SS、ESP 压入新栈 (Ring 0 栈)
3. 将 EFLAGS、CS、EIP 压入新栈
4. 切换到 Ring 0 执行中断处理

**重要**：TSS.ESP0 必须指向专用内核栈，不能是当前正在使用的栈，否则会覆盖数据。

---

## 内存布局

```
0x00000000 ┌──────────────────────┐
           │  中断向量表 (实模式)   │
           │  + 页目录 + 页表       │  ← 分页用 (占用 ~12KB)
0x00000400 │  BIOS 数据区          │
0x00007C00 │  引导扇区             │
0x0009FC00 │  640KB 常规内存上限    │
0x000A0000 ├──────────────────────┤
           │  VGA 图形缓冲区       │
0x000B0000 │  VGA 文本缓冲区       │
           │  (0xB8000)           │
0x000C0000 │  VGA BIOS            │
0x000F0000 │  系统 BIOS            │
0x00100000 ├──────────────────────┤  ← 内核加载地址 (1MB)
           │  .text (代码段)       │
           │  .rodata (只读数据)   │
           │  .data (已初始化数据)  │
           │  .bss (未初始化数据)   │
0x00108000 ├──────────────────────┤  ← 栈顶 (1MB + 32KB)
           │                      │
           │  Identity Map 区域    │
           │  (0~8MB 物理内存      │
           │   映射到相同虚拟地址)  │
           │                      │
           │  用户程序 0x400000    │  ← 加载用户程序
           │                      │
0x00800000 └──────────────────────┘  ← 8MB identity map 上限
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
DPL = Descriptor Privilege Level (0 = 内核, 3 = 用户)
D   = 门类型 (1 = 32位中断门)

常见 flags 值:
0x8E = present, DPL=0, 32-bit interrupt gate  → 异常处理
0xEE = present, DPL=3, 32-bit interrupt gate  → IRQ 和系统调用
```

### PMM 位图结构

```
位图: 每一位代表一个 4KB 物理页
  bit=0: 空闲
  bit=1: 已分配

示例: 32MB 内存 = 8192 页 = 8192 位 = 1024 字节位图

分配: 在位图中查找第一个 bit=0 的位置, 标记为 1
释放: 将对应位标记为 0
```

### MM 堆块结构

```c
struct mm_block {
    uint32_t magic;      // 魔数 (0xDEADBEEF) 用于检测有效性
    uint32_t size;       // 块大小 (包括头部)
    uint32_t free;       // 1=空闲, 0=已分配
    struct mm_block* next; // 下一个块
    struct mm_block* prev; // 上一个块
    // 数据区紧随其后
};
```

---

## 文件依赖关系

```
boot.asm
    └─> kernel_main() [kernel.c]
        ├─> gdt_init()          [gdt.c]
        │   └─> gdt_asm.SET_GDT [gdt.asm]
        │       └─> lgdt
        │
        ├─> vga_initialize()    [vga.c]
        │   └─> outb()          [io.asm]
        │
        ├─> pmm_init()          [pmm.c]
        │   └─> 使用 GRUB 内存映射
        │
        ├─> mm_init()           [mm.c]
        │   └─> pmm_alloc_page() [pmm.c]
        │
        ├─> paging_init()       [paging.c]
        │   └─> pmm_alloc_page() [pmm.c]
        │
        ├─> tss_init()          [tss.c]
        │   └─> gdt_set_gate()  [gdt.c]
        │       └─> gdt_flush() [gdt.asm]
        │           └─> ltr
        │
        ├─> idt_initialize()    [interrupts.c]
        │   └─> idt_load()      [interrupts.asm]
        │       └─> lidt
        │
        ├─> pic_initialize()    [interrupts.c]
        │   └─> outb()          [io.asm]
        │
        ├─> timer_initialize()  [timer.c]
        │   └─> outb()          [io.asm]
        │
        ├─> keyboard_initialize() [keyboard.c]
        │   └─> inb()           [io.asm]
        │
        └─> shell_init()        [shell.c]
            └─> keyboard_register_char_callback() [keyboard.c]

中断/异常发生时:
    interrupts.asm (stub)
        │
        ├─ isr_handler()        [interrupts.c]
        │   └─ exception_handler() [except.c]
        │       └─ vga_writestring() [vga.c]
        │
        └─ irq_handler()        [interrupts.c]
            ├─ timer_handler()  [timer.c]
            ├─ keyboard_handler() [keyboard.c]
            │   └─ inb() / outb() [io.asm]
            └─ pic_send_eoi()   [interrupts.c]

系统调用 (int 0x80):
    user.asm (user_main, Ring 3)
        └─ int 0x80
            └─ isr128 stub [interrupts.asm]
                └─ syscall_handler() [interrupts.c]
                    ├─ vga_writestring() [vga.c]
                    └─ user_exit_handler() [user.asm]

用户态切换 (testuser):
    kernel.c (test_user_mode)
        └─ run_user_task() [user.asm]
            └─ iret → Ring 3
                └─ user_main
                    ├─ syscall 1 → 打印
                    ├─ hlt → GPF → 捕获并跳过
                    └─ syscall 0 → 返回 Ring 0
                        └─ user_exit_handler → 回到 kernel.c

用户程序加载 (runuser):
    shell.c (runuser 命令)
        └─ run_loaded_user() [loader.c]
            ├─ pmm_alloc_page() [pmm.c]     ← 分配用户栈
            ├─ memcpy() [string.c]
            ├─ printf() [stdio.c]            ← 输出加载信息
            └─ run_user_task_ex() [user.asm] ← 自定义栈入口
                └─ iret → Ring 3
                    └─ (USER_PROG_BASE = 0x400000)
                        └─ _start (crt0.s) → main() → exit()
                            └─ syscall 0 → 返回内核态
                                └─ user_exit_handler → loader.c
                                    └─ pmm_free_page() [pmm.c]
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
| 6 | Invalid Opcode | 无效操作码 |
| 8 | Double Fault | 双重故障 |
| 13 | General Protection Fault | 通用保护故障 |
| 14 | Page Fault | 页故障 |
| 32 | IRQ0 | 定时器 |
| 33 | IRQ1 | 键盘 |
| 128 (0x80) | Syscall | 系统调用 |

### 3. 寄存器查看

使用 QEMU 调试：`qemu-system-i386 -kernel build/tinyos.bin -d int,cpu_reset`

### 4. 异常错误码解析

| 错误码 | 含义 |
|--------|------|
| 0x00 | 非段相关违规（如特权指令） |
| 其他 | 段选择子索引（如 0x18=GDT[3]） |

### 5. 用户态调试要点

- 检查 CS 选择子是否正确设置了 RPL=3（0x1B 而非 0x18）
- 检查 TSS.ESP0 是否指向有效且未使用的内核栈
- 检查 IRQ 和系统调用的 IDT 门 DPL 是否为 3（0xEE）
- 检查用户态 EFLAGS.IF 是否为 1（启用中断）