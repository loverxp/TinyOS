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
```bash
# 汇编
nasm -f elf32 boot/boot.asm -o build/boot.o
nasm -f elf32 drivers/interrupts.asm -o build/interrupts.o
nasm -f elf32 drivers/gdt.asm -o build/gdt.o
nasm -f elf32 drivers/io.asm -o build/io.o

# 编译 C 文件
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c <file.c> -o <file.o>

# 链接
i686-elf-ld -T linker.ld -nostdlib -o tinyos.bin <所有 .o 文件>
```

### 运行
```batch
"D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32
```

## 项目结构
- `boot/`: 启动代码
- `kernel/`: 内核主程序
- `drivers/`: 设备驱动（VGA、键盘、定时器、中断）
- `lib/`: 库函数（字符串处理）
- `include/`: 头文件
- `tools/`: 交叉编译工具链（已下载到本地）

## 关键文件
- `boot/boot.asm`: Multiboot 引导头，内核入口点
- `linker.ld`: 链接器脚本，定义内存布局
- `kernel/kernel.c`: 内核主函数

## 内存布局
- 内核加载地址: 0x100000 (1MB)
- 栈顶: 0x108000
- VGA 缓冲区: 0xB8000

## 注意事项
- 使用 `-fno-stack-protector` 避免需要 libssp
- 使用 `-nostdlib -nostdinc` 不使用标准库
- 使用 `-fno-pic -fno-pie` 生成位置相关代码
- Multiboot 魔数: 0x1BADB002
