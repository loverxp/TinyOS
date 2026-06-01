# TinyOS - 简单操作系统

一个基于 C 语言的简单操作系统，可在 QEMU 模拟器上运行。

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

## 项目结构

```
TinyOS/
├── boot/boot.asm          # Multiboot 引导程序
├── kernel/kernel.c        # 内核主程序
├── drivers/               # 设备驱动
│   ├── vga.c             # VGA 显示
│   ├── keyboard.c        # 键盘驱动
│   ├── timer.c           # 定时器
│   ├── interrupts.c/asm  # 中断处理
│   ├── gdt.asm           # 全局描述符表
│   └── io.asm            # I/O 端口
├── lib/string.c          # 字符串处理
├── include/              # 头文件
├── tools/                # 交叉编译器 (2GB+)
├── nasm.exe              # 汇编器
├── tinyos.bin            # 编译后的内核
├── build_simple.bat      # 构建脚本
└── run.bat               # 运行脚本
```

## 功能特性

- ✅ Multiboot 兼容引导
- ✅ 32位 x86 保护模式
- ✅ VGA 文本显示 (80x25, 16色)
- ✅ 键盘输入支持
- ✅ 定时器中断
- ✅ 中断描述符表 (IDT)
- ✅ 硬件光标

## 修复记录

### 键盘无法输入
**原因**: 内核未初始化中断系统（IDT/PIC），键盘中断未被处理
**修复**: 在 `kernel_main()` 中添加 `idt_initialize()`、`pic_initialize()`、`register_interrupt_handler()` 和 `enable_interrupts()`

## 许可证

MIT License
