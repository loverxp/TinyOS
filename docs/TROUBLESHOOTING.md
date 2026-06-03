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