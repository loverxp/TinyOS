# TinyOS 演化路线图

## 当前状态 (v0.2)

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
- **TCP 协议栈**（三次握手/四次挥手，连接状态机，简易连接表）
- **HTTP WebServer**（端口 80，基于 TCP，响应 HTML 页面）
- **printf 增强**（支持 `%02x` `%04x` 等宽度和零填充修饰符）
- **串口 Shell**（COM1 中断驱动收发，与 VGA 键盘双终端并行）
- **RTC 实时时钟驱动**（CMOS，BCD/二进制自动检测，`date` 命令）
- **PRNG**（xorshift32，`prng_seed`/`prng_next`/`prng_range`，`rand` 命令）
- **调试框架**（`kprintf` 四级日志，内核异常栈回溯 `kernel_backtrace`）
- **网络命令**（`arp` 显示/清空缓存，`netstat` 收发统计，`recv <port>` UDP 监听）
- **Shell 输出优化**（ping/send 移除 `[1/4]` 调试噪声，仅显示关键状态）
- **yield/sleep 系统调用**（`task_yield()`/`task_sleep(ms)`，syscall 8/9，协作式多任务基础）
- **FAT16 写支持**（`ata_write_sectors`，文件创建/覆写/删除，FAT 链分配/释放，`write`/`rm` 命令）
- **DHCP 客户端**（Discover/Offer/Request/ACK 四步协商，`dhcp` 命令，动态获取 IP/网关/掩码）
- **TCP 监听命令**（`tcp-recv <port>` 10秒监听，显示接收数据）
- **Socket 抽象层**（`sock_create`/`bind`/`connect`/`send`/`recv`/`listen`/`close`，UDP+TCP 统一接口，环形接收缓冲区）
- [x] **进程间通信 (IPC)**（Pipe 字节流管道、Message Queue 消息队列、Shared Memory 共享内存，`ipctest` 命令验证，syscall 10-20）
- [x] **fork/exec 系统调用**（syscall 25 fork / syscall 26 exec，TCB 扩展 `is_forked` 标志，fork 时克隆内核栈和用户栈，exec 替换当前进程映像）
- [x] **Shell 管道**（`cmd1 | cmd2` 解析与执行，通过 Pipe 重定向子进程 stdin/stdout）
- [x] **forktest 用户程序**（验证 fork/exec/yield/exit 流程）

---

## 第零阶段：开发者体验 (v0.15 - v0.19)

### 0.1 构建与调试工具
**目标**：提升开发效率，建立稳定的调试基础设施

- [ ] **Makefile / 构建系统**
  - 支持增量编译（只重新编译修改过的文件）
  - 自动处理文件依赖关系
  - 简化新文件的添加流程
- [x] **GDB 内核调试集成**
  - 通过 QEMU `-s -S` 连接 GDB
  - 支持源码级断点调试
  - `.gdbinit` 配置脚本
- [x] **统一调试输出框架**
  - 实现 `kprintf()` 替代分散在各文件的 `serial_write`
  - 支持调试级别（ERROR/WARN/INFO/DEBUG）
  - 开关控制编译时是否包含调试输出
- [x] **内核异常回溯 (backtrace)**
  - 异常发生时打印栈回溯信息
  - 显示调用链（EIP + 函数名）
  - 辅助定位故障位置

### 0.2 额外设备驱动
**目标**：支持更多基础硬件

- [x] **RTC 驱动**
  - 读取 CMOS 实时时钟
  - BCD/二进制模式自动检测
  - `date` 命令显示当前日期时间
- [x] **鼠标驱动**
  - PS/2 鼠标支持（IRQ12）
  - 解析鼠标数据包（位移、按键）
- [x] **PCI 总线枚举**
  - 扫描 PCI 配置空间
  - 发现并列出所有 PCI 设备
- [x] **PRNG (伪随机数生成器)**
  - xorshift32 算法实现 (`lib/prng.c`)
  - `prng_seed()` / `prng_next()` / `prng_range()` 接口
  - Shell 命令: `rand` 显示随机数
