# TinyOS 问题排查记录

## 问题1：键盘无法输入

### 现象
QEMU 启动后显示正常，但按键盘无任何反应。

### 根本原因
**中断系统完全没有初始化**。键盘中断（IRQ1）从未被启用：
- ❌ 未调用 `idt_initialize()` 初始化中断描述符表
- ❌ 未调用 `pic_initialize()` 重映射 PIC
- ❌ 未注册键盘中断处理程序
- ❌ 未调用 `enable_interrupts()` 开启中断

### 解决过程

**第一次尝试**：在 `kernel_main()` 中添加中断初始化
```c
idt_initialize();
pic_initialize();
register_interrupt_handler(33, keyboard_handler);
enable_interrupts();
```
结果：**失败**，出现中断风暴。

**第二次尝试**：发现 `pic_initialize()` 中 `outb(0x21, 0x00)` 是**取消屏蔽所有中断**
结果：**失败**，产生大量 `irq=0x10` 的未知中断。

**第三次尝试**：修改 PIC 初始化逻辑
- 初始化时**屏蔽所有中断** (`0xFF`)
- 注册 handler 后再**逐个 unmask**
```c
pic_initialize();           // 全部屏蔽
register_interrupt_handler(32, timer_handler);
pic_unmask_irq(0);          // 启用 timer
register_interrupt_handler(33, keyboard_handler);
pic_unmask_irq(1);          // 启用 keyboard
enable_interrupts();
```
结果：**失败**，仍然没有定时器 tick。

**第四次尝试**：检查汇编 stub，发现 `irq_common_stub` **没有传递参数**给 `irq_handler`
```nasm
; 错误代码
irq_common_stub:
    pusha
    ...
    call irq_handler    ; 没有压入参数！
    ...
```

**最终修复**：在 `irq_common_stub` 中添加参数传递
```nasm
irq_common_stub:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    ...
    mov ebx, [esp+36]   ; 获取中断向量号
    sub ebx, 32         ; 转换为 IRQ 号
    push ebx            ; 压入参数
    call irq_handler
    add esp, 4          ; 清理参数
    ...
```
结果：**成功！** 键盘和定时器都正常工作。

---

## 问题2：编译错误 - `i686-elf-gcc` 找不到 `cc1`

### 现象
```
i686-elf-gcc: fatal error: cannot execute 'cc1': CreateProcess: No such file or directory
```

### 原因
交叉编译器的 `cc1.exe` 位于 `libexec/gcc/i686-elf/13.2.0/`，但 GCC 找不到它。

### 解决
将 `libexec` 目录复制到工具链目录：
```bash
cp -r /tmp/libexec tools/libexec
```

---

## 问题3：链接错误 - `undefined reference to 'outb'`

### 现象
```
undefined reference to `outb'
```

### 原因
`io.h` 中 `outb` 定义为 `static inline`，但编译器未内联，导致链接时找不到符号。

### 解决
创建 `drivers/io.asm` 提供汇编实现：
```nasm
global outb
outb:
    mov al, [esp + 8]
    mov dx, [esp + 4]
    out dx, al
    ret
```

---

## 问题4：链接错误 - `relocation truncated to fit`

### 现象
```
drivers/gdt.asm:(.text+0x2a): relocation truncated to fit: R_386_16 against `.text'
```

### 原因
GDT 放在 `.text` 段，但代码中使用 16 位跳转 `jmp CODE_SEG:.reload_cs`。

### 解决
将 GDT 移到 `.data` 段：
```nasm
section .data
align 4
gdt_start:
    ...
```

---

## 问题5：NASM 检测失败

### 现象
批处理脚本提示 `ERROR: NASM not found`，但 `nasm.exe` 存在于当前目录。

### 原因
1. `setlocal EnableDelayedExpansion` 导致变量扩展问题
2. `where nasm` 命令被其他进程占用

### 解决
简化批处理脚本，直接使用当前目录的 `nasm.exe`：
```batch
set NASM=nasm.exe
if not exist %NASM% (
    echo ERROR: nasm.exe not found
    exit /b 1
)
%NASM% -f elf32 boot/boot.asm -o build/boot.o
```

---

## 问题6：LD 检测失败

### 现象
```
ERROR: i686-elf-ld not found
```

### 原因
`tools\i686-elf-ld.exe` 存在，但批处理中嵌套 `if` 逻辑错误。

