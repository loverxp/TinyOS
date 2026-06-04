# TinyOS - 简单操作系统

一个基于 C 语言的简单操作系统内核，可在 QEMU 模拟器上运行。

## 快速开始

### 1. 直接运行（已编译好）

双击 **`scripts\run.bat`** 即可启动 QEMU 运行操作系统。

### 2. 重新构建并运行

双击 **`scripts\build_simple.bat`** 或运行 `make` 会自动编译并启动。

## 键盘输入使用说明

**重要：必须先在 QEMU 窗口内点击一下，才能接收键盘输入！**

1. 双击 `scripts\run.bat` 启动 QEMU
2. 看到蓝色界面的 TinyOS 启动后，**用鼠标点击一下 QEMU 窗口内部**
3. 此时光标应该闪烁，直接打字即可
4. **Enter** 换行，**Backspace** 删除
5. **Ctrl+Alt** 释放鼠标（从 QEMU 窗口切出）

### 如果键盘仍然无法输入

尝试以下方法：

1. **确保点击了 QEMU 窗口内部**（标题栏不算）
2. 尝试按 **Ctrl+Alt+G** 切换鼠标/键盘捕获
3. 检查是否开启了中文输入法（建议切换到英文输入法）
4. 尝试在命令行手动运行：
   ```batch
   "D:\Program Files\qemu\qemu-system-i386.exe" -kernel build\tinyos.bin -m 32
   ```

## Shell 命令

进入系统后，可直接在 `TinyOS>` 提示符下输入命令：

| 命令            | 说明                      |
| ------------- | ----------------------- |
| `help`        | 显示所有可用命令                |
| `clear`       | 清屏                      |
| `uptime`      | 显示系统运行时间                |
| `meminfo`     | 显示物理内存信息                |
| `alloc <N>`   | 分配 N 个物理页（默认 1）         |
| `free 0xADDR` | 释放指定地址的物理页              |
| `kmtest`      | 运行 kmalloc/kfree 堆分配器测试 |
| `except`      | 触发除零异常测试异常处理            |
| `echo <文本>`   | 回显输入的文本                 |
| `testuser`    | 切换到 Ring 3（用户态）并返回      |
| `runuser`     | 加载并执行嵌入式用户程序           |
| `gfxsnake`    | VGA 图形模式贪吃蛇游戏（像素模式） |
| `pageinfo`    | 显示页表信息                    |
| `hello`       | 运行 Ring 3 示例用户程序（使用 libc） |

## 功能特性

- ✅ Multiboot 兼容引导
- ✅ 32位 x86 保护模式
  - ✅ GDT（全局描述符表）—— 内核段 + 用户段 + TSS
  - ✅ 用户态（Ring 3）/ 内核态（Ring 0）切换
  - ✅ TSS（任务状态段）—— 特权级切换栈管理
- ✅ VGA 文本显示 (80x25, 16色)
- ✅ VGA 图形模式 (Mode 13h, 320×200, 256 色)
- ✅ VGA 字模保存与恢复（图形↔文本模式切换）
- ✅ VGA 状态栏（系统运行时间、堆使用量、空闲内存）
- ✅ 硬件光标
- ✅ 键盘输入（中断驱动，事件回调）
- ✅ 定时器中断（中断驱动，事件回调）
- ✅ 中断描述符表 (IDT)
  - ✅ 异常处理（除零、GPF 等）
  - ✅ 用户态异常捕获与指令跳过
- ✅ 物理内存管理（PMM，位图式页帧分配器）
- ✅ 分页机制（页目录/页表，identity map 前 8MB）
- ✅ 堆内存分配器（kmalloc/kfree，块式管理）
- ✅ printf/sprintf 格式化输出（%s, %d, %u, %x, %c, %p）
- ✅ 用户态标准库 libc（printf, sprintf, exit, 字符串函数）
- ✅ 交互式 Shell（多命令支持）
- ✅ 系统调用（int 0x80，支持从用户态返回内核态）
- ✅ 串口调试输出

## 项目结构

