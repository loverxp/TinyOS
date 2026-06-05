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
make run          # 构建并运行（VGA 窗口 + PS/2 键盘）
make run-debug    # 构建并运行（VGA 窗口 + 串口日志到文件）
make run-serial   # 构建并运行（纯串口模式，-nographic）
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
i686-elf-ld -T linker.ld -nostdlib -o build/tinyos.bin build/boot_asm.o build/interrupts_asm.o build/gdt_asm.o build/gdt.o build/tss.o build/io_asm.o build/kernel.o build/except.o build/shell.o build/pmm.o build/mm.o build/user_asm.o build/vga.o build/keyboard.o build/timer.o build/interrupts_c.o build/string.o build/stdio.o build/loader.o build/scheduler.o build/switch_asm.o build/embedded_user_asm.o build/embedded_hello_asm.o

### 运行
```batch
"D:\Program Files\qemu\qemu-system-i386.exe" -kernel build/tinyos.bin -m 32
```

## 项目结构
- `boot/`: 启动代码
- `kernel/`: 内核主程序（kernel.c, shell.c, gdt.c, tss.c, pmm.c, mm.c, except.c, loader.c, scheduler.c, wm.c, embedded_user.asm, user.asm, switch.asm）
- `user/`: 用户程序（crt0.s, hello.c, user.ld, build.bat）
- `drivers/`: 设备驱动（VGA、键盘、定时器、中断、GDT、串口、I/O、VBE、帧缓冲、鼠标）
- `lib/`: 库函数（字符串处理、printf/sprintf 格式化输出、PRNG、调试框架）
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
- `kernel/scheduler.c`: 抢占式 Round-Robin 调度器（TCB、prepare_switch、task_create、task_exit）
- `kernel/switch.asm`: 上下文切换汇编（do_switch、task_trampoline）
- `include/scheduler.h`: 调度器 API（task_t、task_create、prepare_switch、task_exit）
- `drivers/interrupts.c` / `drivers/interrupts.asm`: 中断处理（irq_common_stub 含调度 hook）
- `kernel/pmm.c`: 物理内存管理器（位图分配）
- `kernel/mm.c`: 堆内存分配器（kmalloc/kfree）
- `lib/stdio.c`: printf/sprintf 格式化输出实现
- `drivers/serial.c`: 串口驱动（COM1 中断收发、环形缓冲区、回调注册）
- `include/serial.h`: 串口驱动接口定义
- `drivers/ne2000.c`: NE2000 网卡驱动（PCI 发现、远程 DMA、环形缓冲区接收）
- `drivers/pci.c`: PCI 总线扫描（配置空间读写、BAR、IRQ 获取）
- `include/net.h`: 网络协议数据结构（eth/arp/ip/icmp/udp 头）
- `include/ne2000.h`: NE2000 寄存器定义和常量
- `drivers/vbe.c` / `include/vbe.h`: Bochs VBE 显卡驱动（检测、高分辨率模式设置、LFB 映射）
- `drivers/framebuf.c` / `include/framebuf.h`: 帧缓冲抽象层（putpixel、fillrect、字体渲染）
- `drivers/mouse.c` / `include/mouse.h`: PS/2 鼠标驱动（IRQ12、数据包解析、事件回调）
- `kernel/wm.c` / `include/window.h`: 窗口管理器（窗口创建/移动/关闭、Z-order、标题栏、鼠标事件）
- `lib/prng.c` / `include/prng.h`: 伪随机数生成器 (xorshift32)
- `lib/debug.c` / `include/debug.h`: 调试输出框架 (`kprintf` 四级日志、内核栈回溯)
- `kernel/net.c`: 网络协议栈（ARP / IPv4 / ICMP / UDP / TCP），含 ARP 表访问 API 和网络统计

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
- **多任务调度器**: 抢占式 Round-Robin，IRQ0 每次 tick 设置 `need_reschedule=1`，`irq_common_stub` 在 EOI 后调用 `prepare_switch()` + `do_switch()` 完成上下文切换。idle 任务（主循环）作为循环链表节点参与轮换
- **schedtest 命令**: `schedtest [N]` 创建两个测试线程交替打印 A/B，N 秒后自动退出（默认 10 秒，上限 300 秒）。任务通过 `task_exit()` 标记 FINISHED
- **gfxsnake**: 新版 VGA Mode 13h 像素模式贪吃蛇，作为用户程序在 Ring 3 运行，使用系统调用切换视频模式和读取输入
- **VGA 字模恢复机制**: Mode 13h (chain-4) 写入 `0xA0000` 时会破坏 VGA plane 2 的字体数据。内核在开机时调用 `vga_save_font()` 保存 4096 字节字模到缓冲区，切换回文本模式时由 `vga_set_mode03h()` 调用 `vga_restore_font()` 恢复
- **VGA Mode 13h 初始化顺序**: Misc Output → Sequencer (复位→编程→释放) → Graphics Controller → CRTC (解锁→编程→上锁) → Attribute Controller (编程→重新使能) → DAC 调色板。顺序错误会导致黑屏或花屏
- **图形界面 (GUI) 初始化顺序**: VBE 检测 → 模式设置 → LFB 分页映射 → 帧缓冲初始化 → PS/2 鼠标 → 窗口管理器。`fb_init()` 封装了 VBE 初始化和映射，之后调用鼠标和 WM 初始化
- **VBE (Bochs)**: 通过 I/O 端口 0x01CE/0x01CF 编程，支持检测 (写入 0xB0C2 到 ID 寄存器并读回)、模式设置 (分辨率/bpp/使能)、LFB 物理地址从 PCI BAR 读取（QEMU std VGA: vendor=0x1234, device=0x1111）
- **分页映射 LFB**: 帧缓冲物理地址通常在 0xE0000000，大小约 800×600×4=1.92MB，需要分配页表并通过 `paging_map_page()` 逐页映射。identity mapping 以简化实现
- **鼠标数据包**: PS/2 鼠标在 IRQ12 上发送 3 字节包 (状态+位移 X+位移 Y)，需同步检测 (byte[0] bit 3 必须为 1)，Y 方向取反（屏幕 Y 轴向下增长）
- **窗口管理器渲染**: `wm_redraw()` 先绘制桌面背景，然后按 Z-order 绘制所有可见窗口（先绘制底层窗口）。窗口拖拽通过标题栏鼠标按下检测 + 位移计算实现，关闭按钮在标题栏右上角