- [x] **调试输出框架**
  - `kprintf()` 支持 ERROR/WARN/INFO/DEBUG 四个级别 (`lib/debug.c`)
  - 编译时可通过 `KLOG_LEVEL` 控制输出
  - `KERROR()` / `KWARN()` / `KINFO()` / `KDBG()` 便捷宏
  - 内核异常时自动打印调用栈回溯 (`kernel_backtrace()`)
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
- [x] 支持 `%d`, `%u`, `%x`, `%s`, `%c`, `%p`
- [x] 格式化数字显示（十进制/十六进制/无符号）
- [x] 宽度和零填充修饰符支持（如 `%02u`, `%04x`）

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

- [x] **yield/sleep 系统调用** ✅ 已实现
  - `yield()` - 主动让出 CPU (syscall 8)
  - `sleep(ms)` - 睡眠等待 (syscall 9)，设置 `sleep_deadline`，调度器在 `prepare_switch()` 中自动唤醒

- [x] **信号系统** ✅ 已实现
  - POSIX-like 信号：SIGHUP(1), SIGINT(2), SIGKILL(9), SIGTERM(15)
  - `signal_send(pid, sig)` 发送信号，`signal_check_and_deliver(task)` 投递
  - Ctrl+C 广播 SIGINT 到所有非 idle 任务
  - `prepare_switch()` 遍历全任务数组投递待处理信号
  - 默认动作 TERMINATE 终止任务，支持 SIG_ACTION_IGN 忽略
  - `kill(pid, sig)` 命令 (syscall 33)

### 2.2 进程间通信 (IPC) ✅ 已实现
**目标**：进程间数据交换

- [x] **管道 (Pipe)** (`kernel/ipc.c`)
  - 512 字节环形缓冲区，最多 8 个 pipe
  - `pipe_create()` / `pipe_read()` / `pipe_write()` / `pipe_close()`
  - 满时阻塞写者，空时阻塞读者，写后唤醒读者，读后唤醒写者

- [x] **消息队列 (Message Queue)**
  - 8 个消息槽，每条最大 64 字节，最多 8 个队列
  - `mq_create()` / `mq_send()` / `mq_recv()` / `mq_close()`
  - 消息边界保持（非字节流），FIFO 顺序

- [x] **共享内存 (Shared Memory)**
  - 按名称查找，`pmm_alloc_page()` 分配物理页
  - `shm_create(name, size)` / `shm_open(name)` / `shm_close(name)`
  - 所有任务共享地址空间，返回的指针直接可用

- [x] **IPC 阻塞机制**
  - `task_t` 扩展 `ipc_wait_obj` + `ipc_wait_type` 字段
  - `ipc_block()` 阻塞当前任务，`ipc_wake()` 精确唤醒等待者
  - `scheduler_wake_ipc()` 在调度器内部遍历任务数组

- [x] **系统调用 10-20**（int 0x80）
  - Pipe: create(10) / read(11) / write(12) / close(13)
  - MQ: create(14) / send(15) / recv(16) / close(17)
  - SHM: create(18) / open(19) / close(20)

- [x] **Shell 命令** `ipctest`
  - 三阶段自动化测试：Pipe → MQ → SHM
  - 每阶段创建生产者/消费者任务，验证数据完整性和阻塞/唤醒机制

---

## 第三阶段：文件系统 (v0.6 - v0.7) ✅ 部分完成

### 3.1 FAT16 文件系统（已实现，读写）

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

- [x] **写支持**（FAT 表更新、文件创建/覆写/删除，`write`/`rm` 命令）
- [x] **子目录支持**（路径解析、`mkdir`/`rmdir`，`ls/cat/write/rm` 支持路径参数）

### 3.2 块设备驱动

- [x] IDE 硬盘驱动（ATA PIO 模式）
- [ ] DMA 模式（性能优化）
- [x] **MBR 磁盘分区支持**（`kernel/mbr.c`，读取扇区 0 解析分区表，识别 FAT16 分区类型，`partitions` 命令列出分区信息）

### 3.3 高级文件系统

