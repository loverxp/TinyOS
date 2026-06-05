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
- **ATA PIO 磁盘驱动**（主 IDE 通道，28-bit LBA，读扇区 / IDENTIFY）
- **FAT16 文件系统**（只读挂载，BPB 解析，目录列表，文件读取，cluster 链遍历）
- **PCI 总线扫描**（配置空间读取，vendor/device ID，BAR，IRQ line）
- **NE2000 网卡驱动**（PCI 发现，远程 DMA 读写，接收环形缓冲区，IRQ 处理）
- **网络协议栈**（ARP 请求/应答，IPv4，ICMP Echo，UDP 收发）✅ 已验证通过
- **printf 增强**（支持 `%02x` `%04x` 等宽度和零填充修饰符）
- **串口 Shell**（COM1 中断驱动收发，与 VGA 键盘双终端并行）

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
- [x] **鼠标驱动**
  - PS/2 鼠标支持（IRQ12）
  - 解析鼠标数据包（位移、按键）
- [ ] **PCI 总线枚举**
  - 扫描 PCI 配置空间
  - 发现并列出所有 PCI 设备
- [ ] **PRNG (伪随机数生成器)**
  - 实现 xorshift 或 LFSR 算法
  - 为内核提供随机数服务
- [x] ~~**串口 Shell**~~（已完成）
  - 启用 COM1 串口 RX 中断
  - 串口字符回调 → Shell 输入处理（与键盘共享同一输入管道）
  - 实现通过串口发送命令并接收回显，无需 VGA 显示
  - 双终端并行：VGA 键盘 + 串口同时可用

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
  - 标记 FINISHED + 从循环链表摘除
  - 栈页在下次 prepare_switch() 时延迟释放（IRQ 安全）
  - trampoline 压入 task_exit 作为返回地址安全网

- [ ] **待完善**
  - `task_exit()` 直接构造帧并调用 do_switch（跳过最后一次 IRQ 浪费）

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

## 第三阶段：文件系统 (v0.6 - v0.7) ✅ 部分完成

### 3.1 FAT16 文件系统（已实现，只读）

- [x] **ATA PIO 驱动** (`drivers/ata.c`)
  - 主 IDE 通道 (0x1F0)，28-bit LBA 寻址
  - `ata_init()` 检测磁盘，`ata_identify()` 读取设备信息
  - `ata_read_sectors()` 读扇区，支持多扇区连续读

- [x] **FAT16 解析器** (`kernel/fat16.c`)
  - BPB (BIOS Parameter Block) 解析与验证
  - 根目录扫描（8.3 文件名，LFN 跳过）
  - 文件查找 (`fat16_find`) + 读取 (`fat16_read`)
  - Cluster 链遍历，支持跨 cluster 读取
  - Shell 命令：`ls`（目录列表）、`cat`（文件内容）、`diskinfo`（磁盘信息）

- [x] **磁盘镜像生成** (`scripts/mkfat16.py`)
  - 生成 16MB FAT16 镜像（4 sectors/cluster, 8120 clusters）
  - 内含示例文件：readme.txt, hello.c, config.txt, test.txt

- [ ] **写支持**（FAT 表更新、文件创建/删除）
- [ ] **子目录支持**

### 3.2 块设备驱动

- [x] IDE 硬盘驱动（ATA PIO 模式）
- [ ] DMA 模式（性能优化）
- [ ] 磁盘分区支持

### 3.3 高级文件系统

- [ ] **VFS 虚拟文件系统层**
- [ ] **Ext2** - 类 Unix 文件系统

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
- [x] 内建命令：`help`, `clear`, `uptime`, `meminfo`, `alloc`, `free`, `except`, `kmtest`, `echo`, `testuser`, `runuser`, `ls`, `cat`, `diskinfo`, `pci`, `net`, `ping`, `send`
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

## 第五阶段：网络与高级功能 (v1.0+) ✅ 网络部分完成

### 5.1 网络协议栈（已实现基础功能）

- [x] **NE2000 网卡驱动** (`drivers/ne2000.c`)
  - PCI 自动发现（vendor=0x10EC, device=0x8029）
  - 远程 DMA 读写（NIC 内存访问）
  - 接收环形缓冲区（page 0x46~0x80，32KB NIC 内存）
  - IRQ 处理（IRQ 11，slave PIC via cascade IRQ 2）
  - MAC 地址读取，promiscuous + broadcast 接收

- [x] **PCI 总线扫描** (`drivers/pci.c`)
  - 配置空间读写（CF8h/CFC h）
  - Vendor/Device ID、BAR、IRQ line 读取
  - Shell 命令：`pci`（列出所有 PCI 设备）

- [x] **ARP 协议** (`kernel/net.c`)
  - ARP 请求发送 + 应答处理
  - 16 项 ARP 缓存表
  - 自动学习发送方 MAC

- [x] **IPv4**
  - IP 头部构建、校验和计算
  - 子网掩码 + 网关路由（直接 vs 网关转发）

- [x] **ICMP**
  - Echo Request 发送 / Echo Reply 应答
  - Shell 命令：`ping <ip>`

- [x] **UDP**
  - UDP 数据报发送/接收
  - Shell 命令：`send <ip> <port> <msg>`

### 5.1a 网络功能增强（近期待办）