### 解决
使用 `goto` 标签替代嵌套 `if`：
```batch
set LD=tools\bin\i686-elf-ld.exe
if exist %LD% goto ld_found
set LD=tools\i686-elf-ld.exe
if exist %LD% goto ld_found
echo ERROR: i686-elf-ld not found
exit /b 1
:ld_found
```

---

## 问题7：用户态实现 - GPF 错误码 0x18

### 现象
切换到用户态后界面不停打印错误。

### 原因
用户代码段选择子缺少 RPL=3，导致特权级切换失败。
- 错误码 `0x18` = 用户代码段选择子索引（GDT[3] = 0x18），但缺少 RPL 位
- 正确的选择子应为 `0x1B`（0x18 | 3，即 RPL=3）

### 解决
修正段选择子，确保所有用户态段寄存器设置 RPL=3：
```nasm
; 错误
push 0x18    ; CS = 用户代码段，RPL=0
push 0x20    ; SS = 用户数据段，RPL=0

; 修复
push 0x1B    ; CS = 用户代码段 | RPL=3
push 0x23    ; SS = 用户数据段 | RPL=3
```

---

## 问题8：用户态无法输入

### 现象
切换到用户态后键盘无法输入。

### 原因
1. 用户态 `EFLAGS.IF=0` 禁用了中断
2. IRQ 的 IDT 门描述符 DPL=0，不允许用户态接收中断

### 解决
1. 在 IRET 帧中设置 `EFLAGS.IF=1`（0x3200）
2. 将 IRQ 的 IDT 门描述符 DPL 改为 3（flags = 0xEE）
```c
// 0xEE = present, DPL=3, interrupt gate (32-bit)
idt_set_gate(32, (uint32_t)irq0, 0x08, 0xEE);
```

---

## 问题9：用户态无法返回内核态

### 现象
用户态程序执行完毕无法回到内核 Shell。

### 原因
缺少从用户态（Ring 3）返回内核态（Ring 0）的机制。

### 解决
实现 `syscall 0`，通过修改中断栈帧实现返回：
1. 在 `syscall_handler` 中将 EIP 改为 `user_exit_handler`
2. 将 CS 改为内核代码段（0x08，RPL=0）
3. 执行 IRET 时 CPU 检测到同级特权级切换（CS.RPL=0 == CPL=0），仅弹出 EIP、CS、EFLAGS
4. `user_exit_handler` 恢复内核数据段和内核栈指针，`ret` 返回调用者

```c
void syscall_handler(uint32_t* regs) {
    if (syscall_no == 0) {
        regs[11] = (uint32_t)user_exit_handler;  // EIP
        regs[12] = 0x08;                          // CS = 内核代码段 (RPL=0)
        return;
    }
}
```

---

## 问题10：TSS 栈溢出导致数据破坏

### 现象
进入用户态后系统行为异常，包括状态栏数据显示错误、Shell 命令执行异常等。

### 原因
TSS.ESP0 指向 `kernel_main` 当前栈帧内部。当用户态触发中断时，CPU 自动切换到 TSS.ESP0 指定的内核栈，覆盖了正在使用的栈空间。

### 解决
为 TSS 分配专用 4KB 内核栈，不共享当前内核栈：
```c
static uint8_t tss_kernel_stack[4096] __attribute__((aligned(16)));
tss_init((uint32_t)tss_kernel_stack + 4096);
```

---

## 调试技巧总结

### 1. 串口调试
```c
static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}
```
启动 QEMU 时添加 `-serial stdio` 即可在终端看到输出。

### 2. 检查中断是否触发
在 `irq_handler` 和 `keyboard_handler` 中添加串口输出，确认中断路径。

### 3. 反汇编检查
```bash
i686-elf-objdump -d tinyos.bin | grep -A 20 "<irq_common_stub>:"
```
确认参数是否正确传递。

### 4. QEMU 调试选项
```bash
qemu-system-i386 -kernel build/tinyos.bin -d int,cpu_reset
```

### 5. 分页调试
启用分页后异常向量 14 (Page Fault) 会显示 CR2 寄存器值（出错的虚拟地址），帮助定位页表映射问题。

### 6. 检查异常错误码
异常错误码包含段选择子信息，可用于排查用户态特权级相关问题：
- 错误码 `0x00`：非段相关违规（如特权指令）
- 错误码 `0x18`：GDT[3] 段选择子（即用户代码段），通常表示缺少 RPL=3