- [x] **VFS 虚拟文件系统层**（`kernel/vfs.c`，多后端路由架构，16 个 fd，DevFS 后端 `/dev/null` + `/dev/zero`，syscall 28 open / 29 read / 30 write / 31 close / 62 dup2，`filetest` 命令验证）
- [ ] **FAT16 VFS 后端** — 将 FAT16 操作接入 VFS 统一接口
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
- [x] 内建命令：`help`, `clear`, `uptime`, `meminfo`, `alloc`, `free`, `except`, `kmtest`, `echo`, `testuser`, `runuser`, `hello`, `forktest`, `filetest`, `ls [path]`, `cat <path>`, `mkdir <path>`, `rmdir <path>`, `write <path>`, `rm <path>`, `diskinfo`, `pci`, `partitions`, `net`, `ping <ip|hostname>`, `send <ip|hostname>`, `recv`, `arp`, `netstat`, `rand`, `dhcp`, `tcp-recv`, `http-get`, `ipctest`, `schedtest`, `kill`, `webserver`, `dino`, `si`, `tinyhttpd`, `date`, `more`
  - 已迁移到 Ring 3 用户态：`help`, `clear`, `echo`, `hello`, `uptime`, `date`, `rand`, `meminfo`, `diskinfo`, `ls`, `cat`, `more`, `write`, `rm`, `mkdir`, `rmdir`, `ping`, `arp`, `net`, `netstat`, `dhcp`, `pci`, `kill`, `filetest`, `dino`, `si`, `tinyhttpd` (共 28 个，通过 syscall 27 get_system_info 查询系统信息)
- [x] 程序执行：`fork` (syscall 25) + `exec` (syscall 26)
- [x] 管道支持：`cmd1 | cmd2`（基于 Pipe IPC + I/O 重定向）
  - Shell 解析 `|` 分隔的多条命令
  - 为每条命令创建子进程，通过 pipe 连接 stdin/stdout
  - `forktest` 用户程序验证 fork/exec/yield/exit 流程

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
  - 支持 `%d`, `%u`, `%x`, `%s`, `%c`, `%p`
- [x] **待扩展**
  - `malloc(size)` / `free(ptr)` - 堆分配器 ✅ 已实现
  - `write(fd, buf, len)` - 文件 I/O ✅ 已实现（VFS syscall 30）
  - `scanf` - 格式化输入 ✅ 已实现
  - `getchar` / `readline` / `get_cmdline` / `clear_screen` 系统调用 ✅ 已实现
- [x] **POSIX 兼容层**
  - `errno` 全局变量 + 25+ 标准错误码（EPERM~ENOSYS）✅ 已实现
  - `termios` 终端控制 stub（`tcgetattr`/`tcsetattr`/`cfmakeraw`）✅ 已实现
  - `unistd.h` / `fcntl.h` POSIX 风格头文件 ✅ 已实现

#### 近期修复记录
- **`%u` 格式符缺失修复** (2025-06-06):
  - 问题：用户态 libc 的 `vsprintf()` 未实现 `%u` 格式说明符，`uptime`/`date`/`rand`/`meminfo`/`diskinfo` 等命令输出"一堆 u"
  - 原因：`switch` 语句中缺少 `case 'u'`，落到 `default` 分支原样输出 `u`；`%02u` 变成 `02u`
  - 修复：在 [user/libc/stdio.c](user/libc/stdio.c) 中新增 `case 'u'` 分支，实现无符号整数格式化（类似 `%d`，去掉负数处理）
  - 影响范围：用户态 libc 的 printf/sprintf 系列函数均受益，内核态 lib （`lib/stdio.c`）此前已支持 `%u`

---

## 第五阶段：网络与高级功能 (v1.0+) ✅ 网络部分已完成

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

- [x] **TCP**
  - 三次握手/四次挥手，连接状态机（LISTEN/SYN_RCVD/ESTABLISHED/CLOSE_WAIT/LAST_ACK）
  - 序列号/确认号管理，校验和计算
  - `net_tcp_listen()` / `net_tcp_send()` / `net_tcp_close()` API
  - 简易连接表（支持 4 个并发连接）

