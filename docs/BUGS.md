# TinyOS Bug 清单

> 生成时间: 2026-06-06
> 基于 commit: 96bcc63 (HEAD) + 未提交改动

---

## CRITICAL

### BUG-01: `fat16_delete()` 全局 `sector_buf` 污染导致目录损坏
- **文件**: `kernel/fat16.c:363-379`
- **现象**: `fat16_delete()` 将目录扇区读入全局 `sector_buf`，然后调用 `fat16_free_chain()` 释放簇链。`fat16_free_chain()` 内部调用 `fat16_next_cluster()` / `fat16_set_fat_entry()` 会覆写 `sector_buf` 为 FAT 扇区数据。之后 `entries[e].name[0] = 0xE5` 修改的是已被污染的 buffer，最终写回目录 LBA 时写入的是篡改过的 FAT 数据。
- **后果**: 目录扇区被 FAT 数据覆盖，该扇区所有 16 个目录项全部损坏。
- **修复**: 在调用 `fat16_free_chain()` 之前先标记目录项删除并写回，或改用局部 buffer（如 `fat16_write` 中的 `dir_sector_buf`）。

### BUG-02: `tx_frame` 全局共享，IRQ 上下文与主代码竞态
- **文件**: `kernel/net.c:79`
- **现象**: `tx_frame[1600]` 被所有发送函数共享（`net_send_udp`, `tcp_send_segment`, `dhcp_send_raw`, ICMP reply 等）。NE2000 接收中断 `net_recv_handler` 也在 IRQ 上下文写 `tx_frame`（如 TCP ACK 响应）。主代码构建 UDP 帧时若 IRQ 触发，帧数据被覆盖。
- **后果**: 发送数据损坏，协议行为不可预测。
- **修复**: 发送路径加 `cli/sti` 保护，或使用双缓冲。

---

## HIGH

### BUG-03: `task_entries[]` 数组越界写（PID 不回收）
- **文件**: `kernel/scheduler.c:86`
- **现象**: `task_entries[task->pid] = entry` 按 `next_pid` 索引，`next_pid` 单调递增不回收。`task_entries` 大小为 `MAX_TASKS=16`，但 PID 无限增长。第 16 次 `task_create` 时 `task_entries[16]` 越界写。
- **触发**: `schedtest` 重复运行 8 次（每次创建 2 个任务）即触发。
- **修复**: 改用 task slot index（`task - tasks`）索引 `task_entries`，或对 `pid` 取模。

### BUG-04: UDP/TCP 发送无 payload 长度校验
- **文件**: `kernel/net.c:284` (`net_send_udp`), `net.c:394` (`tcp_send_segment`)
- **现象**: `memcpy(udp_start + sizeof(udp_header_t), data, len)` 中 `len` 为 `uint16_t`（最大 65535），但 `tx_frame` 仅 1600 字节。`len > 1558` 时 buffer overflow。
- **修复**: 添加 `if (len > 1558) return -1;` 校验。

### BUG-05: 接收路径不校验 IP/TCP/ICMP checksum
- **文件**: `kernel/net.c:553` (`handle_ip`), `net.c:461` (`handle_tcp`), `net.c:226` (`handle_icmp`)
- **现象**: 接收端不验证校验和，损坏或伪造包被静默处理并可能触发响应。
- **修复**: 在分发到上层协议前校验 IP header checksum；TCP/ICMP 校验 checksum 后丢弃不匹配包。

### BUG-06: `recv <port>` 命令破坏 socket 层 UDP 回调
- **文件**: `kernel/shell.c:876-899`
- **现象**: 
  1. `port` 参数解析后未用于过滤，回调接收所有 UDP 包。
  2. 命令结束时 `net_set_udp_callback(NULL)` 覆盖了 `net_init` 注册的 `socket_udp_handler`，socket 层 UDP 接收永久失效。
- **修复**: 添加端口过滤；退出时恢复 `socket_udp_handler` 而非 NULL。

---

## MEDIUM

### BUG-07: `fat16_write()` 覆写文件非原子
- **文件**: `kernel/fat16.c:396-399`
- **现象**: 先 `fat16_delete()` 旧文件，再创建新文件。若创建失败（空间不足、I/O 错误），旧文件已永久丢失。
- **修复**: 先写入新数据到临时簇链，成功后再替换目录项和旧簇链。