---

## 问题11：启用分页后用户态程序无法运行

### 现象
启用分页后 `runuser` 命令执行用户程序时崩溃或无响应。

### 原因
页表未正确映射用户程序地址 `0x400000`。分页初始化时只 identity map 了前 8MB 物理内存，如果页表覆盖范围不足，用户程序地址无法访问。

### 解决
确保页表 identity map 覆盖用户程序地址：
```c
// 映射 0~8MB（包含 0x400000）
for (int t = 0; t < 2; t++) {  // 2 个页表 = 8MB
    page_tables[t] = (page_table_entry_t*)pmm_alloc_page();
    uint32_t base = t * 0x400000;
    for (int i = 0; i < PT_ENTRIES; i++) {
        // 设置 present=1, rw=1, user=1
        // 物理地址 = base + i * 4096
    }
    // 设置页目录项
}
```

---

## 问题12：贪吃蛇游戏键盘无响应

### 现象
运行 `snake` 命令后游戏画面正常显示，蛇会按初始方向自动移动，但：
- 按方向键 / WASD 无法改变方向
- 按 Q / ESC 无法退出游戏
- 游戏完全无法操作

### 根本原因
**PIC 的 IRQ1 EOI 未发送，导致键盘中断被阻塞。**

`snake_start()` 的调用链如下：
```
IRQ1 触发 → keyboard_handler() → char_callback('\n')
  → shell_char_callback() → shell_handle_command("snake")
    → snake_start(0)
```

当 IRQ1 触发时，PIC（可编程中断控制器）将 IRQ1 标记为 "In-Service"（ISR 位置位）。
在发送 EOI（End of Interrupt）之前，PIC 不会再次递送 IRQ1——即使有新的键盘事件。

由于 `snake_start()` 在中断处理函数内部运行其游戏主循环，而原始的 `keyboard_handler()`
尚未返回（EOI 在 `irq_handler()` 末尾的 `pic_send_eoi()` 中发送），所以 IRQ1 始终被
PIC 阻塞，键盘完全无响应。

### 解决
在 `snake_start()` 进入游戏循环之前，手动发送 IRQ1 的 EOI：
```c
// snake_start 被调用自 IRQ1 中断处理链中
// 必须发送 EOI 让 PIC 允许新的键盘中断
outb(0x20, 0x20);  // 向主 PIC 发送 EOI

keyboard_register_raw_callback(snake_raw_cb);
keyboard_register_char_callback(NULL);
timer_register_tick_callback(snake_tick);

render_all();
enable_interrupts();  // sti

while (running) {
    asm volatile("hlt");
    if (needs_render) {
        needs_render = 0;
        render_update();
    }
}
```

**额外优化：**
- 将 `snake_tick` 中的渲染移到主循环（通过 `needs_render` 标志），避免在中断上下文中做大量 VGA 写入
- `render_update()` 中补上 `draw_border()` 避免边框被擦除

### 为什么正常 Shell 使用时没有问题？

关键在于回调函数**是否立即返回**。

**正常 Shell 操作：**
```
IRQ1 触发
  → keyboard_handler()
    → char_callback('a')
      → shell_char_callback('a')   ← 打印一个字符，立即返回
    → 返回
  → pic_send_eoi(1)                ← EOI 正常发送 ✓
  → iret                           ← 回到主循环
```

`shell_char_callback` 处理一个字符后在微秒级返回。IRQ 处理链正常完成，
`irq_handler` 末尾的 `pic_send_eoi()` 被正常调用。PIC 清除 IRQ1 的
"In-Service"位，下次按键可以正常触发中断。

**贪吃蛇游戏：**
```
IRQ1 触发（用户输入 "snake" 后按回车）
  → keyboard_handler()
    → char_callback('\n')
      → shell_char_callback('\n')
        → shell_handle_command("snake")
          → snake_start(0)
            → while (running) {    ← 卡在这里，永远不返回！
                hlt;
              }
```

`snake_start` 在回调函数内部运行**整个游戏循环**，直到游戏结束才返回。
因此 `keyboard_handler` 永不返回，`irq_handler` 永远到不了 `pic_send_eoi()`，
PIC 始终将 IRQ1 标记为 "In-Service"——**无限期阻塞所有后续键盘中断**。