- [x] **HTTP WebServer**
  - 基于 TCP + HTTP 返回系统信息 HTML 页面
  - Shell 命令：`webserver` / `webserver stop`
  - 通过 QEMU `hostfwd=tcp::8088-:80` 从宿主机访问

### 5.1a 网络功能增强（近期待办）

- [ ] **UDP 接收命令** — 添加 `recv <port>` 命令，TinyOS 可监听 UDP 端口接收主机数据 ✅ 已实现
- [ ] **ARP 缓存管理** — 添加 `arp` 命令显示/清空 ARP 缓存表 ✅ 已实现
- [ ] **网络统计信息** — 添加 `netstat` 命令显示收发统计（发送/接收包数、错误数） ✅ 已实现
- [ ] **Shell 命令输出优化** — 移除 ping/send 的 `[1/4]` 调试输出，仅在出错时显示诊断信息 ✅ 已实现
- [x] **DHCP 客户端** — 自动获取 IP/网关/掩码，替代硬编码 10.0.2.15 ✅ 已实现（`dhcp` 命令）
- [x] **DNS 解析** — 支持域名到 IP 的解析（查询 10.0.2.3）✅ 已实现
- [x] **Shell 命令迁移到用户态** — echo/clear/help 作为 Ring 3 ELF 程序运行 ✅ 已实现
- [x] **get_system_info 通用系统调用** — syscall 27，支持 5 种信息查询类型：uptime (0)、date (1)、rand (2)、meminfo (3)、diskinfo (4) ✅ 已实现
- [x] **命令迁移到 Ring 3** — uptime/date/rand/meminfo/diskinfo 已作为独立 ELF 用户程序运行，通过 syscall 27 查询系统信息 ✅ 已实现
- [x] **TCP 协议栈**
  - 三次握手（SYN → SYN-ACK → ACK），连接状态机
  - 序列号/确认号管理 + 校验和（复用 IP 校验和函数）
  - 四次挥手（FIN 处理）
  - 简易连接表（支持 4 个并发连接）
- [x] **Socket 抽象层** ✅ 已实现
  - `sock_create()` / `sock_bind()` / `sock_connect()` / `sock_send()` / `sock_recv()` / `sock_listen()` / `sock_close()`
  - 环形接收缓冲区（1024 bytes），超时接收（`sock_recv` with timeout_ms）
  - UDP 自动路由到对应 socket（按 local_port 匹配）
- [x] **HTTP 客户端** — 基于 TCP 实现 GET 请求，获取网页内容
- [x] **Web 服务器** — 基于 TCP + HTTP 提供静态页面服务
  - 解析 HTTP 请求行（GET /path HTTP/1.1）
  - 返回 HTML 响应（状态行 + Content-Type）
  - 支持 200 OK 响应
- [ ] **SSH 服务器** — 基于 TCP 的加密远程 Shell
  - 需要 TCP + 熵源（PRNG）+ 加密算法（简易 AES 或 XOR 流密码）
  - 用户名/密码认证
  - 远程 Shell 交互（串口 Shell 的复用）

### 5.2 网络使用指南

**QEMU 网络配置**（已在 Makefile 中配置）：
```
-netdev user,id=net0,hostfwd=tcp::8088-:80,hostfwd=udp::8888-:8888
-device ne2k_pci,netdev=net0
```

> **为什么不能直接访问 10.0.2.15？** QEMU 的 `-netdev user` 创建一个隔离的虚拟网络，虚拟机内部 10.0.2.15 只在 QEMU 内部可见，宿主机无法直接路由。必须通过 `hostfwd` 规则将宿主机端口转发到虚拟机端口。反过来，虚拟机访问 `10.0.2.2` 可达宿主机，无需转发。

**IP 配置**（QEMU user-mode networking）：
| 角色 | IP | 说明 |
|------|------|------|
| 虚拟机 (guest) | 10.0.2.15 | TinyOS 固定 IP |
| 网关 (gateway) | 10.0.2.2 | QEMU 内置 NAT 网关 |
| DNS | 10.0.2.3 | QEMU 内置 DNS |
| 主机 (host) | 10.0.2.2 | 通过网关访问 |

