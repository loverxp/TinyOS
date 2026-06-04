# TinyOS 演化路线图

## 当前状态 (v0.1)

✅ 已完成：
- Multiboot 引导
- VGA 文本显示
- 键盘输入（中断驱动）
- 定时器中断
- IDT/PIC 中断管理
- 串口调试输出
- 物理内存管理（PMM，位图式页帧分配器）
- 堆内存分配器（kmalloc/kfree，块式管理）
- 异常处理（除零、GPF 等 CPU 异常捕获与显示）
- 用户态异常捕获与指令跳过
- GDT（内核段 + 用户代码段 + 用户数据段 + TSS 段）
- TSS（任务状态段，支持 Ring 3→Ring 0 中断栈切换）
- 系统调用（int 0x80，Ring 3→Ring 0 特权级切换）
- 用户态（Ring 3）/ 内核态（Ring 0）切换
- 交互式 Shell（多命令支持）
- VGA 状态栏（系统运行时间、堆使用量、空闲内存）
- 抢占式多任务调度器（Round-Robin，IRQ0 驱动，`schedtest` 命令验证）

---

## 第零阶段：开发者体验 (v0.15 - v0.19)

### 0.1 构建与调试工具
**目标**：提升开发效率，建立稳定的调试基础设施

- [ ] **Makefile / 构建系统**
  - 支持增量编译（只重新编译修改过的文件）
  - 自动处理文件依赖关系
  - 简化新文件的添加流程
- [ ] **GDB 内核调试集成**
  - 通过 QEMU `-s -S` 连接 GDB
  - 支持源码级断点调试
  - `.gdbinit` 配置脚本
- [ ] **统一调试输出框架**
  - 实现 `debug_printf()` 替代分散在各文件的 `serial_write`
  - 支持调试级别（ERROR/WARN/INFO/DEBUG）
  - 开关控制编译时是否包含调试输出
- [ ] **内核异常回溯 (backtrace)**
  - 异常发生时打印栈回溯信息
  - 显示调用链（EIP + 函数名）
  - 辅助定位故障位置

### 0.2 额外设备驱动
**目标**：支持更多基础硬件

- [ ] **RTC 驱动**
  - 读取 CMOS 实时时钟
  - 初始设置系统时间（而非从 0 开始计时）
- [ ] **鼠标驱动**
  - PS/2 鼠标支持（IRQ12）
  - 解析鼠标数据包（位移、按键）
- [ ] **PCI 总线枚举**
  - 扫描 PCI 配置空间
  - 发现并列出所有 PCI 设备
- [ ] **PRNG (伪随机数生成器)**
  - 实现 xorshift 或 LFSR 算法
  - 为内核提供随机数服务

---

## 第一阶段：核心基础设施 (v0.2 - v0.3)

### 1.1 内存管理
**目标**：实现物理内存和虚拟内存管理

- [x] **物理内存检测**
  - 通过 BIOS/GRUB 获取内存映射 (E820)
  - 实现内存分配器（位图）
  
- [x] **分页机制**
  - 启用 CR3 页目录
  - 4KB 页框管理
  - identity map 前 8MB 物理内存
  
- [x] **内核堆**
  - `kmalloc()` / `kfree()` 实现
  - 基于块式管理（含分割与合并）

**技术要点**：
```c
// 页目录项结构
struct page_directory_entry {
    uint32_t present    : 1;   // 是否存在
    uint32_t rw         : 1;   // 读写权限
    uint32_t user       : 1;   // 用户/内核
    uint32_t accessed   : 1;   // 是否访问过
    uint32_t dirty      : 1;   // 是否修改过
    uint32_t frame      : 20;  // 物理页框地址
};
```

### 1.2 异常处理完善
**目标**：处理所有 CPU 异常，提供有用的错误信息

- [x] 页故障处理 (Page Fault)
- [x] 通用保护故障 (GPF)
- [x] 除零错误
- [x] 用户态异常捕获与指令跳过
- [ ] 堆栈溢出检测
- [ ] 蓝屏/崩溃信息

### 1.3 字符串格式化输出
**目标**：支持 `printf` 风格格式化

- [x] `sprintf()` / `printf()` 实现
- [x] 支持 `%d`, `%x`, `%s`, `%c`, `%p`
- [x] 格式化数字显示（十进制/十六进制）

---

## 第二阶段：多任务 (v0.4 - v0.5)