### 经验教训
> 1. 如果一个函数在中断处理链中启动长时间运行的逻辑（如游戏循环），必须确保对应 IRQ 的
>    EOI 已经发送，否则 PIC 会阻塞该 IRQ 的后续中断。
> 2. 正常 Shell 命令不会有此问题，因为回调立即返回，EOI 能正常发送。
>    只有**劫持中断处理流程**的长驻逻辑才需要手动发送 EOI。

---

## 问题13：gfxsnake 启动黑屏（VGA Mode 13h 初始化失败）

### 现象
运行 `gfxsnake` 命令后屏幕全黑，按键无反应。游戏应显示状态栏、边框、蛇和食物。

### 根本原因
VGA Mode 13h 寄存器编程顺序不正确：
1. 缺少 DAC 调色板初始化 — 像素值需要有调色板才能映射为颜色
2. CRTC/Sequencer/Attribute Controller 寄存器编程顺序错误

### 解决
修正寄存器编程顺序为：Misc Output → Sequencer（复位→编程→释放）→ Graphics Controller → CRTC（解锁→编程→上锁）→ Attribute Controller（编程→重新使能视频）→ DAC 调色板

关键代码在 `vga_set_mode13h()`：
```c
// 1. Misc Output — 必须在 CRTC 之前设置，因为 CRTC 时序依赖点时钟
outb(0x3C2, 0x63);

// 2. Sequencer — 在复位状态下编程，完成后释放
outb(0x3C4, 0x00); outb(0x3C5, 0x01);  // 复位
// ... 编程寄存器 ...
outb(0x3C4, 0x00); outb(0x3C5, 0x03);  // 释放复位

// 3. Graphics Controller
outb(0x3CE, 0x05); outb(0x3CF, 0x40);  // Mode: 256-color
outb(0x3CE, 0x06); outb(0x3CF, 0x05);  // Misc: A0000, graphics

// 4. CRTC — 先解锁再编程最后上锁
outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & 0x7F);  // 解锁
// ... CRTC 时序寄存器 ...
outb(0x3D4, 0x11); outb(0x3D5, 0x8E);  // 上锁

// 5. Attribute Controller — 先复位 flip-flop
inb(0x3DA);
outb(0x3C0, 0x10); outb(0x3C0, 0x41);  // Mode Control
// ... 其他寄存器 ...
outb(0x3C0, 0x20);  // 重新使能视频

// 6. DAC 调色板 — 初始化标准 16 色
outb(0x3C8, 0);
for (int i = 0; i < 16; i++) {
    outb(0x3C9, r); outb(0x3C9, g); outb(0x3C9, b);
}
```

### 验证
- 内核级 `gtest` 命令显示绿色背景和交叉图案
- gfxsnake 游戏正常运行

---

## 问题14：退出 gfxsnake 后屏幕花屏（竖线、字符黑块）

### 现象
退出贪吃蛇游戏后，屏幕出现大量竖线，原本应该显示字符的地方变为黑块。

### 根本原因
**VGA 字模数据被破坏。**

Mode 13h 使用 chain-4 线性帧缓冲模式。写入像素到 `0xA0000` 时，每 4 个字节中有一个写入 VGA plane 2（通过 chain-4 交织映射）。Plane 2 在文本模式下存储由 BIOS 加载的 8x16 字模位图（256 字符 × 16 字节 = 4096 字节）。

Mode 13h 的像素写入覆盖了 plane 2 的字模数据，导致切换回文本模式后 VGA 用损坏的字模渲染字符 — 看起来就是花屏和黑块。

### 解决
#### 1. 开机保存字模
新增 `vga_save_font()` 在 `vga_initialize()` 后立即调用：
```c
void vga_save_font(void) {
    // 临时配置 VGA 寄存器以读取 plane 2
    outb(0x3C4, 0x04); outb(0x3C5, 0x06);  // Memory Mode: 无 chain-4
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);  // Map Mask: plane 2
    outb(0x3CE, 0x04); outb(0x3CF, 0x02);  // Read Map: plane 2
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);  // Mode: 无 odd/even
    outb(0x3CE, 0x06); outb(0x3CF, 0x05);  // Misc: A0000
    
    // 读取 4096 字节字模数据
    volatile uint8_t* font_vram = (volatile uint8_t*)0xA0000;
    for (int i = 0; i < 4096; i++)
        font_buf[i] = font_vram[i];
    
    // 恢复寄存器
}
```

