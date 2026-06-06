# TinyOS 已知问题（搁置）

以下问题已确认但尚未修复，临时绕过方案可用。

---

## 退出 GUI 后键盘可能无响应

`cmd_gui` 退出流程中 `shell_char_callback` 重注册时机与键盘中断存在竞态，或 `enable_interrupts()` 前后 8042 状态不一致。

**临时绕过**：使用串口终端。

---

## 首次划入 QEMU 窗口鼠标位置不正确

PS/2 鼠标初始化后的首个数据包包含异常位移值，导致光标瞬间跳到错误位置，后续恢复正常。

**可免方案**：忽略前 N 个数据包。

---

## schedtest task_sleep 唤醒未验证

`task_sleep()` 设置 `state = TASK_BLOCKED` 后，`prepare_switch` 中的唤醒逻辑 (`now >= sleep_deadline` → `TASK_READY`) 在代码层面正确，但日志中 `wake_check` 始终显示 `wakes=0, delta=-9`（deadline 永远差 9 tick 到期），且无 `W!` / `[sched] Waking` 输出。旧版二进制（`task_sleep` 前）任务靠 busy-wait 运行，不真正阻塞，故无需唤醒。当前代码若任务真正 BLOCKED 且唤醒未触发，任务会永久卡死。

**待验证**：手动删除 `logs/serial.log` 后重新 `make run-debug`。

---

## http-get 首次执行连接超时，重试后成功

`http-get httpbin.org 80 /get` 首次执行报 `Connection timeout`，退出后立即重试则正常。DNS 解析和 ARP 缓存均正常（已预先 ping 过）。

**推测原因**：TCP 状态机在初始连接时有时序问题——SYN 发出后 SYN-ACK 未正确关联到 `TCP_SYN_SENT` 状态的连接，或 `ne2000_poll_recv()` 轮询接收不足。

**临时绕过**：重试一次即成功。

---

## forktest 子进程退出后可能触发 Page Fault

fork 后的子进程通过 `task_exit()` 退出后，调度器释放子进程内核栈，但 timer IRQ 处理程序可能仍在子进程栈上运行，导致访问已释放内存。

**临时绕过**：无。该问题涉及 TSS.esp0 切换、紧急退出栈等多个机制，修复复杂度较高，已搁置。