### 2.1 进程/线程管理
**目标**：实现基本的抢占式多任务

- [x] **任务控制块 (TCB)**
  - 循环链表 + 状态机 (READY/RUNNING/BLOCKED/FINISHED)
  - 栈上伪造 IRQ 帧，无需单独保存 EBP/EIP

- [x] **上下文切换**
  - `prepare_switch()` (C) + `do_switch()` (汇编) + `task_trampoline`
  - 基于 IRQ0 定时器中断抢占
  - 内核线程共享页目录（无 CR3 切换）

- [x] **Round-Robin 调度器**
  - 时间片轮转 (每个 timer tick 触发调度)
  - idle 任务作为链表节点参与轮换
  - `schedtest [N]` 命令验证（N 秒后自动停止）

- [x] **task_exit()**
  - 标记 FINISHED，等待下次 IRQ 切走
  - 未释放栈内存、未从链表移除（已知限制）

- [ ] **待完善**
  - `task_exit()` 直接触发调度（避免死亡任务浪费 IRQ）
  - FINISHED 任务从循环链表移除 + 释放栈页
  - 任务函数意外返回的保护

- [ ] **系统调用**
  - `fork()` - 创建进程
  - `yield()` - 主动让出 CPU
  - `sleep()` - 睡眠等待

### 2.2 进程间通信 (IPC)
**目标**：进程间数据交换

- [ ] 管道 (Pipe)
- [ ] 消息队列
- [ ] 共享内存（需要分页支持）

---

## 第三阶段：文件系统 (v0.6 - v0.7)

### 3.1 虚拟文件系统 (VFS)
**目标**：抽象文件系统接口

```c
struct vfs_node {
    char name[256];
    uint32_t type;          // FILE, DIRECTORY, DEVICE
    uint32_t size;
    uint32_t permissions;
    
    // 操作函数指针
    uint32_t (*read)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    void (*open)(struct vfs_node* node);
    void (*close)(struct vfs_node* node);
    struct vfs_node* (*finddir)(struct vfs_node* node, char* name);
};
```

### 3.2 简单文件系统
**目标**：实现一个基本的文件系统

- [ ] **RAMFS** - 内存文件系统（最简单）
- [ ] **FAT12/16** - 兼容 DOS 文件系统
- [ ] **Ext2** - 类 Unix 文件系统（高级）

### 3.3 块设备驱动
**目标**：读写磁盘

- [ ] IDE 硬盘驱动
- [ ] ATA PIO 模式
- [ ] 磁盘分区支持

---

## 第四阶段：用户空间 (v0.8 - v0.9)

### 4.1 用户模式
**目标**：从内核模式切换到用户模式运行外部程序

- [x] **特权级切换**
  - Ring 0 (内核) -> Ring 3 (用户)
  - 系统调用门 (int 0x80, DPL=3)
  - 用户态异常捕获与指令跳过

- [x] **用户程序运行时 (crt0)**
  - 编写 `crt0.s` 启动代码：`_start → main → exit`
  - 清理 BSS 段
  - 传递 argc/argv

- [x] **用户程序链接脚本**
  - `user.ld`：定义用户程序内存布局
  - 入口点 `_start`，基址 `0x400000`（4MB）
  - `-ffreestanding -nostdlib` 编译选项

- [x] **简易二进制加载（第一阶段）**
  - 用户程序编译为纯二进制 (`objcopy -O binary`)
  - 内核直接加载到固定地址执行
  - 绕过 ELF 解析复杂度，快速验证流程

- [x] **ELF 加载器（第二阶段）**
  - 校验 ELF magic (`0x7F E L F`)
  - 解析 Program Header，映射 LOAD 段
  - 创建用户栈，跳转到 `e_entry`
  - `elf_load(elf_data, size)` 接口

- [x] **加载用户程序到内存**
  - 从嵌入的 ELF 二进制加载用户程序
  - 通过 IRET 进入 Ring 3 执行
  - hello 和 gfxsnake 均已使用 ELF 加载器

### 4.2 Shell
**目标**：命令行解释器

- [x] 命令解析
- [x] 内建命令：`help`, `clear`, `uptime`, `meminfo`, `alloc`, `free`, `except`, `kmtest`, `echo`, `testuser`, `runuser`
- [ ] 程序执行：`fork` + `exec`
- [ ] 管道支持：`cmd1 | cmd2`

