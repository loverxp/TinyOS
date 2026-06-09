# TinyOS 使用手册

## 构建与运行

### 工具链
- **NASM**: 汇编器，用于编译 .asm 文件
- **i686-elf-gcc**: 交叉编译器，用于编译 C 代码
- **i686-elf-ld**: 链接器
- **QEMU**: 模拟器（需独立安装，路径配置见 Makefile）

### 常用命令
```bash
make              # 构建内核和用户程序
make run          # 构建并运行（VGA 窗口 + PS/2 键盘）
make run-debug    # 构建并运行（VGA 窗口 + 串口日志到文件）
make run-serial   # 构建并运行（纯串口模式，-nographic）
make clean        # 清理构建产物
make rebuild      # 清理并重新构建
make user-programs # 仅编译用户程序
```

## Shell 命令参考

### 系统命令

**`help` — 显示命令帮助列表**
```
TinyOS> help
```

**`clear` — 清屏**
```
TinyOS> clear
```

**`uptime` — 显示系统运行时间**
```
TinyOS> uptime
Uptime: 123.456 seconds
```

**`date` — 显示 RTC 日期时间**
```
TinyOS> date
Date: 2026-06-06 12:34:56
```

**`rand` — 生成随机数（xorshift32 PRNG）**
```
TinyOS> rand
963418056
```

**`meminfo` — 显示物理内存使用情况**
```
TinyOS> meminfo
Total pages: 8192, Free: 7654, Used: 538
Total memory: 32 MB
```

**`diskinfo` — 显示 FAT16 磁盘信息**
```
TinyOS> diskinfo
Disk size: 16777216 bytes
Sector size: 512
Sectors per cluster: 1
Total clusters: 32768
Root entries: 512
```

**`alloc` — 测试内核内存分配**
```
TinyOS> alloc
```

**`free` — 测试内核内存释放**
```
TinyOS> free
```

**`except` — 触发异常测试（用于验证异常处理）**
```
TinyOS> except
```

**`kmtest` — 内核内存测试**
```
TinyOS> kmtest
```

**`pageinfo` — 显示分页信息**
```
TinyOS> pageinfo
```

### 网络命令

**QEMU 网络配置**（已在 Makefile 中配置）：
```
-netdev user,id=net0,hostfwd=tcp::8088-:80,hostfwd=udp::8888-:8888
-device ne2k_pci,netdev=net0
```

**IP 配置**（QEMU user-mode networking）：

| 角色 | IP | 说明 |
|------|------|------|
| 虚拟机 (guest) | 10.0.2.15 | TinyOS 固定 IP |
| 网关 (gateway) | 10.0.2.2 | QEMU 内置 NAT 网关 |
| DNS | 10.0.2.3 | QEMU 内置 DNS |
| 主机 (host) | 10.0.2.2 | 通过网关访问 |

> **为什么不能直接访问 10.0.2.15？** QEMU 的 `-netdev user` 创建一个隔离的虚拟网络，虚拟机内部 10.0.2.15 只在 QEMU 内部可见，宿主机无法直接路由。必须通过 `hostfwd` 规则将宿主机端口转发到虚拟机端口。反过来，虚拟机访问 `10.0.2.2` 可达宿主机，无需转发。

---

**`net` — 查看网络配置**
```
TinyOS> net
Network config:
  IP:      10.0.2.15
  Gateway: 10.0.2.2
  Mask:    255.255.255.0
  MAC:     52:54:00:12:34:56
```

**`ping <ip|hostname>` — 发送 ICMP Echo 请求**
```
TinyOS> ping 10.0.2.2
Pinging 10.0.2.2...
Ping sent to 10.0.2.2
```
> 支持域名解析（DNS）。首次 ping 会自动发送 ARP 请求解析目标 MAC，后续 ping 使用缓存。

**`send <ip|hostname> <port> <msg>` — 发送 UDP 数据包**
```
TinyOS> send 10.0.2.2 8888 Hello from TinyOS!
Sent 18 bytes to 10.0.2.2:8888
```
> 支持域名解析（DNS，通过 QEMU 内置 DNS 10.0.2.3:53）。首次发送会自动 ARP 解析。

**`recv <port>` — 监听 UDP 端口（5秒）**
```
TinyOS> recv 8888
Listening on UDP port 8888 (5 seconds)...

[UDP 10.0.2.2:12345 -> :8888] (16 bytes)
Hello from host!

Done listening on port 8888.
```
> 注意：必须使用 QEMU `hostfwd` 中配置的端口（当前为 8888）。监听其他端口需要先在 Makefile 中添加对应的 `hostfwd` 规则。

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

