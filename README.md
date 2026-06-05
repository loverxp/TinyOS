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
| `hello`       | 运行 Ring 3 示例用户程序（使用 libc） |
| `pageinfo`    | 显示页表信息                    |
| `schedtest [N]`| 启动调度器测试 N 秒（默认 10）     |
| `snake`       | 文本模式贪吃蛇游戏               |
| `gfxsnake`    | VGA 图形模式贪吃蛇游戏（像素模式） |
| `gtest`       | 内核级 VGA 图形测试              |
| `gui`         | 进入图形桌面环境（VBE 高分辨率模式，按 Esc 退出） |
| **文件系统**    |                         |
| `ls`          | 列出磁盘根目录文件               |
| `cat <文件名>`   | 显示文件内容                   |
| `diskinfo`    | 显示磁盘/文件系统信息              |
| **网络**       |                         |
| `pci`         | 列出所有 PCI 设备               |
| `net`         | 显示网络配置 (IP/网关/MAC)     |
| `ping <IP>`   | 发送 ICMP Echo 请求           |
| `send <IP> <端口> <消息>` | 发送 UDP 数据包        |

## 功能特性

- ✅ Multiboot 兼容引导
- ✅ 32位 x86 保护模式
  - ✅ GDT（全局描述符表）—— 内核段 + 用户段 + TSS
  - ✅ 用户态（Ring 3）/ 内核态（Ring 0）切换
  - ✅ TSS（任务状态段）—— 特权级切换栈管理
- ✅ VGA 文本显示 (80x25, 16色)
- ✅ VGA 图形模式 (Mode 13h, 320×200, 256 色)
- ✅ VBE 高分辨率图形模式 (Bochs VBE, 800×600×32)
- ✅ 帧缓冲驱动框架 (putpixel/fillrect/font rendering)
- ✅ 双缓冲 (backbuffer + fb_flip 消除闪屏)
- ✅ PS/2 鼠标驱动 (IRQ12, 三字节数据包, 事件回调)
- ✅ 窗口管理器 (动态创建/拖拽/关闭, Z-order, 标题栏)
- ✅ GUI 桌面环境 (`gui` 命令启动, Esc 退出恢复文本模式)
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
- ✅ printf/sprintf 格式化输出（%s, %d, %u, %x, %02x, %04x, %c, %p）
- ✅ 用户态标准库 libc（printf, sprintf, exit, 字符串函数）
- ✅ 交互式 Shell（多命令支持）
- ✅ 系统调用（int 0x80，支持从用户态返回内核态）
- ✅ 抢占式多任务调度器（Round-Robin，IRQ0 驱动）
- ✅ 串口调试输出
- ✅ ATA PIO 磁盘驱动（主 IDE 通道，28-bit LBA）
- ✅ FAT16 文件系统（只读，目录列表/文件读取）
- ✅ PCI 总线扫描（配置空间读取，设备枚举）
- ✅ NE2000 网卡驱动（远程 DMA，接收环形缓冲区，IRQ 处理）
- ✅ 网络协议栈（ARP / IPv4 / ICMP / UDP）

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
│   ├── loader.c           # 用户程序 ELF 加载器
│   ├── scheduler.c        # 抢占式多任务调度器
│   ├── fat16.c            # FAT16 文件系统解析器
│   ├── net.c              # 网络协议栈 (ARP/IP/ICMP/UDP)
│   ├── wm.c               # 窗口管理器（创建/拖拽/关闭/Z-order）
│   ├── embedded_user.asm  # 嵌入的 gfxsnake.elf
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
│   ├── io.asm             # I/O 端口操作
│   ├── ata.c              # ATA PIO 磁盘驱动
│   ├── pci.c              # PCI 总线扫描
│   ├── ne2000.c           # NE2000 网卡驱动
│   ├── vbe.c              # Bochs VBE 高分辨率显卡驱动
│   ├── framebuf.c         # 帧缓冲抽象层（putpixel/fillrect/双缓冲）
│   ├── mouse.c            # PS/2 鼠标驱动（IRQ12）
│   └── serial.c           # 串口驱动（COM1 中断收发）
├── lib/
│   ├── string.c           # 字符串处理
│   └── stdio.c            # printf/sprintf 格式化输出
├── include/               # 头文件
│   ├── types.h            # 类型定义
│   ├── elf.h              # ELF32 数据结构
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
│   ├── loader.h
│   ├── scheduler.h
│   ├── ata.h              # ATA 驱动
│   ├── fat16.h            # FAT16 文件系统
│   ├── pci.h              # PCI 总线
│   ├── ne2000.h           # NE2000 网卡
│   ├── net.h              # 网络协议栈
│   ├── vbe.h              # Bochs VBE 显卡
│   ├── framebuf.h         # 帧缓冲抽象层
│   ├── mouse.h            # PS/2 鼠标
│   ├── serial.h           # 串口驱动
│   └── window.h           # 窗口管理器
├── tools/                 # 交叉编译器
├── scripts/               # 构建与运行脚本
│   ├── mkfat16.py         # FAT16 磁盘镜像生成器
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
  3. VGA 初始化（清屏、光标重置、保存字模）
  4. VBE 检测（仅检测，不切换模式；输入 `gui` 命令后才激活）
  5. PMM 初始化（从 GRUB 获取内存映射）
  6. MM 初始化（基于 PMM 的堆分配器）
  7. 分页初始化（identity map 前 8MB）
  8. TSS 初始化（分配内核栈，供 Ring 3→Ring 0 使用）
  9. IDT 初始化（异常 + IRQ + 系统调用门）
  10. PIC 初始化（重映射，解除 IRQ 2 cascade 屏蔽）
  11. 定时器初始化（注册 handler，unmask IRQ0）
  12. 键盘初始化（注册 handler，unmask IRQ1）
  13. Shell 初始化（注册键盘回调）
  14. 启用中断（sti）
  15. 调度器初始化（idle 任务 + 循环链表）
  16. ATA 磁盘检测
  17. FAT16 文件系统挂载
  18. PCI 总线扫描
  19. NE2000 网卡 + 网络协议栈初始化
  20. 事件驱动主循环（halt）
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