### BUG-08: FAT16 不检查只读属性
- **文件**: `kernel/fat16.c:372` (`fat16_delete`), `fat16.c:388` (`fat16_write`)
- **现象**: 对 `FAT16_ATTR_READ_ONLY` 文件执行删除或覆写无拦截。
- **修复**: 添加属性检查，只读文件返回错误。

### BUG-09: ARP 表无条件更新
- **文件**: `kernel/net.c:143`, `net.c:618-621`
- **现象**: 每个 ARP 包（含 unsolicited reply）和每个 IP 包都无条件更新 ARP 表。攻击者可发伪造 ARP/IP 包劫持流量。
- **修复**: 仅更新已有 ARP 条目或仅响应已发起的请求。

### BUG-10: TCP ISN 可预测
- **文件**: `kernel/net.c:500`
- **现象**: `tcp_my_seq` 从 10000 开始逐连接 +1，序列号完全可预测。
- **修复**: 使用 PRNG 或基于 timer tick + 连接的哈希生成 ISN。

### BUG-11: DHCP 使用固定 transaction ID
- **文件**: `kernel/net.c:888`
- **现象**: `dhcp_xid = 0xDEADBEEF`，攻击者可抢先发送伪造 OFFER。
- **修复**: 使用 `prng_next()` 生成随机 xid。

### BUG-12: `tcp_checksum()` 栈上分配 1600 字节
- **文件**: `kernel/net.c:328`
- **现象**: `uint8_t buf[1600]` 在内核栈（通常 4KB）上分配，IRQ 上下文可能栈溢出。
- **修复**: 改用 `tx_frame` 或静态 buffer。

### BUG-13: TCP FIN 后立即释放连接
- **文件**: `kernel/net.c:523-531`
- **现象**: 收到 FIN 后设 `TCP_CLOSE_WAIT` 并立即 `tcp_free_conn()`。后续重传包找不到连接被丢弃。
- **修复**: 保持连接直到本端也关闭（实现 `TIME_WAIT` 或至少延迟释放）。

### BUG-14: `rand` 命令每次重新 seed
- **文件**: `kernel/shell.c:900-902`
- **现象**: 每次 `rand` 都用 `timer_get_ticks()` 重新 seed。同一 20ms tick 内多次调用输出相同。
- **修复**: 仅在 `kernel_main` 中 seed 一次，后续直接调用 `prng_next()`。

### BUG-15: `task_sleep` 唤醒未验证
- **文件**: `kernel/scheduler.c:151-180`
- **现象**: `prepare_switch` 中 `now >= sleep_deadline` 唤醒逻辑代码正确，但日志中 `wake_check` 始终显示 `wakes=0`。当前代码的 BLOCKED→READY 转换尚未被实际验证。
- **状态**: 待手动清理日志后重新验证。

---

## LOW

### BUG-16: `kprintf()` 无边界检查
- **文件**: `lib/debug.c:11,26`
- **现象**: `buf[256]` 写入无越界保护，长 `%s` 参数导致栈溢出。

### BUG-17: `kprintf()` 对 `INT_MIN` 取反是 UB
- **文件**: `lib/debug.c:45-46`
- **现象**: `v = -v` 当 `v == INT_MIN` 时是有符号整数溢出（未定义行为）。

### BUG-18: `sock_accept()` 未实现
- **文件**: `kernel/net.c:1027-1031`
- **现象**: 始终返回 -1，服务端 TCP socket 不可用。

### BUG-19: TCP socket 源端口硬编码
- **文件**: `kernel/net.c:384`
- **现象**: `tcp_send_segment()` 源端口固定为 `tcp_listen_port`，无法自定义。

### BUG-20: `sock_recv(timeout=0)` 无限等待
- **文件**: `kernel/net.c:1002-1007`
- **现象**: `timeout_ms == 0` 时跳过超时检查，若无数据到达则永久 hang。

### BUG-21: 网卡混杂模式 + 无目的 IP 过滤
- **文件**: `drivers/ne2000.c:271`, `kernel/net.c:553`
- **现象**: NIC 配置为混杂模式，`handle_ip()` 不检查 `dst_ip` 是否为本机 IP，处理发往其他主机的包。

### BUG-22: Socket ring buffer 满时静默丢数据
- **文件**: `kernel/net.c:945-951`
- **现象**: 1024 字节 ring buffer 满后，新数据截断写入，无错误指示、无计数器、无流控。