```
TinyOS/
├── boot/boot.asm           # Multiboot 引导程序
├── kernel/
│   ├── kernel.c           # 内核主程序
│   ├── shell.c            # 交互式 Shell
│   ├── gdt.c              # GDT 初始化（C 部分）
│   ├── tss.c              # TSS 初始化
│   ├── pmm.c              # 物理内存管理器（位图分配）
│   ├── mm.c               # 堆内存分配器（kmalloc/kfree）
│   ├── paging.c           # 分页机制（页目录/页表）
│   ├── except.c           # 异常处理（含 Page Fault 详细诊断）
│   ├── loader.c           # 用户程序加载器
│   ├── embedded_user.asm  # 嵌入的用户程序二进制
│   └── user.asm           # 用户态入口和切换逻辑
├── user/
│   ├── crt0.s             # 用户程序启动代码
│   ├── hello.c            # 示例用户程序（使用 libc）
│   ├── gfxsnake.c         # 贪吃蛇游戏（VGA Mode 13h 像素模式）
│   ├── libc/              # 用户态标准库
│   │   ├── stdio.c        # printf/sprintf 实现
│   │   ├── stdlib.c       # exit 等工具函数
│   │   ├── string.c       # 字符串与内存操作
│   │   ├── syscall.h      # 系统调用封装
│   │   └── ...            # 头文件
│   ├── user.ld            # 用户程序链接脚本
│   ├── build.bat          # 用户程序构建脚本
│   └── programs/          # 编译输出的用户程序（移入 build/user/）
├── drivers/
│   ├── vga.c              # VGA 文本显示（80x25, 状态栏）
│   ├── keyboard.c         # 键盘驱动（中断驱动）
│   ├── timer.c            # 定时器驱动（中断驱动）
│   ├── interrupts.c       # 中断处理（C 部分）
│   ├── interrupts.asm     # 中断处理中断桩（汇编）
│   ├── gdt.asm            # GDT 表定义（汇编）
│   └── io.asm             # I/O 端口操作
├── lib/
│   ├── string.c           # 字符串处理
│   └── stdio.c            # printf/sprintf 格式化输出
├── include/               # 头文件
│   ├── types.h            # 类型定义
│   ├── vga.h
│   ├── keyboard.h
│   ├── timer.h
│   ├── shell.h
│   ├── interrupts.h
│   ├── except.h
│   ├── gdt.h
│   ├── tss.h
│   ├── pmm.h
│   ├── paging.h
│   ├── mm.h
│   ├── io.h
│   ├── string.h
│   ├── stdio.h
│   └── loader.h
├── tools/                 # 交叉编译器
├── scripts/               # 构建与运行脚本
│   ├── run.bat            # 运行 TinyOS
│   ├── run_debug.bat      # 串口调试模式运行
│   ├── run_test.bat       # 异常演示模式运行
│   ├── build.bat          # 全量构建脚本
│   ├── build_simple.bat   # 简易构建脚本
│   ├── test_build.sh      # Linux 构建脚本
│   └── test_qemu.sh       # Linux QEMU 运行脚本
├── docs/                  # 文档
│   ├── ARCHITECTURE.md    # 架构与主流程详解
│   ├── ROADMAP.md         # 演化路线图
│   └── TROUBLESHOOTING.md # 问题排查记录
├── build/                 # 编译产物（.o 目标文件、tinyos.bin、user 程序）
├── logs/                  # 串口调试日志
├── nasm.exe               # 汇编器
├── Makefile               # 增量构建（推荐）
├── linker.ld              # 链接器脚本
├── AGENTS.md              # AI 助手说明
└── README.md              # 本文件
```

## 系统架构概要

```
+-------------+     +-------------+     +-------------+     +-----------------+
|  QEMU/GRUB  | --> |  boot.asm   | --> | kernel_main | --> |  事件驱动主循环  |
|  (Bootloader)|    | (Multiboot) |     |   初始化...   |     |  halt 等待中断  |
+-------------+     +-------------+     +-------------+     +-----------------+
                                                                    |
                    +-------------------+------------------+--------+
                    |                   |                  |
                    ▼                   ▼                  ▼
              定时器中断             键盘中断         用户态切换
              (IRQ0, 50Hz)          (IRQ1)          (int 0x80)
```

### 初始化顺序

