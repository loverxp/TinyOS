# TinyOS IPC 测试指南

## 概述

TinyOS 进程间通信 (IPC) 提供三种机制：
- **Pipe** — 字节流管道（512 字节环形缓冲区）
- **Message Queue** — 消息队列（8 个槽位，每条最大 64 字节）
- **Shared Memory** — 共享内存（按名称查找，4KB 页分配）

## 快速测试

### 启动 TinyOS

```bash
make run
# 或带串口调试日志
make run-debug
# 或纯串口模式
make run-serial
```

### 运行 ipctest

在 Shell 提示符下输入：

```
TinyOS> ipctest
```

**预期输出：**

```
=== IPC Test ===
[Pipe] Creating pipe...
[Pipe] PASS
[MQ] Creating message queue...
[MQ] PASS
[SHM] Creating shared memory 'test'...
[SHM] PASS
=== IPC Test Done ===
```

如果任何阶段显示 `FAIL`，检查串口日志 (`logs/serial.log` 或 COM1 输出) 中的 `[sched]` 和 `[IPC]` 前缀的调试信息。

---

## 测试阶段详解

### 阶段 1: Pipe 测试

**原理：** 创建两个任务 — `ipc_prod`（生产者）和 `ipc_cons`（消费者）

1. 主线程创建 pipe（ID=0）
2. `ipc_cons` 启动，调用 `pipe_read()` → 管道为空，消费者阻塞
3. `ipc_prod` 启动，调用 `pipe_write()` 写入 "Hello IPC!"（10 字节）
4. 写入后唤醒消费者，消费者读取并比对数据
5. 消费者设置 `ipc_pipe_result = 0`（成功），退出
6. 主线程关闭 pipe，打印结果

**验证点：** 阻塞读/写、唤醒机制、数据完整性

### 阶段 2: Message Queue 测试

**原理：** 创建 `ipc_mqtx`（发送者）和 `ipc_mqrx`（接收者）

1. 主线程创建消息队列
2. `ipc_mqrx` 启动，调用 `mq_recv()` → 队列为空，接收者阻塞
3. `ipc_mqtx` 发送 3 条消息：`msg_one`、`msg_two`、`msg_thr`（每条 7 字节）
4. 每次发送后唤醒接收者，接收者逐条接收并比对
5. 接收者验证消息顺序和内容，退出

**验证点：** 消息边界保持（非字节流）、FIFO 顺序、阻塞接收

### 阶段 3: Shared Memory 测试

**原理：** 创建 `ipc_shmw`（写入者）和 `ipc_shmr`（读取者）

1. `ipc_shmw` 调用 `shm_create("test", 64)` 创建共享内存，写入 `0xDEADBEEF` 和 `42`
2. `ipc_shmr` 先 sleep 100ms 确保写入者完成，然后调用 `shm_open("test")` 获取指针
3. 读取者验证两个值，然后调用 `shm_close("test")` 释放

**验证点：** 按名称查找、PMM 页分配、跨任务内存共享

---

## 手动测试单个 IPC 机制

### Pipe 手动测试

在内核代码中添加测试任务：

```c
static int test_pipe = -1;

static void pipe_writer_task(void) {
    const char* msg = "Test data";
    int len = 0;
    while (msg[len]) len++;
    
    printf("[writer] Writing %d bytes...\n", len);
    int n = pipe_write(test_pipe, msg, len);
    printf("[writer] Wrote %d bytes\n", n);
    task_exit();
}

static void pipe_reader_task(void) {
    char buf[64];
    printf("[reader] Waiting for data...\n");
    int n = pipe_read(test_pipe, buf, sizeof(buf));
    buf[n] = '\0';
    printf("[reader] Got %d bytes: %s\n", n, buf);
    task_exit();
}

// 在 shell 命令中调用：
test_pipe = pipe_create();
task_create("reader", pipe_reader_task);
task_create("writer", pipe_writer_task);
```

**测试场景：**
- **阻塞读：** reader 先启动，应该阻塞直到 writer 写入
- **阻塞写：** 连续写入超过 512 字节，writer 应该阻塞
- **关闭行为：** `pipe_close()` 后，阻塞的任务应该被唤醒