#### 2. 切换回文本模式时恢复字模
在 `vga_set_mode03h()` 末尾调用 `vga_restore_font()`：
```c
static void vga_restore_font(void) {
    // 配置为写入 plane 2
    outb(0x3C4, 0x04); outb(0x3C5, 0x06);
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);
    outb(0x3CE, 0x06); outb(0x3CF, 0x05);
    outb(0x3CE, 0x00); outb(0x3CF, 0x00);  // Set/Reset: 无
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);  // Enable Set/Reset: 无
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);  // Bit Mask: 全部
    
    // 写回 4096 字节字模
    volatile uint8_t* font_vram = (volatile uint8_t*)0xA0000;
    for (int i = 0; i < 4096; i++)
        font_vram[i] = font_buf[i];
}
```

#### 3. 初始化 Attribute Controller 调色板
在 `vga_set_mode03h()` 中添加 16 个调色板寄存器的初始化为恒等映射（颜色 i → DAC 索引 i）：
```c
inb(0x3DA);  // 复位 flip-flop
for (int i = 0; i < 16; i++) {
    outb(0x3C0, i);       // Palette 寄存器索引
    outb(0x3C0, i);       // 值 = DAC 索引（恒等映射）
}
```

### 涉及文件
- `drivers/interrupts.c`: 新增 `vga_save_font()`、`vga_restore_font()`；修改 `vga_set_mode03h()` 添加调色板初始化和字模恢复
- `kernel/kernel.c`: 在 `vga_initialize()` 后添加 `vga_save_font()` 调用
- `include/vga.h`: 添加 `vga_save_font()` 声明

### 验证
退出 gfxsnake 后文本模式恢复正常，无竖线，字符显示正确。

---

## 问题15：用户程序 hello 输出被清除

### 现象
运行 `hello` 命令后，用户程序输出的 "Hello from user mode!" 和 "Running in Ring 3 via libc" 不可见，只显示 "Return to kernel mode" 或 "Hello program finished, back in kernel mode."

### 根本原因
**`vga_set_mode03h()` 的清屏逻辑破坏了 VGA 文本缓冲区内容。**

hello 程序通过 syscall 7 调用 `vga_writestring()` 将输出写入 VGA 文本缓冲区（0xB8000）。当 hello 调用 `exit(0)`（syscall 0）时，内核的 syscall 处理函数调用 `vga_set_mode03h()` 恢复文本模式，该函数末尾有清屏代码（循环写入 0x0720 覆盖 80×25 个字符），直接抹掉了 hello 的所有输出。

随后 `vga_writestring("*** Return to kernel mode ***")` 写入的"Return"消息虽然可见，但 hello 的原始输出已不复存在。

### 解决
1. **移除 `vga_set_mode03h()` 中的清屏操作**：将清屏的责任交给调用者。`vga_set_mode03h()` 只负责 VGA 寄存器恢复和字模恢复，不清除文本缓冲区。
2. **简化 `loader.c` 中的 `run_hello_user()`**：不再调用 `vga_initialize()`（其内部会清屏），而是仅设置光标位置和颜色，保留 VGA 缓冲区的已有内容。

### 涉及文件
- `drivers/interrupts.c`: `vga_set_mode03h()` — 移除末尾的清屏代码
- `kernel/loader.c`: `run_hello_user()` — 用 `vga_set_cursor()` + `vga_set_color()` 替代 `vga_initialize()`

### 验证
运行 `hello` 后能看到完整的输出链：
```
Hello from user mode!
Running in Ring 3 via libc
*** Return to kernel mode ***
Hello program finished, back in kernel mode.
```

---

## 问题16：FAT16 初始化失败 - cluster 数量超出 FAT16 范围

### 现象
`diskinfo` 和 `ls` 命令无法工作，串口日志显示：
```
[FAT16] Cluster count 4060 not in FAT16 range
```

### 根本原因
`mkfat16.py` 默认使用 8 sectors/cluster (4096 bytes)，对于 16MB 磁盘：
- 总扇区 = 32768
- 数据区扇区 = 32768 - 289 = 32479
- cluster 数 = 32479 / 8 = 4060
- FAT16 要求 cluster 数在 4085~65525 范围内