#### Shell 命令参考

**`net` — 查看网络配置**
```
TinyOS> net
Network config:
  IP:      10.0.2.15
  Gateway: 10.0.2.2
  Mask:    255.255.255.0
  MAC:     52:54:00:12:34:56
```

**`ping <ip>` — 发送 ICMP Echo 请求**
```
TinyOS> ping 10.0.2.2
Pinging 10.0.2.2...
Ping sent to 10.0.2.2
```
> 首次 ping 会自动发送 ARP 请求解析目标 MAC，后续 ping 使用缓存。

**`send <ip> <port> <msg>` — 发送 UDP 数据包**
```
TinyOS> send 10.0.2.2 8888 Hello from TinyOS!
Sent 18 bytes to 10.0.2.2:8888
```

**`recv <port>` — 监听 UDP 端口（5秒）**
```
TinyOS> recv 8888
Listening on UDP port 8888 (5 seconds)...

[UDP 10.0.2.2:12345 -> :8888] (16 bytes)
Hello from host!

Done listening on port 8888.
```
> 注意：必须使用 QEMU `hostfwd` 中配置的端口（当前为 8888）。监听其他端口需要先在 Makefile 中添加对应的 `hostfwd` 规则。

**`arp` — 显示/清空 ARP 缓存**
```
TinyOS> arp
ARP cache:
  10.0.2.2 -> 52:56:00:00:00:02
  10.0.2.3 -> 52:56:00:00:00:03

TinyOS> arp -c
ARP cache cleared.
```

**`netstat` — 显示网络统计**
```
TinyOS> netstat
Network statistics:
  RX packets: 42
  TX packets: 38
  RX errors:  0
  TX errors:  0
  ARP:  2 requests sent, 2 replies recv
  ICMP: 3 sent, 2 recv
  UDP:  1 sent, 1 recv
  TCP:  12 sent, 8 recv
```

**`rand` — 生成随机数（xorshift32 PRNG）**
```
TinyOS> rand
963418056
```

**`netstat -r` — 重置网络统计**
```
TinyOS> netstat -r
Network statistics reset.
```

**`dhcp` — 通过 DHCP 获取 IP 配置**
```
TinyOS> dhcp
Sending DHCP Discover...
DHCP result:
  IP:      10.0.2.15
  Gateway: 10.0.2.2
  Mask:    255.255.255.0
```
> DHCP 向 QEMU 内置 DHCP 服务器（10.0.2.2）发送 Discover/Offer/Request/ACK 四步协商，动态获取 IP 地址。

**`tcp-recv <port>` — 监听 TCP 端口（10秒）**
```
TinyOS> tcp-recv 80
Listening for TCP on port 80 (10 seconds)...

[TCP 10.0.2.2:54321] (85 bytes)
GET / HTTP/1.1
Host: localhost
...

Done listening on TCP port 80.
```
> 与 `webserver` 命令类似，但仅显示接收到的数据，不发送响应。用于调试 TCP 连接。

**`http-get <host> [port] [path]` — HTTP GET 请求客户端**
```
TinyOS> http-get httpbin.org 80 /get
[HTTP] Resolving httpbin.org...
[HTTP] Resolved httpbin.org -> 52.34.40.43
[HTTP] Connecting to 52.34.40.43:80...
[HTTP] Connected. Sending GET request...
[HTTP] Response (1234 bytes):
HTTP/1.1 200 OK
Content-Type: application/json
...
[HTTP] GET request completed.
```
> 发送 HTTP/1.0 GET 请求到指定服务器的指定路径。默认端口 80，默认路径 `/`。
> 支持主机名解析（DNS）和 IP 地址直接连接。
> 详细用法和测试方法见 [docs/USAGE.md](docs/USAGE.md)。