```
kernel_main()
  1. 串口初始化（调试输出）
  2. GDT 初始化（内核段 + 用户段 + TSS）
  3. VGA 初始化（清屏、光标重置）
  4. PMM 初始化（从 GRUB 获取内存映射）
  5. MM 初始化（基于 PMM 的堆分配器）
  6. 分页初始化（identity map 前 8MB）
  7. TSS 初始化（分配内核栈，供 Ring 3→Ring 0 使用）
  8. IDT 初始化（异常 + IRQ + 系统调用门）
  9. PIC 初始化（重映射，全部屏蔽）
  10. 定时器初始化（注册 handler，unmask IRQ0）
  11. 键盘初始化（注册 handler，unmask IRQ1）
  12. Shell 初始化（注册键盘回调）
  13. 启用中断（sti）
  14. 事件驱动主循环（halt）
```

### 用户态切换流程

```
testuser 命令
  │
  ▼
run_user_task(user_main)    ← 内核态（Ring 0）
  │
  ├─ 设置用户段寄存器 (DS/ES/FS/GS = 0x23)
  ├─ 构建 IRET 帧 (SS=0x23, CS=0x1B, EFLAGS.IF=1)
  └─ iret                    → 切换到用户态（Ring 3）
                                │
                                ▼
                          user_main (Ring 3)
                                │
                          ├─ syscall 1 → 打印消息
                          ├─ hlt       → 触发 GPF（被捕获并跳过）
                          └─ syscall 0 → 返回内核态
                                │
                                ▼
                          user_exit_handler (Ring 0)
                                ├─ 恢复内核段寄存器
                                ├─ 恢复内核栈
                                └─ ret → 回到 run_user_task 调用者
```

## 技术支持

### 内存布局

| 区域      | 地址             |
| ------- | -------------- |
| 内核加载地址  | 0x100000 (1MB) |
| 栈顶      | 0x108000       |
| VGA 缓冲区 | 0xB8000        |
| 内核堆     | PMM 动态分配       |

### GDT 布局

| 选择子  | 段      | DPL    |
| ---- | ------ | ------ |
| 0x00 | Null 段 | -      |
| 0x08 | 内核代码段  | Ring 0 |
| 0x10 | 内核数据段  | Ring 0 |
| 0x1B | 用户代码段  | Ring 3 |
| 0x23 | 用户数据段  | Ring 3 |
| 0x28 | TSS 段  | Ring 0 |

### 中断向量

| 向量         | 名称                       | 说明        |
| ---------- | ------------------------ | --------- |
| 0          | Divide Error             | 除零错误      |
| 6          | Invalid Opcode           | 无效操作码     |
| 8          | Double Fault             | 双重故障      |
| 13         | General Protection Fault | 通用保护故障    |
| 14         | Page Fault               | 页故障       |
| 32         | IRQ0                     | 定时器（50Hz） |
| 33         | IRQ1                     | 键盘        |
| 128 (0x80) | Syscall                  | 系统调用      |

### 构建工具链

- **NASM**: 汇编器，编译 .asm 文件
- **i686-elf-gcc**: 交叉编译器，编译 C 代码
- **i686-elf-ld**: 链接器
- **QEMU**: 模拟器，位于 `D:\Program Files\qemu`

### 构建命令

使用 Makefile 增量构建（推荐）：
```bash
make              # 构建内核和用户程序
make run          # 构建并运行
make run-debug    # 构建并运行（串口调试输出）
make run-serial   # 构建并运行（纯串口模式）
make clean        # 清理构建产物
make rebuild      # 清理并重新构建
```

手动编译命令：
```bash
# 汇编
nasm -f elf32 boot/boot.asm -o build/boot_asm.o
nasm -f elf32 drivers/interrupts.asm -o build/interrupts_asm.o
nasm -f elf32 drivers/gdt.asm -o build/gdt_asm.o
nasm -f elf32 drivers/io.asm -o build/io_asm.o
nasm -f elf32 kernel/user.asm -o build/user_asm.o
nasm -f elf32 kernel/embedded_user.asm -o build/embedded_user_asm.o

# 编译 C 文件
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c <file.c> -o <file.o>

# 链接
i686-elf-ld -T linker.ld -nostdlib -o tinyos.bin <所有 .o 文件>
```

## 许可证

MIT License