### 解决
将 `BYTES_PER_CLUSTER` 从 4096 (8 sectors) 改为 2048 (4 sectors)：
```python
BYTES_PER_CLUSTER = 2048  # 4 sectors per cluster
```
cluster 数 = 32479 / 4 = 8120，在 FAT16 范围内。

### 重新生成磁盘镜像
```bash
python scripts\mkfat16.py disk.img
```

---

## 问题17：printf 不支持 %02x / %04x 宽度修饰符

### 现象
串口日志和屏幕输出中 `%04x` 等格式不生效，例如：
```
[NE2K] I/O base: 0xc000, IRQ: 11    ← 正确
[NE2K] MAC: 525400123456:...          ← %02x 没补零，连在一起
```

### 根本原因
`vsprintf_internal()` 直接跳到 format specifier，没有解析 `%` 后面的宽度和填充标志。

### 解决
在 `lib/stdio.c` 中添加宽度和零填充解析：
```c
fmt++; // skip '%'
int pad_zero = 0;
int width = 0;
if (*fmt == '0') { pad_zero = 1; fmt++; }
while (*fmt >= '0' && *fmt <= '9') {
    width = width * 10 + (*fmt - '0');
    fmt++;
}
```
并在 `%x`, `%X`, `%d`, `%u` case 中应用 padding。

---

## 问题18：NE2000 网卡无法接收数据包 (IRQ 11 不触发)

### 现象
`ping 10.0.2.2` 发送 ARP 请求后没有回复，串口日志显示：
```
[ARP] Sending request for 10.0.2.2
[ARP] Sending request for 10.0.2.2   ← 重复发送，无回复
```

### 根本原因
**PIC cascade (IRQ 2) 被屏蔽。**

IRQ 11 在从 PIC (IRQ 8-15) 上。从 PIC 的中断通过 IRQ 2 转发到主 PIC。
但 PIC 初始化时 `outb(0x21, 0xFF)` 屏蔽了主 PIC 的所有中断，包括 IRQ 2。
结果：从 PIC 的中断永远无法到达 CPU。

### 解决
在 `pic_initialize()` 中，将主 PIC 的初始屏蔽值从 `0xFF` 改为 `0xFB`：
```c
// Bit 2 = 0: unmask IRQ 2 (cascade), needed for slave PIC (IRQ 8-15)
outb(0x21, 0xFB);  // 0xFB = 11111011
outb(0xA1, 0xFF);
```

### 同时修复的问题
- NE2000 RCR 从 0x04 改为 0x14 (PRO + AB)，确保 promiscuous + broadcast 接收
- RX_STOP 从 0x60 改为 0x80，充分利用 QEMU NE2000 的 32KB 内存

### 经验教训
> 使用从 PIC (IRQ 8-15) 的设备时，必须确保主 PIC 的 IRQ 2 (cascade) 已解除屏蔽。
> 这是 NE2000 (IRQ 11)、RTC (IRQ 8)、PS/2 鼠标 (IRQ 12) 等设备的共同要求。

---

## 问题19：NE2000 PCI I/O Space 未启用（信号被静默丢弃）

### 现象
NE2000 初始化完成后发送数据包无任何回应，QEMU 侧完全收不到以太网帧。
串口日志显示 NE2000 内部寄存器写入正常，但数据实际未到达 PCI 总线。

### 根本原因
**PCI Command Register 的 I/O Space Enable 位（bit 0）未置位。**

QEMU 模拟的 NE2000 在 PCI 配置空间复位后，I/O 地址解码默认关闭。
NE2000 驱动仅读取了 BAR0 获取 I/O 基址，但没有写 Command Register 启用 I/O 空间。
结果：所有对 NE2000 I/O 端口的 `outb`/`inb` 写入 NIC 内部寄存器成功（因为 QEMU
NE2000 的 I/O 端口映射到 PCI 配置空间的 BAR 区域，访问不报错），但以太网帧
从未真正被 QEMU 的网络后端处理——信号被 PCI 桥静默丢弃。

### 解决
在读取 BAR0 后，显式设置 PCI Command Register bit 0：
```c
uint32_t pci_cmd = pci_read_config(bus, dev, func, 0x04);
pci_cmd |= 0x00000001;  /* Set bit 0: I/O Space Enable */
pci_write_config(bus, dev, func, 0x04, pci_cmd);
```

### 验证
设置后串口日志应显示：
```
[NE2K] PCI Command Register after = 0x00000001 (bit0=1 means I/O enabled)
```
此后数据包正常收发。