**`netstat -r` — 重置网络统计**
```
TinyOS> netstat -r
Network statistics reset.
```

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

**`pci` — 显示 PCI 设备列表**
```
TinyOS> pci
```

**`partitions` — 显示 MBR 分区表**
```
TinyOS> partitions
MBR Partition Table:
Idx  Boot Type           Start(LBA)   Size(MB)
----------------------------------------------
0    Yes  FAT16 LBA             2048         16
```
> 读取磁盘扇区 0 (MBR)，解析分区表并显示各分区的类型、起始 LBA 和大小。支持识别 FAT12/16/32、NTFS、Linux 等常见分区类型。

#### HTTP GET 请求

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

发送 HTTP/1.0 GET 请求到指定服务器的指定路径。默认端口 80，默认路径 `/`。支持主机名解析（DNS）和 IP 地址直接连接。

**测试方法：**

方法 1 — 测试外部站点（需要互联网）：
```
TinyOS> http-get httpbin.org 80 /get
```

方法 2 — 宿主机上启动 HTTP 服务器后连接：
```
# 宿主机（Windows PowerShell）：
cd ~\Desktop
python -m http.server 9999

# TinyOS 中连接（10.0.2.2 为宿主机地址）：
TinyOS> http-get 10.0.2.2 9999 /test.txt
```

方法 3 — 先用 ping 确认网络可达：
```
TinyOS> ping 10.0.2.2
TinyOS> ping httpbin.org
```

**常见问题：**

| 现象 | 可能原因 | 解决 |
|------|---------|------|
| `Connection timeout` | 目标服务器不可达或防火墙拦截 | 换用 `10.0.2.2 9999`（宿主机本地测试） |
| `Could not resolve` | DNS 查询失败或站点使用 CNAME 链 | 先用 `ping 10.0.2.3` 确认 DNS 可达，或直接用 IP |
| `No response received` | 服务器没返回数据或连接中途关闭 | 检查 HTTP 路径是否正确 |
| 编译错误 | 增量构建漏编新文件 | 执行 `make rebuild` |

#### Web 服务器

TinyOS 内置一个简易 HTTP 服务器，监听 TCP 端口 80，通过 QEMU `hostfwd=tcp::8088-:80` 映射到宿主机端口 8088：
```
# 在 TinyOS 中启动 Web 服务器
TinyOS> webserver

# 在宿主机浏览器中访问
http://localhost:8088/
```

### 文件系统命令

**`ls [path]` — 列出目录内容**
```
TinyOS> ls
Directory listing:
Name             Size  Type
--------------------------------------
README.TXT         256  FILE
SUB_DIR              0  DIR
```

**`cat <path>` — 显示文件内容**
```
TinyOS> cat README.TXT
Welcome to TinyOS!
```

**`write <path> <text>` — 创建或覆写文件**
```
TinyOS> write note.txt Hello World!
Wrote 12 bytes to note.txt
```
> 支持路径（如 `DIR/SUBDIR/FILE.TXT`）。文件名 8.3 格式（最多 8字符名 + 3字符扩展名），文本不支持引号。

**`rm <path>` — 删除文件**
```
TinyOS> rm note.txt
Deleted note.txt
```
> 支持路径。

**`mkdir <path>` — 创建子目录**
```
TinyOS> mkdir SUBDIR
```
> 支持路径（自动定位父目录）。

**`rmdir <path>` — 删除空子目录**
```
TinyOS> rmdir SUBDIR
```
> 支持路径。

### 进程与 IPC 命令

**`schedtest [N]` — 测试调度器（N 秒后退出，默认 10 秒，上限 300 秒）**
```
TinyOS> schedtest 5
```

**`forktest` — 测试 fork/exec/yield/exit 流程**
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
> **注意**: 当前版本中，子进程退出后 idle 任务可能触发 Page Fault（见已知问题）。

**`ipctest` — 运行 IPC 三阶段自动化测试**
```
TinyOS> ipctest
--- IPC Test: Pipe ---
Pipe test PASSED
--- IPC Test: Message Queue ---
MQ test PASSED
--- IPC Test: Shared Memory ---
SHM test PASSED
All IPC tests passed!
```
> 验证 Pipe 管道、Message Queue 消息队列、Shared Memory 共享内存三种 IPC 机制的阻塞/唤醒和数据完整性。