### 4.3 用户态标准库 (libc)
**目标**：提供基础 C 运行时，用户程序无需关心内核细节

- [x] **系统调用封装**
  - `exit(code)` - 通过 syscall 0 退出
  - `printf(fmt, ...)` - 通过 syscall 7 输出到控制台
- [x] **字符串处理**
  - `strlen`, `strcpy`, `strcmp`, `strcat`
  - `memcpy`, `memset`, `memcmp`
- [x] **格式化输出**
  - `printf(fmt, ...)` - 基于 syscall 7 实现
  - `sprintf(buf, fmt, ...)` - 格式化到字符串
  - 支持 `%d`, `%x`, `%s`, `%c`, `%p`
- [ ] **待扩展**
  - `malloc(size)` / `free(ptr)` - 堆分配器
  - `write(fd, buf, len)` - 文件 I/O
  - `scanf` - 格式化输入

---

## 第五阶段：网络与高级功能 (v1.0+)

### 5.1 网络协议栈
**目标**：实现 TCP/IP 协议栈

- [ ] **网卡驱动**
  - RTL8139 或 E1000 驱动
  - QEMU 虚拟网卡支持

- [ ] **网络层**
  - ARP 协议
  - IP 协议
  - ICMP (ping)

- [ ] **传输层**
  - UDP
  - TCP（简化版）

- [ ] **应用层**
  - DHCP 客户端
  - DNS 解析
  - HTTP 客户端/服务器

### 5.2 图形界面
**目标**：图形用户界面 (GUI)

- [x] **VGA 图形模式**
  - 320x200x256 (Mode 13h)
  - 或 VESA 高分辨率

- [ ] **窗口系统**
  - 窗口管理器
  - 事件驱动（鼠标、键盘）
  - 基本控件（按钮、标签、文本框）

- [x] **贪吃蛇游戏**（VGA 图形模式）

### 5.3 高级功能

- [ ] **动态链接器**
- [ ] **多核/SMP 支持**
- [ ] **USB 驱动**
- [ ] **声音驱动**
- [ ] **PCI 总线枚举**

---

## 技术演进路径

```
v0.1 ──> v0.15 ──> v0.2 ──> v0.3 ──> v0.4 ──> v0.5 ──> v0.6 ──> v0.7 ──> v0.8 ──> v0.9 ──> v1.0
 │        │         │         │         │         │         │         │         │         │
 ▼        ▼         ▼         ▼         ▼         ▼         ▼         ▼         ▼         ▼
引导    构建系统   内存管理   异常处理   多任务✓   调度器✓   VFS      用户态    网络
VGA     GDB调试   分页机制   printf    TCB✓     系统调用   RAMFS    crt0      GUI
键盘    调试框架   堆分配器  蓝屏      上下文✓   同步     FAT12    libc      TCP/IP
定时器   RTC驱动              栈回溯    IPC       锁      IDE       ELF加载  信号
用户态   鼠标                            Spinlock           二进制加载  ACPI
系统调用  PCI                              Mutex               用户程序   SMP
```

---

## 每个阶段的检查清单

### 完成标准

| 阶段 | 必须完成 | 验证方法 |
|------|----------|----------|
| v0.15 | Makefile + GDB 调试 | `make qemu-gdb` 断点命中 |
| v0.2 | kmalloc/kfree 工作 | 分配内存并读写测试 |
| v0.3 | 页故障正确处理 | 访问无效地址触发蓝屏 |
| v0.4 | 两个任务交替运行 | `schedtest` 命令验证 A B 交替 |
| v0.5 | 系统调用正常工作 | 用户程序调用 `write()` |
| v0.6 | 文件读写正常 | `echo hello > file.txt` |
| v0.7 | 磁盘分区可挂载 | `mount /dev/hda1 /mnt` |
| v0.8 | 用户程序运行 | 编译并运行 "Hello World" |
| v0.9 | Shell 交互正常 | 输入命令得到正确输出 |
| v1.0 | 网络连通 | `ping 8.8.8.8` 成功 |

---

## 参考资源

- [OSDev Wiki](https://wiki.osdev.org/) - 操作系统开发百科
- [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) - Intel 开发者手册
- [JamesM's Kernel Tutorial](https://archive.org/details/jamesm-kernel-tutorial) - 经典内核教程
- [Linux 0.01 源码](https://www.kernel.org/pub/linux/kernel/Historic/) - 最早期 Linux 源码