**`forktest` — 测试 fork/exec 系统调用**
```
TinyOS> forktest
ForkTest: before fork...
PARENT: child PID=2
PARENT: yielding...
CHILD: fork=0, running!
CHILD: yielding...
PARENT: back from yield!
CHILD: back from yield!
PARENT: exiting.
CHILD: exiting.
```
> 创建子进程验证 fork (syscall 25)、yield (syscall 8) 和 task_exit 流程。
> **注意**: 当前版本中，子进程退出后 idle 任务可能触发 Page Fault（见已知问题）。

**`write <file> <text>` — 写文件到 FAT16 磁盘**
```
TinyOS> write note.txt Hello World!
Wrote 12 bytes to note.txt

TinyOS> ls
Directory listing:
Name             Size  Type
--------------------------------------
NOTE.TXT            12  FILE
```
> 支持创建新文件和覆写现有文件。文件名支持 8.3 格式（最多 8字符名 + 3字符扩展名）。

**`rm <file>` — 删除文件**
```
TinyOS> rm note.txt
Deleted note.txt
```

#### 双向通信示例

**TinyOS → Windows（发送 UDP）**：
```powershell
# Windows 上监听
$udp = New-Object System.Net.Sockets.UdpClient(8888)
$remote = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
$data = $udp.Receive([ref]$remote)
[System.Text.Encoding]::UTF8.GetString($data)
$udp.Close()
```
```
# TinyOS 端发送
TinyOS> send 10.0.2.2 8888 Hello from TinyOS!
```

**Windows → TinyOS（发送 UDP）**：
```
# TinyOS 端监听
TinyOS> recv 8888
```
```powershell
# Windows 上发送（Python，因为 Windows 没有 netcat）
python -c "import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.sendto(b'Hello from host!', ('127.0.0.1', 8888)); s.close()"
```

#### 添加新端口转发

若要使用其他端口，在 Makefile 的 `run`/`run-debug` 目标中添加 `hostfwd` 规则：
```makefile
# 示例：添加 UDP 9999 和 TCP 3000
-netdev user,id=net0,hostfwd=tcp::8088-:80,hostfwd=udp::8888-:8888,hostfwd=udp::9999-:9999,hostfwd=tcp::3000-:3000
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
- **fork/exec 后 idle 任务崩溃**（搁置）: 在 `forktest` 中，子进程通过 `task_exit()` 正确退出后，调度器切回 idle 任务时触发 Page Fault（错误地址 0xe987f4）和 Invalid Opcode 异常。已采取以下措施但仍未完全解决：
  - `scheduler_pick_next()` 扩展为同时考虑 `TASK_READY` 和 `TASK_RUNNING` 状态，确保 idle 任务能被选中
  - `prepare_switch()` 中为 idle 任务重置 `TSS.esp0` 到专用内核栈，避免使用已释放的子进程栈
  - `task_exit()` 切换到紧急安全栈 (`exit_safe_stack`) 后再 halt，防止 IRQ 处理程序使用已释放的内存
  - 根本原因推测：子进程内核栈释放后，残留的 IRQ 上下文引用导致页面错误

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
键盘    调试框架   堆分配器  蓝屏      上下文✓   同步     FAT12    libc      TCP/IP✓
定时器   RTC驱动              栈回溯    IPC✓      锁      IDE       ELF加载  信号
用户态   鼠标                            Spinlock           二进制加载  ACPI
系统调用  PCI                              Mutex               用户程序   SMP
```

---

## 每个阶段的检查清单

### 完成标准

| 阶段 | 必须完成 | 验证方法 |
|------|----------|----------|
| v0.15 | Makefile + GDB 调试 (已完成) | `make run-gdb` + `make gdb` 断点命中 |
| v0.2 | kmalloc/kfree 工作 | 分配内存并读写测试 |
| v0.3 | 页故障正确处理 | 访问无效地址触发蓝屏 |
| v0.4 | 两个任务交替运行 | `schedtest` 命令验证 A B 交替 |
| v0.4+ | IPC 三种机制正常 | `ipctest` 命令验证 Pipe/MQ/SHM 全部 PASS |
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