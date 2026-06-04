# TinyOS 项目说明

## 项目概述
这是一个基于 C 语言的简单操作系统内核，可在 QEMU 模拟器上运行。

## 构建系统

### 工具链
- **NASM**: 汇编器，用于编译 .asm 文件
- **i686-elf-gcc**: 交叉编译器，用于编译 C 代码
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
make user-programs # 仅编译用户程序
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
i686-elf-ld -T linker.ld -nostdlib -o build/tinyos.bin build/boot_asm.o build/interrupts_asm.o build/gdt_asm.o build/gdt.o build/tss.o build/io_asm.o build/kernel.o build/except.o build/shell.o build/pmm.o build/mm.o build/user_asm.o build/vga.o build/keyboard.o build/timer.o build/interrupts_c.o build/string.o build/stdio.o build/loader.o build/embedded_user_asm.o

### 运行
```batch
"D:\Program Files\qemu\qemu-system-i386.exe" -kernel build/tinyos.bin -m 32
```

## 项目结构
- `boot/`: 启动代码
- `kernel/`: 内核主程序（kernel.c, shell.c, gdt.c, tss.c, pmm.c, mm.c, except.c, loader.c, embedded_user.asm, user.asm）
- `user/`: 用户程序（crt0.s, hello.c, user.ld, build.bat）
- `drivers/`: 设备驱动（VGA、键盘、定时器、中断、GDT、I/O）
- `lib/`: 库函数（字符串处理、printf/sprintf 格式化输出）
- `include/`: 头文件
- `tools/`: 交叉编译工具链（已下载到本地）

## 关键文件
- `boot/boot.asm`: Multiboot 引导头，内核入口点
- `linker.ld`: 链接器脚本，定义内存布局
- `kernel/kernel.c`: 内核主函数
- `drivers/gdt.asm` / `kernel/gdt.c`: GDT 定义与初始化
- `kernel/tss.c`: TSS 初始化，管理 Ring 3→Ring 0 栈切换
- `kernel/user.asm`: 用户态入口、Ring 3 切换及退出（含自定义栈版本 `run_user_task_ex`）
- `kernel/loader.c`: 用户程序加载器，拷贝二进制到 0x400000，分配栈，执行
- `kernel/embedded_user.asm`: 使用 `incbin` 嵌入用户程序二进制
- `drivers/interrupts.c` / `drivers/interrupts.asm`: 中断处理
- `kernel/pmm.c`: 物理内存管理器（位图分配）
- `kernel/mm.c`: 堆内存分配器（kmalloc/kfree）
- `lib/stdio.c`: printf/sprintf 格式化输出实现

## 内存布局
- 内核加载地址: 0x100000 (1MB)
- 栈顶: 0x108000
- VGA 缓冲区: 0xB8000

## GDT 布局
| 选择子 | 段 | DPL |
|--------|-----|-----|
| 0x00 | Null 段 | - |
| 0x08 | 内核代码段 | Ring 0 |
| 0x10 | 内核数据段 | Ring 0 |
| 0x18 | 用户代码段 | Ring 3 |
| 0x20 | 用户数据段 | Ring 3 |
| 0x28 | TSS 段 | Ring 0 |

用户态使用选择子时需设置 RPL=3：CS=0x1B(0x18|3), DS/SS=0x23(0x20|3)

## 注意事项
- 使用 `-fno-stack-protector` 避免需要 libssp
- 使用 `-nostdlib -nostdinc` 不使用标准库
- 使用 `-fno-pic -fno-pie` 生成位置相关代码
- Multiboot 魔数: 0x1BADB002
- TSS 需要专用内核栈（非当前内核栈），避免中断时栈溢出
- IRQ 的 IDT 门描述符 DPL 设为 3 (0xEE) 以允许用户态接收中断
- 系统调用门 (int 0x80) DPL 设为 3 (0xEE) 以允许用户态触发
- 在中断处理链中运行长时间逻辑（如 Snake 游戏）时，必须手动发送对应 IRQ 的 EOI (`outb(0x20, 0x20)`)，否则 PIC 会阻塞该 IRQ 的后续中断。正常 Shell 命令无此问题，因为回调立即返回，EOI 能正常发送；只有**劫持中断流程的长驻逻辑**才需手动 EOI
- Snake 游戏采用中断驱动架构：`snake_tick`（IRQ0 回调）处理游戏逻辑，主循环负责渲染（通过 `needs_render` 标志）
- **gfxsnake**: 新版 VGA Mode 13h 像素模式贪吃蛇，作为用户程序在 Ring 3 运行，使用系统调用切换视频模式和读取输入
- **VGA 字模恢复机制**: Mode 13h (chain-4) 写入 `0xA0000` 时会破坏 VGA plane 2 的字体数据。内核在开机时调用 `vga_save_font()` 保存 4096 字节字模到缓冲区，切换回文本模式时由 `vga_set_mode03h()` 调用 `vga_restore_font()` 恢复
- **VGA Mode 13h 初始化顺序**: Misc Output → Sequencer (复位→编程→释放) → Graphics Controller → CRTC (解锁→编程→上锁) → Attribute Controller (编程→重新使能) → DAC 调色板。顺序错误会导致黑屏或花屏