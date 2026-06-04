# TinyOS 多任务调度器

## 概述

已实现的抢占式 Round-Robin 调度器，支持多个**内核级线程**（Ring 0）交替执行。通过 Shell 命令 `schedtest` 触发测试，任务在指定时间后自动退出。

### 验证标准

- `schedtest [N]` 命令创建两个线程，交替打印 `A B A B A B ...`，N 秒后自动停止
- Shell 在 idle 态正常运行，命令仍可输入
- 系统稳定，不崩溃、不花屏

---

## 架构

```
定时器 IRQ0 (50Hz)
       │
       ▼
timer_handler()
       │
       └──→ need_reschedule = 1
                    │
                    ▼
irq_common_stub (irq_handler 返回后)
       │
       ├── need_reschedule == 0 → 正常返回 (popa + iret)
       │
       └── need_reschedule == 1
                    │
                    ├── hook_esp = ESP       (保存当前帧位置)
                    ├── call prepare_switch  (C 层选任务，返回新 ESP)
                    │     ├── current_task->esp = hook_esp
                    │     ├── current_task->state = READY
                    │     ├── next_task->state = RUNNING
                    │     └── return next_task->esp (0 = 不切换)
                    │
                    └── EAX != 0 → call do_switch(EAX)
                         ├── mov esp, [new_esp]  (切换栈)
                         ├── pop eax → DS/ES/FS/GS
                         ├── popa
                         ├── add esp, 8 (跳过 int_no/err_code)
                         └── iret → 恢复 EIP/CS/EFLAGS
                              │
                              ├── 已运行过的任务 → 回到被中断处继续
                              └── 全新任务 → task_trampoline → jmp [new_task_entry]
```

### 核心思想

1. 定时器每次 tick 设置 `need_reschedule = 1`
2. IRQ 汇编 stub 在 `irq_handler` 返回后、`popa`/`iret` 之前检查该标志
3. 若需要调度，C 层 `prepare_switch()` 选下一任务并返回其 ESP
4. 汇编 `do_switch()` 直接替换 ESP，执行 `popa + iret` 恢复新任务上下文
5. 上下文切换完全在 IRQ 处理流程中完成

---

## Round-Robin 算法

```
就绪队列：循环链表
          ┌──────────┐    next    ┌──────────┐    next    ┌──────────┐
          │  Task A  │ ────────→ │  Task B  │ ────────→ │  idle    │
          │ running  │           │ ready    │           │ ready    │
          └──────────┘ ←──────── └──────────┘ ←──────── └──────────┘
               next                  next                  next

算法:
  next = current->next
  while next != current && next->state != READY:
      next = next->next
  if next->state != READY → 不切换
  else → 切换到 next
```

idle 任务（主循环/Shell）作为循环链表的一个节点参与轮换。Shell 响应频率取决于就绪任务数量（例如 A、B、idle 三者轮换时，Shell 每 3 个 tick 才轮到一次，约 60ms @50Hz）。

---

## 数据结构

### Task Control Block (TCB)

```c
#define TASK_NAME_MAX    16
#define TASK_STACK_SIZE  4096
#define MAX_TASKS        16

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_FINISHED
} task_state_t;

typedef struct task {
    uint32_t pid;
    char name[TASK_NAME_MAX];
    task_state_t state;
    uint32_t esp;            // 内核栈指针（上下文切换时保存）
    uint32_t stack_base;     // 分配的栈基址（用于释放）
    uint32_t ticks_used;     // 累计被调度次数
    struct task* next;       // 循环链表指针
} task_t;
```

TCB 中不单独保存 EBP/EIP：
- **ESP 已经包含所有现场**：任务被切换时，ESP 指向 `irq_common_stub` 保存的 pusha 帧。`popa` 一次恢复所有通用寄存器。
- **EIP 由 iret 恢复**：寄存器帧下方是 int_no/err_code，再下面是 CPU 自动压入的 EIP/CS/EFLAGS。

### 初始任务帧布局

新建任务时，在分配的栈上**伪造一个 IRQ 帧**，让 `do_switch` 加载后看起来就像刚从 IRQ 返回：

```
偏移  内容                     说明
[56]  entry                    任务入口函数 (trampoline 读取后跳转)
[52]  EFLAGS = 0x0200          IF=1，允许中断
[48]  CS = 0x08                内核代码段
[44]  EIP = task_trampoline    首次 iret 跳转到 trampoline
[40]  err_code = 0             占位
[36]  int_no = 0               占位
[32]  EDI = 0                  pusha 寄存器 (全 0)
[28]  ESI = 0
[24]  EBP = 0
[20]  ESP = 0                  pusha 规范忽略此字段
[16]  EBX = 0
[12]  EDX = 0
[ 8]  ECX = 0
[ 4]  EAX = 0
[ 0]  DS = 0x10                内核数据段 ← ESP 指向这里
```

### 全局变量