### 经验教训
> PCI 设备的 I/O 和内存地址解码默认关闭，访问 BAR 之前必须先设置 Command Register。
> 不同 PCI 设备行为不同——QEMU NE2000 在未启用 I/O 空间时仍可读写内部寄存器
>（因为 QEMU 的 PCI 层不会阻止对 BAR 范围内端口的访问），但数据不会真正发送出去。
> 这是一个"静默失败"的典型陷阱。

---

## 问题20：NE2000 接收环形缓冲区处理错误

### 现象
NE2000 初始化正常，MAC 地址正确，IRQ 11 触发正常，但收到的数据包内容
为乱码或无法解析，ARP 回复到达后内核无法识别。

### 根本原因
**三处逻辑错误叠加导致接收完全失效：**

#### 1. 包起始地址错误（BNDRY vs BNDRY+1）
NE2000 的接收环形缓冲区的约定：BNDRY 寄存器指向**最后一个已读的页面**，
下一个包从 **BNDRY + 1 页面**开始。原始代码直接从 BNDRY 页面读取包头，
读到的 4 字节是上一个包的末尾数据，而非当前包的 RSR/NextPage/Length。

```c
// 错误：从 BNDRY 页读取
uint16_t hdr_addr = (uint16_t)bnry * NE2K_PAGE_SIZE;

// 正确：从 BNDRY + 1 页读取
uint8_t page = bnry + 1;
uint16_t hdr_addr = (uint16_t)page * NE2K_PAGE_SIZE;
```

#### 2. 16-bit DMA 模式下的头部解析错误
DCR 配置为 `0x49`（word-wide DMA, little-endian）。在 16-bit 模式下，
NIC 内存的每个 16-bit word 传输一次。MAC 地址在 NIC 内存中也是按 word
存储的（每个字节占一个 word 的低 8 位）。对于接收头部的 4 个逻辑字节
（RSR, NextPage, LengthLo, LengthHi），它们在 16-bit DMA 读取时占 2 个 word：
- Word 0 = [NextPage | RSR]（低字节是 RSR，高字节是 NextPage）
- Word 1 = [LengthHi | LengthLo]

原始代码错误地读取了 8 字节（4 个 word），且没有正确处理 word 到字节的映射。
修正后直接用 `ne2k_rx_header_t` 结构体读取 4 字节（2 个 word），
`ne2k_dma_read` 已针对 16-bit 模式实现正确的字节序转换。

#### 3. 包长度计算和 BNDRY 更新错误
NE2000 报告的包长度包含 4 字节头部，读取数据时需要减去 4。
而且 BNDRY 应更新为 `next_page - 1`（标记下一页之前的所有页面为已读），
而非 `next_page - 1` 的另一种错误计算。

```c
// 长度计算：减去 4 字节头部
uint16_t data_len = hdr.length - 4;

// BNDRY 更新：next_page - 1
bnry = hdr.next_page - 1;
if (bnry < NE2K_RX_START) bnry = NE2K_RX_STOP - 1;
ne2k_out(NE2K_BNDRY, bnry);
```

### 验证
修复后串口日志显示正确的包头解析：
```
[NE2K]   RX page=0x47: rsr=0x01 next=0x48 pkt_len=64
[NE2K]   Delivering packet: data_len=60
```

### 涉及文件
- `drivers/ne2000.c`: `ne2k_process_rx()` 全部重写

---

## 问题21：NE2000 发送后无回复（ARP 轮询缺失）

### 现象
ARP 请求已正确发送（QEMU 侧可抓到），但 TinyOS 不等待回复就直接放弃。

### 根本原因
原始 `ping` 命令在 `net_send_icmp_echo()` 返回 -1（ARP 表未命中）后，
仅打印 "ARP pending, try again..."，要求用户手动重新执行命令。
没有自动轮询接收环形缓冲区来检查 ARP 回复是否到达。

### 解决
在 Shell 的 ping/send 命令中添加 ARP 自动轮询逻辑：
```c
if (ret < 0) {
    uint32_t start = timer_get_ticks();
    while (timer_get_ticks() - start < 10) {  /* ~200ms at 50Hz */
        ne2000_poll_recv();     /* 轮询接收缓冲区 */
        ret = net_send_icmp_echo(target_ip, 1, 1);
        if (ret >= 0) break;    /* ARP 解析成功 */
    }
}
```