## 网络命令使用说明
- **`recv <port>`**: 监听 UDP 端口 5 秒。注意只能监听 QEMU `hostfwd` 中配置的端口（默认 8888）。其他端口需先在 Makefile 添加 `hostfwd=udp::<port>-:<port>`
- **`arp`** / **`arp -c`**: 显示或清空 ARP 缓存表
- **`netstat`**: 显示网络收发统计（ARP/ICMP/UDP/TCP 各协议的发送/接收包数、错误数）
- **`rand`**: 生成随机数（以 timer ticks 为种子的 xorshift32）
- **`ping <ip>`** / **`send <ip> <port> <msg>`**: 发送 ICMP 或 UDP，首次会自动 ARP 解析
- **Windows 发送 UDP 到 TinyOS**（Windows 无 netcat）:
  ```powershell
  python -c "import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.sendto(b'Hello', ('127.0.0.1', 8888)); s.close()"
  ```

## 已知问题（搁置）
- **退出 GUI 后键盘可能无响应**: `cmd_gui` 退出流程中 `shell_char_callback` 重注册时机与键盘中断存在竞态，或 `enable_interrupts()` 前后 8042 状态不一致。临时绕过：使用串口终端
- **首次划入 QEMU 窗口鼠标位置不正确**: PS/2 鼠标初始化后的首个数据包包含异常位移值，导致光标瞬间跳到错误位置。后续恢复正常。可免方案：忽略前 N 个数据包