## 图形模式说明

TinyOS 有两种图形模式：

| 模式 | 标准 | 分辨率 | 色深 | 使用方式 | 适用场景 |
|------|------|--------|------|----------|----------|
| **VGA Mode 13h** | 标准 VGA | 320×200 | 8-bit 调色板 (256色) | `gfxsnake` / `gtest` 命令 | 像素游戏、快速绘图 |
| **VBE GUI** | Bochs VBE | 800×600 | 32-bit 真彩色 | `gui` 命令（按 Esc 退出） | 窗口桌面、信息展示 |

- `gfxsnake` 在用户态 (Ring 3) 通过系统调用 `int 0x80` 切换视频模式
- `gui` 在内核态 (Ring 0) 通过 I/O 端口直接编程 VBE，包含鼠标和窗口管理器
- 两者可各自独立使用，互不干扰；VBE 切换会自动恢复 VGA 文本模式

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
| 43         | IRQ11                    | NE2000 网卡  |
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
make disk         # 生成 FAT16 磁盘镜像 (disk.img)
make run          # 构建并运行（含磁盘+网络）
make run-debug    # 构建并运行（串口调试输出到 logs/serial.log）
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

---

## 已知问题

- **退出 GUI 后键盘可能无响应**: `gui` 命令按 Esc 退出后偶尔键盘无响应。临时绕过：使用串口终端（`make run-debug` 或 `make run-serial`）
- **首次划入 QEMU 窗口鼠标位置不正确**: PS/2 鼠标初始化后的首个数据包包含异常位移值，导致光标瞬间跳到错误位置，后续恢复正常。

详见 [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) 中的详细分析。

## 许可证

MIT License

---

## 文件系统使用指南

### 前置条件

运行前需先生成磁盘镜像：
```bash
make disk          # 生成 disk.img（16MB FAT16，内含示例文件）
```

`make run` / `make run-debug` 会自动挂载磁盘镜像。

### 命令示例

```
TinyOS> ls
Directory listing:
Name                Size  Type
--------------------------------------
TINYOS                 0  VOL
README.TXT            49  FILE
HELLO.C               91  FILE
CONFIG.TXT            60  FILE
TEST.TXT             760  FILE

5 entries

TinyOS> cat readme.txt
Welcome to TinyOS!
This is a FAT16 disk image.
(49 bytes)

TinyOS> diskinfo
Disk info:
  Total size:      16384 KB
  Bytes/sector:    512
  Sectors/cluster: 4
  Total clusters:  8120
  Root entries:    512
```

### 自定义磁盘内容

编辑 `scripts/mkfat16.py` 中的 `files` 列表，然后重新运行 `make disk`。

### 磁盘镜像工具

#### `scripts/mkfat16.py` — FAT16 镜像生成器

纯 Python 脚本，无需任何外部依赖，从 Python 字符串直接构建合法的 FAT16 磁盘镜像。

**功能：**
- 生成 16 MB raw disk image（`disk.img`）
- 写入完整的 BPB（BIOS Parameter Block）、两份 FAT 表、根目录
- 将文件分配到连续簇，并构建 FAT 链表

**磁盘参数：**

| 参数 | 值 |
|------|------|
| 总大小 | 16 MB（32768 扇区） |
| 扇区大小 | 512 字节 |
| 每簇扇区数 | 4（2 KB/簇） |
| FAT 副本数 | 2 |
| 根目录条目数 | 512 |
| FAT 表大小 | 128 扇区 |