| 变量 | 类型 | 说明 |
|------|------|------|
| `need_reschedule` | `volatile uint8_t` | 定时器设置为 1，`irq_common_stub` 检查并清除 |
| `hook_esp` | `uint32_t` | `irq_common_stub` 保存的当前 ESP（传给 `prepare_switch`） |
| `new_task_entry` | `uint32_t` | 新任务首次运行时，trampoline 读取的入口地址 |

---

## 文件清单

| 文件 | 说明 |
|------|------|
| `include/scheduler.h` | TCB 结构体、调度器 API 声明 |
| `kernel/scheduler.c` | 调度器实现（初始化、创建、选择、prepare_switch、task_exit） |
| `kernel/switch.asm` | `do_switch` (上下文切换) + `task_trampoline` (新任务入口) |
| `drivers/interrupts.asm` | `irq_common_stub` 中的调度 hook |
| `drivers/timer.c` | `timer_handler` 设置 `need_reschedule = 1` |
| `kernel/shell.c` | `schedtest [N]` 命令（创建测试任务，N 秒后自动退出） |
| `kernel/kernel.c` | `scheduler_init()` 调用 |

---

## schedtest 命令

```
TinyOS> schedtest        # 默认 10 秒
TinyOS> schedtest 30     # 运行 30 秒
```

实现细节：
- 创建 `task_a` 和 `task_b`，各自在循环中打印 "A " / "B " + `hlt`
- 通过 `schedtest_deadline` 全局变量设置绝对 tick 截止时间
- 每次循环检查 `timer_get_ticks() < schedtest_deadline`
- 到达截止时间后调用 `task_exit()` 标记为 FINISHED
- 时长上限 300 秒（5 分钟）

---

## 上下文切换详细流程

### 任务 A → 任务 B（两者均已运行过）

```
Task A 执行 hlt
  │
  ▼ IRQ0 触发
irq_common_stub:
  pusha; push ds; ...
  call irq_handler → timer_handler() → need_reschedule=1
  add esp, 8
  │
  ├── cmp [need_reschedule], 0 → 不为 0
  ├── mov [hook_esp], esp         ← 保存 Task A 的帧位置
  ├── call prepare_switch
  │     ├── current_task(A)->esp = hook_esp
  │     ├── A->state = READY
  │     ├── B->state = RUNNING
  │     └── return B->esp
  │
  ├── push eax                    ← B 的 ESP
  ├── call do_switch
  │     ├── add esp, 4            ← 丢弃 do_switch 返回地址
  │     ├── mov esp, [esp]        ← ESP = B->esp
  │     ├── pop eax → DS/ES/FS/GS ← 恢复 B 的段寄存器
  │     ├── popa                  ← 恢复 B 的通用寄存器
  │     ├── add esp, 8            ← 跳过 int_no/err_code
  │     └── iret                  ← 恢复 B 的 EIP/CS/EFLAGS
  │           │
  │           ▼
  │        Task B 从中断处继续执行
  │
  add esp, 4  (Task A 下次恢复时执行)
  pop eax → DS...
  popa
  add esp, 8
  iret → 回到 Task A 的 hlt
```

### 全新任务首次运行

```
do_switch 加载新任务 ESP → pop eax (DS) → popa → add esp, 8 → iret
  │
  ▼ iret 跳转到 task_trampoline (EIP 在伪造帧中设为 trampoline 地址)
task_trampoline:
  push dword task_exit    ← 返回地址安全网：若任务函数意外 return 则调用 task_exit
  jmp [new_task_entry]    ← 跳转到任务入口函数
  │
  ▼
任务入口函数开始执行
```

`new_task_entry` 由 `prepare_switch` 在 `ticks_used == 0` 时设置。

---

## 已知限制

### 1. task_exit() 仍有一次 IRQ 浪费

`task_exit()` 标记 FINISHED 并从链表摘除后，在 `hlt` 循环等待。下次 IRQ 仍在死亡任务的栈上运行，
但 `scheduler_pick_next()` 不再遍历到该任务（已从环中移除），因此只浪费一次 IRQ 入口/出口的开销，
不会在调度决策上浪费时间。

理想方案：`task_exit()` 直接构造栈帧并调用 `do_switch()`，完全跳过下一次 IRQ。

### 2. 完成的任务未从任务数组移除

FINISHED 状态的任务已从循环链表摘除，但 `tasks[]` 数组槽位保留到 `prepare_switch()`
下次运行时才释放栈页并清零槽位。这是有意设计：在 IRQ 上下文中延迟清理更安全。

### 3. 不支持优先级 / sleep / IPC

当前仅支持 Round-Robin 时间片轮转。无 `yield()`、`sleep()`、信号量等高级原语。

---

## 后续扩展

| 功能 | 说明 |
|------|------|
| `task_exit()` 直接切换 | 构造栈帧并调用 do_switch，避免死亡任务浪费 IRQ |
| `yield()` | 主动让出 CPU（int 0x81 或复用 int 0x80） |
| `task_sleep(ticks)` | 阻塞等待，超时后恢复 READY |
| 用户进程（Ring 3） | 任务切换支持特权级切换 + 独立页目录 |
| 优先级调度 | 多级反馈队列或固定优先级 |
| IPC | 管道、消息队列、共享内存 |