### Message Queue 手动测试

```c
static int test_mq = -1;

static void mq_sender_task(void) {
    mq_send(test_mq, "Hello", 5);
    mq_send(test_mq, "World", 5);
    printf("[sender] Sent 2 messages\n");
    task_exit();
}

static void mq_receiver_task(void) {
    char buf[64];
    int n;
    
    n = mq_recv(test_mq, buf, sizeof(buf));
    buf[n] = '\0';
    printf("[receiver] Got: %s (%d bytes)\n", buf, n);
    
    n = mq_recv(test_mq, buf, sizeof(buf));
    buf[n] = '\0';
    printf("[receiver] Got: %s (%d bytes)\n", buf, n);
    
    task_exit();
}

// 在 shell 命令中调用：
test_mq = mq_create();
task_create("receiver", mq_receiver_task);
task_create("sender", mq_sender_task);
```

**测试场景：**
- **消息边界：** 发送 "Hello" 和 "World"，应该分别接收（不是 "HelloWorld"）
- **满队列：** 连续发送 9 条消息（超过 8 个槽位），第 9 次应该阻塞
- **关闭行为：** `mq_close()` 唤醒所有等待者

### Shared Memory 手动测试

```c
// 创建并写入
uint32_t* shm = (uint32_t*)shm_create("mydata", 64);
if (shm) {
    shm[0] = 12345;
    shm[1] = 0xCAFEBABE;
    printf("[writer] Wrote to shared memory\n");
}

// 在另一个任务中读取
uint32_t* shm = (uint32_t*)shm_open("mydata");
if (shm) {
    printf("[reader] shm[0]=%u, shm[1]=0x%x\n", shm[0], shm[1]);
}

// 清理
shm_close("mydata");
```

**测试场景：**
- **重复创建：** 同名 `shm_create()` 返回相同指针
- **打开不存在的：** `shm_open("nonexistent")` 返回 NULL
- **内存释放：** `shm_close()` 后，`pmm_get_free_pages()` 应该增加

---

## 系统调用接口

用户程序可通过 `int 0x80` 调用 IPC 系统调用：

| 调用号 | 名称 | EAX | EBX | ECX | EDX | 返回 (EAX) |
|--------|------|-----|-----|-----|-----|------------|
| 10 | pipe_create | 10 | — | — | — | pipe ID |
| 11 | pipe_read | 11 | id | buf | max | 字节数 |
| 12 | pipe_write | 12 | id | data | len | 字节数 |
| 13 | pipe_close | 13 | id | — | — | — |
| 14 | mq_create | 14 | — | — | — | mq ID |
| 15 | mq_send | 15 | id | data | len | 0 |
| 16 | mq_recv | 16 | id | buf | max | 消息长度 |
| 17 | mq_close | 17 | id | — | — | — |
| 18 | shm_create | 18 | name | size | — | 指针 |
| 19 | shm_open | 19 | name | — | — | 指针 |
| 20 | shm_close | 20 | name | — | — | 0 |

**示例（用户态汇编）：**

```asm
; 创建管道
mov eax, 10
int 0x80
mov [pipe_id], eax

; 写入管道
mov eax, 12
mov ebx, [pipe_id]
mov ecx, message
mov edx, msg_len
int 0x80
```

---

## 调试技巧

### 查看串口日志

```bash
# run-debug 模式会输出到 logs/serial.log
make run-debug

# 查看日志
cat logs/serial.log | grep -E "\[IPC\]|\[sched\]|pipe|mq|shm"
```

**关键日志：**
- `[IPC] Initialized` — 启动时初始化
- `[sched] 'task_x' sleeping for N ticks` — 任务阻塞
- `[sched] Waking 'task_x'` — 定时器唤醒
- `[sched] Freeing stack for 'task_x'` — 任务退出清理

### 使用 GDB 调试

```bash
# 终端 1：启动 QEMU 并暂停
make run-gdb

# 终端 2：连接 GDB
make gdb

# 在 GDB 中设置断点
(gdb) break pipe_write
(gdb) break mq_recv
(gdb) break ipc_block
(gdb) continue
```

然后在 QEMU 中运行 `ipctest`，观察断点命中和变量值。