### 图形界面命令

**`gui` — 启动图形界面（VBE 高分辨率 + 鼠标 + 窗口管理器）**
```
TinyOS> gui
```

**`gfxsnake` — VGA Mode 13h 贪吃蛇游戏**
```
TinyOS> gfxsnake
```

## 双向通信示例

### TinyOS → Windows（发送 UDP）
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

### Windows → TinyOS（发送 UDP）
```
# TinyOS 端监听
TinyOS> recv 8888
```
```powershell
# Windows 上发送（Python，因为 Windows 没有 netcat）
python -c "import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.sendto(b'Hello from host!', ('127.0.0.1', 8888)); s.close()"
```

### 添加新端口转发
在 Makefile 的 `run`/`run-debug` 目标中添加 `hostfwd` 规则：
```makefile
# 示例：添加 UDP 9999 和 TCP 3000
-netdev user,id=net0,hostfwd=tcp::8088-:80,hostfwd=udp::8888-:8888,hostfwd=udp::9999-:9999,hostfwd=tcp::3000-:3000
```

## 用户程序参考

以下用户程序已作为独立 Ring 3 ELF 嵌入内核，通过 `run_embedded_elf()` 加载执行：

| 命令 | 功能 | 系统调用 | 源文件 |
|------|------|----------|--------|
| `hello` | 打印 "Hello from userspace!" | syscall 7 (printf) | [hello.c](user/apps/hello.c) |
| `echo [text]` | 回显参数 | syscall 23 (get_cmdline) | [echo.c](user/apps/echo.c) |
| `clear` | 清空 VGA 文本屏幕 | syscall 24 (clear_screen) | [clear.c](user/apps/clear.c) |
| `help` | 显示命令帮助列表 | syscall 7 (printf) | [help.c](user/apps/help.c) |
| `uptime` | 显示系统运行时间（秒.毫秒） | syscall 27 type=0 | [uptime.c](user/apps/uptime.c) |
| `date` | 显示 RTC 日期时间 | syscall 27 type=1 | [date.c](user/apps/date.c) |
| `rand` | 生成 xorshift32 随机数 | syscall 27 type=2 | [rand.c](user/apps/rand.c) |
| `meminfo` | 显示物理内存使用情况 | syscall 27 type=3 | [meminfo.c](user/apps/meminfo.c) |
| `diskinfo` | 显示 FAT16 磁盘信息 | syscall 27 type=4 | [diskinfo.c](user/apps/diskinfo.c) |
| `forktest` | 测试 fork/exec/yield/exit 流程 | syscall 25/26/8/0 | [forktest.c](user/apps/forktest.c) |
| `gfxsnake` | VGA Mode 13h 贪吃蛇游戏 | syscall 21/22/24 | [gfxsnake.c](user/apps/gfxsnake.c) |

## IPC 参考

- **Pipe**（管道）: 512 字节环形缓冲区，最多 8 个。`pipe_create()` / `read()` / `write()` / `close()`。空时阻塞读者，满时阻塞写者
- **Message Queue**（消息队列）: 8 槽 × 64 字节，最多 8 个。`mq_create()` / `send()` / `recv()` / `close()`。保持消息边界，FIFO 顺序
- **Shared Memory**（共享内存）: 按名称查找，PMM 页分配。`shm_create(name, size)` / `shm_open(name)` / `shm_close(name)`。所有任务共享地址空间
- **系统调用 10-20**: Pipe(10-13) / MQ(14-17) / SHM(18-20)，通过 int 0x80 调用

## 系统信息查询（syscall 27）

用户程序可通过 syscall 27 查询系统信息：
```c
int ret = get_system_info(type, &buf, sizeof(buf))
```

| 类型 | 功能 | 返回数据 |
|------|------|----------|
| 0 | uptime | `uint32_t ticks`（50Hz） |
| 1 | date | `rtc_time_t` 结构 |
| 2 | rand | `uint32_t` 随机数 |
| 3 | meminfo | 4 × `uint32_t`（总页数/空闲/已用/总MB） |
| 4 | diskinfo | 5 × `uint32_t`（总字节/扇区大小/每簇扇区/总簇数/根目录项数） |

调用示例：
```c
uint32_t ticks;
if (get_system_info(0, &ticks, sizeof(ticks)) >= sizeof(ticks)) {
    uint32_t secs = ticks / 50;
    uint32_t ms = (ticks % 50) * 20;
    printf("Uptime: %u.%u seconds\n", secs, ms);
}
```