- [ ] **UDP 接收命令** — 添加 `recv <port>` 命令，TinyOS 可监听 UDP 端口接收主机数据
- [ ] **ARP 缓存管理** — 添加 `arp` 命令显示/清空 ARP 缓存表
- [ ] **网络统计信息** — 添加 `netstat` 命令显示收发统计（发送/接收包数、错误数）
- [ ] **Shell 命令输出优化** — 移除 ping/send 的 `[1/4]` 调试输出，仅在出错时显示诊断信息
- [ ] **DHCP 客户端** — 自动获取 IP/网关/掩码，替代硬编码 10.0.2.15
- [ ] **DNS 解析** — 支持域名到 IP 的解析（查询 10.0.2.3）
- [ ] **TCP 协议栈**
  - 三次握手（SYN → SYN-ACK → ACK），连接状态机
  - 序列号/确认号管理 + 校验和（复用 IP 校验和函数）
  - 超时重传（RTO 定时器）
  - 四次挥手（FIN 处理 + TIME_WAIT）
  - 简易连接表（支持 4-8 个并发连接）
- [ ] **Socket 抽象层**
  - `tcp_listen(port)` / `tcp_accept()` / `tcp_recv()` / `tcp_send()` / `tcp_close()`
  - 阻塞等待 + 与调度器协作（recv 阻塞时让出 CPU）
- [ ] **HTTP 客户端** — 基于 TCP 实现 GET 请求，获取网页内容
- [ ] **Web 服务器** — 基于 TCP + HTTP 提供静态文件服务
  - 解析 HTTP 请求行（GET /path HTTP/1.1）和请求头
  - 从 FAT16 磁盘读取文件，拼装 HTTP 响应（状态行 + Content-Type + Content-Length）
  - 支持 200 OK / 404 Not Found 响应
  - 并发连接处理（调度器 + 多任务）
- [ ] **SSH 服务器** — 基于 TCP 的加密远程 Shell
  - 需要 TCP + 熵源（PRNG）+ 加密算法（简易 AES 或 XOR 流密码）
  - 用户名/密码认证
  - 远程 Shell 交互（串口 Shell 的复用）

### 5.2 网络使用指南

**QEMU 网络配置**（已在 Makefile 中配置）：
```
-netdev user,id=net0,hostfwd=udp::8888-:8888
-device ne2k_pci,netdev=net0
```

**IP 配置**（QEMU user-mode networking）：
| 角色 | IP | 说明 |
|------|------|------|
| 虚拟机 (guest) | 10.0.2.15 | TinyOS 固定 IP |
| 网关 (gateway) | 10.0.2.2 | QEMU 内置 NAT 网关 |
| DNS | 10.0.2.3 | QEMU 内置 DNS |
| 主机 (host) | 10.0.2.2 | 通过网关访问 |

**Windows 上接收 UDP 消息**（替代 netcat）：
```powershell
# PowerShell UDP 监听
$udp = New-Object System.Net.Sockets.UdpClient(8888)
$remote = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
$data = $udp.Receive([ref]$remote)
[System.Text.Encoding]::UTF8.GetString($data)
$udp.Close()
```

**TinyOS 端发送**：
```
TinyOS> send 10.0.2.2 8888 Hello from TinyOS!
```

### 5.3 图形界面
**目标**：图形用户界面 (GUI)

- [x] **VGA 图形模式** (Mode 13h, 320×200, 256 色)
  - 寄存器编程（Sequencer / CRTC / Graphics Controller / Attribute Controller）
  - DAC 调色板初始化
  - 字模保存与恢复（图形↔文本模式切换）

- [x] **显卡驱动（VBE/VESA）**
  - Bochs VBE 接口（I/O 端口编程），检测 + 模式设置 + LFB 启用
  - 支持高分辨率（800×600、1024×768 等），32 位真彩色
  - 线性帧缓冲（LFB）通过分页机制映射到内核虚拟空间
  - 与文本模式可共存（VBE 禁用后返回文本模式）

- [x] **窗口系统**
  - 帧缓冲抽象层：putpixel、fillrect、drawrect、16×16 字体渲染
  - PS/2 鼠标驱动（IRQ12），支持移动、点击事件
  - 窗口管理器：窗口创建/关闭/移动/缩放/焦点/Z-order 管理
  - 标题栏绘制、关闭按钮、窗口拖拽
  - 事件驱动：鼠标回调分发、键盘事件分发到焦点窗口

- [x] **贪吃蛇游戏**（VGA 图形模式）

- [x] **PCI 总线枚举**（已实现，`pci` 命令）

#### 已知问题
- **退出 GUI 后键盘可能无响应**: 从 `cmd_gui` 的清零代码返回到 Shell 后，偶尔键盘输入不被处理。推测原因：`shell_char_callback` 重注册时机与键盘中断存在竞态，或 `enable_interrupts()` 前后键盘控制器状态不一致
- **首次划入 QEMU 窗口鼠标位置不正确**: QEMU 窗口激活后，PS/2 鼠标的第一个数据包中的位移值异常（可能包含累积的初始状态），导致光标瞬间跳到错误位置。后续移动恢复正常

### 5.4 高级功能

- [ ] 动态链接器
- [ ] 多核/SMP 支持
- [ ] USB 驱动
- [ ] 声音驱动

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