同时新增 `ne2000_poll_recv()` 函数，在无需 IRQ 触发的情况下
直接检查 ISR 的 PRX 位并处理接收环形缓冲区中的数据包。

### 涉及文件
- `drivers/ne2000.c`: 新增 `ne2000_poll_recv()`
- `include/ne2000.h`: 添加函数声明
- `kernel/shell.c`: ping/send 命令添加 ARP 轮询等待

---

## 搁置问题：gfxsnake 信息区显示乱码

### 现象
运行 `gfxsnake` 后游戏可正常游玩，但屏幕上方的信息区（状态栏）显示乱码，而非预期的文字（如 "SNAKE" 标题、分数等）。

### 根本原因
待排查。可能原因：
- 像素字体渲染函数（`draw_digit`、`draw_number`、`draw_status`）中使用的位图数据在 `uint16_t` 转换时溢出。原始代码使用 17 位二进制字面量（如 `0b11101010101010111`），赋值给 `uint16_t` 时高位被截断，导致数字渲染数据错误。
- DAC 调色板索引与实际使用的颜色值不匹配，导致像素颜色显示异常。

### 状态
搁置，待后续修复。gfxsnake 整体可玩，不影响核心功能。

---

## 搁置问题：退出 gfxsnake 后无法再次进入

### 现象
第一次运行 `gfxsnake` 正常，退出后再次输入 `gfxsnake` 输出"用户栈分配失败"（`pmm_alloc_page()` 返回 NULL）。

### 状态
搁置，待排查。可能原因：
- 用户程序栈页面未正确释放（`pmm_free_page` 后 PMM 位图状态不一致）
- 物理内存管理器的位图分配/释放逻辑问题

---

## 问题22：串口 Shell 输入无响应（FIFO 触发级别过高）

### 现象
串口模式下（`make run-serial`）可以看到启动日志和提示符，但键盘输入无任何反应。

### 根本原因
UART 16550 FIFO 配置的**中断触发级别**设为 14 字节（`FCR = 0xC7`，bits 7-6 = 11）。用户输入的字符少于 14 个时，FIFO 不会触发 IRQ 4，直到积累到 14 字节或超时。

### 解决
将 FCR 改为 `0x07`（bits 7-6 = 00），触发级别降至 **1 字节**，每个字符立刻触发中断：
```c
outb(0x3FA, 0x07);   // FCR: enable FIFO, clear, trigger at 1 byte
```

### 验证
串口 Shell 即时响应输入的每个字符。

---

## 问题23：串口终端按 Enter 无效（CR vs LF 差异）

### 现象
串口模式下输入命令后按回车无反应，命令不执行。

### 根本原因
串口终端按下 Enter 时发送的是 `\r`（CR, ASCII 13），但 `shell_char_callback` 只检查 `c == '\n'`（LF, ASCII 10）。`\r` 的值（13 < 32）落在 `else if (c >= 32)` 条件之外，被**静默丢弃**。

键盘驱动（PS/2）按 Enter 发送的是 `\n`（由 `keyboard.c` 翻译），所以 VGA 模式下无此问题。

### 解决
在 `shell_char_callback` 中将 `\r` 视同为行终止符：
```c
if (c == '\n' || c == '\r') {
```

### 经验教训
> 串口终端使用 CR (`\r`) 作为行终止符，而 Unix 风格使用 LF (`\n`)。
> 实现双终端（VGA 键盘 + 串口）时，Shell 必须同时处理两种行终止符。

---

## 问题24：`make run` 模式下终端无法输入

### 现象
`make run` 启动后，在启动 QEMU 的终端中输入无反应。

### 根本原因
`make run` 添加了 `-serial file:logs/serial.log` 参数，串口被设为**只写文件模式**，不接收终端输入。

### 解决
将 `make run` 恢复为无 `-serial` 参数（VGA 窗口 + PS/2 键盘），新增 `make run-debug` 用于串口日志记录：

| 命令 | 模式 | 串口 | 输入方式 |
|------|------|------|----------|
| `make run` | VGA 窗口 | 无 | QEMU 窗口键盘 |
| `make run-debug` | VGA 窗口 | 写入文件 | QEMU 窗口键盘 |
| `make run-serial` | 纯终端 | 连接到终端 | 终端键盘 |

### 涉及文件
- `Makefile`: `run` / `run-debug` / `run-serial` 三个独立目标