### BUG-23: `except.c` 输出顺序混乱
- **文件**: `kernel/except.c:37-42`
- **现象**: "System halted" 打印在 `kernel_backtrace()` 之前，backtrace 输出出现在 "halted" 之后。

### BUG-24: `fat16_free_chain()` 无簇号上界校验
- **文件**: `kernel/fat16.c:331`
- **现象**: 损坏的 FAT 条目可能指向超出磁盘范围的簇号，导致越界读写 FAT。

### BUG-25: `fat16_set_fat_entry()` 多 FAT 副本部分写入失败
- **文件**: `kernel/fat16.c:299-305`
- **现象**: FAT #1 写入成功但 FAT #2 失败时，副本不一致。

### BUG-26: `parse_ip()` uint32_t 溢出
- **文件**: `kernel/shell.c:397-400`
- **现象**: 长数字串（如 `"99999999999.0.0.1"`）累加时 uint32_t 回绕，最终 `> 255` 检查可能误通过。

### BUG-27: Snake 食物位置用 `timer_get_ticks()` 而非 PRNG
- **文件**: `kernel/shell.c:207-208`
- **现象**: 确定性随机，相同 tick 产生相同位置。已有 `prng_range()` 可替代。

---

## BUG-28: 用户程序运行偶发分配用户栈失败
- **现象**: 应用第一次能运行，第二次提示分配用户栈失败，第三次又能运行（如 `hello` 命令）。呈周期性规律。
- **推测**: 用户栈分配或回收逻辑存在竞态/状态残留，偶发分配失败。可能与 `run_embedded_elf()` 中用户栈的释放时机或 TSS.esp0 切换相关。
- **文件**: `kernel/loader.c`（`run_embedded_elf` 用户栈分配）

### BUG-29: Shell Tab 补全与输入编辑行为异常
- **现象**: 输入 `h` 再按 Tab，然后输入 `e` 再按 Tab，命令行变为 `hhe`；回删到第一个 `h` 时无法再删除。
- **文件**: `kernel/shell.c`（Tab 补全逻辑、行编辑处理）

### BUG-30: 管道命令 `help | more` 提示未知命令或触发页错误
- **现象**: 输入 `help | more` 被识别为未知命令，或触发 Page Fault。
- **文件**: `kernel/shell.c`（管道解析）、`kernel/ipc.c`（Pipe 实现）

### BUG-31: 图形游戏（dino/si）画面画出后立即返回字符模式
- **现象**: `dino` 和 `si` 命令执行后，游戏画面已在屏幕上画出，但输入模式已回到 Shell 命令行，游戏无法继续运行。
- **推测**: 游戏程序切换到 VGA Mode 13h 后执行，但返回时未阻塞等待游戏结束，或恢复文本模式的时机过早。
- **文件**: `user/apps/dino.c`、`user/apps/si.c`、`loader.c`（`run_embedded_elf` 退出流程）

### BUG-32: `tinyhttpd` 命令执行异常
- **现象**: 输入 `tinyhttpd` 后打印大量 syscall 输出，然后返回内核态（Shell），Web 服务器未正常运行。
- **文件**: `user/apps/tinyhttpd.c`、`kernel/webserver.c`

### BUG-33: `sh` 命令不存在
- **现象**: Shell 中执行 `sh` 返回命令不存在。
- **文件**: `kernel/shell.c`（命令表）

### BUG-34: WebServer 后台进程不显示在 `ps` 中
- **现象**: WebServer 自动启动为后台进程，但 `ps` 命令仅显示 `idle` 任务，无法测试 `kill` 命令。
- **文件**: `kernel/shell.c`（`ps` 命令实现）、`kernel/scheduler.c`（任务列表遍历）

### BUG-35: `&` 后台运行操作符无效
- **现象**: Shell 的 `&` 后台执行操作符不起作用，命令仍在前台执行。
- **文件**: `kernel/shell.c`（命令解析/`&` 处理逻辑）

---

## 修复优先级建议

1. **BUG-01** (FAT16 delete 目录损坏) — 数据损坏，必须立即修
2. **BUG-03** (task_entries OOB) — 内核崩溃，schedtest 必现
3. **BUG-04** (UDP payload overflow) — 内存破坏
4. **BUG-02** (tx_frame 竞态) — 网络数据损坏
5. **BUG-06** (recv 破坏 socket) — 功能回归
6. 其余按模块分批处理