**内置示例文件：**

| 文件名 | 内容 | 大小 |
|--------|------|------|
| `README.TXT` | 欢迎信息 | 49 B |
| `HELLO.C` | C 示例程序 | 91 B |
| `CONFIG.TXT` | 系统配置参数 | 60 B |
| `TEST.TXT` | 测试文件（20 行重复） | 760 B |

**添加文件：**

```python
# 在 mkfat16.py 的 files 列表中追加：
files = [
    ("readme.txt", b"Welcome to TinyOS!\r\n..."),
    ("hello.c",    b'#include <stdio.h>\r\n...'),
    ("config.txt", b"# TinyOS Configuration\r\n..."),
    ("test.txt",   b"This is a test file...\r\n" * 20),
    # 新增文件：
    ("data.bin",   bytes(range(256))),   # 256 字节二进制数据
    ("notes.txt",  b"My notes here\r\n"),
]
```

**手动运行：**

```batch
python scripts\mkfat16.py disk.img
```

> **注意：** 修改后需重新生成 `disk.img` 才能生效。`make disk` 会自动调用此脚本。

#### `scripts/mkfs.bat` — 备用磁盘创建脚本

Windows 批处理脚本，尝试用 Python 创建磁盘；若 Python 不可用则回退到 `fsutil` 创建空白镜像。

```batch
scripts\mkfs.bat
```

**流程：**
1. 优先执行 `python scripts\mkfat16.py disk.img`（含文件系统 + 文件）
2. 若 Python 不存在或失败，用 `fsutil file createnew disk.img 16777216` 创建 16 MB 空文件

> **提示：** 推荐使用 `make disk` 而不是直接运行 `mkfs.bat`。

---

## 网络使用指南

### QEMU 网络配置

`make run` 会自动配置 QEMU user-mode 网络：
```
-netdev user,id=net0,hostfwd=udp::8888-:8888
-device ne2k_pci,netdev=net0
```

### 网络地址

| 角色 | IP | 说明 |
|------|------|------|
| 虚拟机 | 10.0.2.15 | TinyOS 固定 IP |
| 网关 | 10.0.2.2 | QEMU 内置 NAT |
| DNS | 10.0.2.3 | QEMU 内置 DNS |

### 命令示例

```
TinyOS> pci
PCI devices:
Bus    Dev    Fn   Vendor       Device       Class
----------------------------------------------
  0     3     0   0x10EC       0x8029       Network
  0     1     0   0x8086       0x7000       Bridge
  0     0     0   0x8086       0x1237       Bridge

3 device(s) found

TinyOS> net
Network config:
  IP:      10.0.2.15
  Gateway: 10.0.2.2
  Mask:    255.255.255.0
  MAC:     52:54:00:12:34:56

TinyOS> ping 10.0.2.2    ← ARP 自动解析（首次会触发 ARP 请求）
[1/4] Target IP: 10.0.2.2
[2/4] Route: dst=10.0.2.2 direct (same subnet)
[3/4] Sending ICMP echo...
[4/4] ARP table miss -> sending ARP request...
  Polling for ARP reply... (attempt 1)
  Polling for ARP reply... (attempt 2)
  ARP resolved OK!
Ping sent to 10.0.2.2

TinyOS> ping 10.0.2.2    ← 再次 ping，ARP 已缓存
[1/4] Target IP: 10.0.2.2
[2/4] Route: dst=10.0.2.2 direct (same subnet)
[3/4] Sending ICMP echo...
[4/4] ARP already cached
Ping sent to 10.0.2.2

TinyOS> send 10.0.2.2 8989 Hello World!
[1/4] Target: 10.0.2.2:8989, msg="Hello World!"
[2/4] Route: dst=10.0.2.2 via gateway 10.0.2.2
[3/4] Sending UDP...
[4/4] ARP already cached
Sent 12 bytes to 10.0.2.2:8989
```

### 在 Windows 上接收 UDP

先在 Windows 上启动监听，再在 TinyOS 中执行 `send` 命令。

**方法 1：Python（推荐）**
```python
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('0.0.0.0', 8989))
print('Listening on UDP 8989...')
data, addr = s.recvfrom(1024)
print(f'Received: {data.decode()} from {addr}')
s.close()
```

**方法 2：PowerShell**
```powershell
$udp = New-Object System.Net.Sockets.UdpClient(8989)
$remote = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
$data = $udp.Receive([ref]$remote)
[System.Text.Encoding]::UTF8.GetString($data)
$udp.Close()
```

> 提示：从 TinyOS 发往主机的 UDP 数据包无需 `hostfwd` 端口转发，QEMU user-mode 网络默认可达主机（10.0.2.2）。`hostfwd` 仅用于主机向虚拟机发送数据。