### 检查任务状态

在 `prepare_switch()` 中添加调试输出：

```c
// kernel/scheduler.c 的 prepare_switch() 中
for (int i = 1; i < MAX_TASKS; i++) {
    if (tasks[i].state == TASK_BLOCKED && tasks[i].ipc_wait_obj) {
        serial_printf("[sched] Task '%s' blocked on IPC obj=0x%x type=%d\n",
                      tasks[i].name, tasks[i].ipc_wait_obj, tasks[i].ipc_wait_type);
    }
}
```

---

## 常见问题

### Q: ipctest 卡住不动

**原因：** 任务阻塞后未被唤醒

**排查：**
1. 检查串口日志是否有 `[sched] 'task_x' sleeping` 但无 `Waking`
2. 确认 `scheduler_wake_ipc()` 被调用
3. 检查 `ipc_wait_obj` 指针是否匹配

### Q: Pipe 测试 FAIL

**原因：** 数据不匹配或长度错误

**排查：**
1. 在 `pipe_consumer` 中打印实际读取的字节数和数据
2. 检查环形缓冲区的 `head`/`tail`/`count` 是否正确更新
3. 确认 `memcpy` 没有越界

### Q: Shared Memory 返回 NULL

**原因：** PMM 无可用页或名称查找失败

**排查：**
1. 运行 `meminfo` 查看空闲页数
2. 确认 `shm_create()` 的名称拼写一致
3. 检查 `MAX_SHM`（默认 8）是否已达上限

### Q: 消息队列顺序错乱

**原因：** 多生产者竞争或 FIFO 逻辑错误

**排查：**
1. 在 `mq_send()` 和 `mq_recv()` 中添加日志打印 `head`/`tail`
2. 确认 `tail` 写入后正确递增并取模
3. 检查并发写入时是否有竞态（当前设计依赖调度器原子性）

---

## 性能测试

### 吞吐量测试

连续写入 1000 次，测量耗时：

```c
uint32_t start = timer_get_ticks();
for (int i = 0; i < 1000; i++) {
    pipe_write(pipe_id, "data", 4);
    // 需要消费者同步读取
}
uint32_t elapsed = timer_get_ticks() - start;
printf("1000 writes took %u ticks (%u ms)\n", elapsed, elapsed * 20);
```

### 延迟测试

测量单次 IPC 操作的上下文切换开销：

```c
uint32_t start = timer_get_ticks();
pipe_write(pipe_id, "x", 1);  // 唤醒消费者
// 消费者读取后设置标志
while (!flag) halt();
uint32_t latency = timer_get_ticks() - start;
printf("IPC latency: %u ticks\n", latency);
```

预期：单次 IPC 操作约 1-2 个 timer tick（20-40ms @ 50Hz）。

---

## 扩展测试

### 多生产者/消费者

创建 3 个写者和 3 个读者，验证并发安全：

```c
for (int i = 0; i < 3; i++) {
    task_create("prod1", pipe_writer_task);
    task_create("prod2", pipe_writer_task);
    task_create("cons1", pipe_reader_task);
}
```

### 压力测试

填满管道后继续写入，验证阻塞和唤醒：

```c
// 写入 600 字节（超过 512 缓冲区）
char big_buf[600];
memset(big_buf, 'A', 600);
int n = pipe_write(pipe_id, big_buf, 600);  // 应该在 512 字节后阻塞
printf("Wrote %d bytes (should be < 600 if blocked)\n", n);
```

### 边界条件

- 空消息：`mq_send(mq_id, "", 0)` 和 `mq_recv(mq_id, buf, 0)`
- 超长消息：`mq_send(mq_id, data, 100)` 应该返回 -1（超过 MQ_MSG_SIZE=64）
- 重复关闭：`pipe_close(id)` 两次应该安全（第二次无效）
- NULL 名称：`shm_create(NULL, 64)` 应该返回 NULL

---

## 参考资料

- 实现细节：`kernel/ipc.c`
- 调度器集成：`kernel/scheduler.c` 的 `scheduler_wake_ipc()`
- 系统调用：`drivers/interrupts.c` 的 `syscall_handler()`
- API 定义：`include/ipc.h`
