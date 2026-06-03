# TinyOS - 简单操作系统

一个基于 C 语言的简单操作系统内核，可在 QEMU 模拟器上运行。

## 快速开始

### 1. 直接运行（已编译好）

双击 **`run.bat`** 即可启动 QEMU 运行操作系统。

### 2. 重新构建并运行

双击 **`build_simple.bat`** 会自动编译并启动。

## 键盘输入使用说明

**重要：必须先在 QEMU 窗口内点击一下，才能接收键盘输入！**

1. 双击 `run.bat` 启动 QEMU
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
   "D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32
   ```

## Shell 命令

进入系统后，可直接在 `TinyOS>` 提示符下输入命令：

| 命令 | 说明 |
|------|------|
| `help` | 显示所有可用命令 |
| `clear` | 清屏 |
| `uptime` | 显示系统运行时间 |
| `meminfo` | 显示物理内存信息 |
| `alloc <大小>` | 分配指定字节的内存并显示地址 |
| `free <地址>` | 释放指定地址的内存 |
| `kmtest` | 运行 kmalloc/kfree 堆分配器测试 |
| `except` | 触发除零异常测试异常处理 |
| `echo <文本>` | 回显输入的文本 |

## 项目结构

```
TinyOS/
├── boot/boot.asm           # Multiboot 引导程序
├── kernel/
│   ├── kernel.c           # 内核主程序
│   ├── shell.c            # 交互式 Shell
│   ├── pmm.c              # 物理内存管理器（位图分配）
│   ├── mm.c               # 堆内存分配器（kmalloc/kfree）
│   └── except.c           # 异常处理
├── drivers/
│   ├── vga.c              # VGA 文本显示（80x25, 状态栏）
│   ├── keyboard.c         # 键盘驱动（事件驱动）
│   ├── timer.c            # 定时器驱动（事件驱动）
│   ├── interrupts.c       # 中断处理（C 部分）
│   ├── interrupts.asm     # 中断处理（汇编部分）
│   ├── gdt.asm            # 全局描述符表
│   └── io.asm             # I/O 端口操作
├── lib/string.c           # 字符串处理
├── include/               # 头文件
│   ├── vga.h
│   ├── keyboard.h
│   ├── timer.h
│   ├── interrupts.h
│   ├── pmm.h
│   └── mm.h
├── tools/                 # 交叉编译器
├── nasm.exe               # 汇编器
├── linker.ld              # 链接器脚本
├── tinyos.bin             # 编译后的内核
├── build_simple.bat       # 构建脚本
└── run.bat                # 运行脚本
```

## 功能特性

- ✅ Multiboot 兼容引导
- ✅ 32位 x86 保护模式（GDT）
- ✅ VGA 文本显示 (80x25, 16色)
- ✅ VGA 状态栏（系统运行时间、堆使用量、空闲内存）
- ✅ 硬件光标
- ✅ 键盘输入（事件驱动）
- ✅ 定时器中断（事件驱动）
- ✅ 中断描述符表 (IDT)
- ✅ 异常处理（除零等）
- ✅ 物理内存管理（PMM，位图式页帧分配器）
- ✅ 堆内存分配器（kmalloc/kfree，块式管理）
- ✅ 交互式 Shell（多命令支持）
- ✅ 串口调试输出

## 技术支持

### 内存布局

| 区域 | 地址 |
|------|------|
| 内核加载地址 | 0x100000 (1MB) |
| 栈顶 | 0x108000 |
| VGA 缓冲区 | 0xB8000 |
| 内核堆 | PMM 动态分配 |

### 构建工具链

- **NASM**: 汇编器，编译 .asm 文件
- **i686-elf-gcc**: 交叉编译器，编译 C 代码
- **i686-elf-ld**: 链接器
- **QEMU**: 模拟器，位于 `D:\Program Files\qemu`

## 许可证

MIT License