# TinyOS 演化路线图

## 当前状态 (v0.1)

✅ 已完成：
- Multiboot 引导
- VGA 文本显示
- 键盘输入（中断驱动）
- 定时器中断
- IDT/PIC 中断管理
- 串口调试输出

---

## 第一阶段：核心基础设施 (v0.2 - v0.3)

### 1.1 内存管理
**目标**：实现物理内存和虚拟内存管理

- [ ] **物理内存检测**
  - 通过 BIOS/GRUB 获取内存映射 (E820)
  - 实现内存分配器（位图或链表）
  
- [ ] **分页机制**
  - 启用 CR3 页目录
  - 4KB 页框管理
  - 虚拟地址映射
  
- [ ] **内核堆**
  - `kmalloc()` / `kfree()` 实现
  - 基于 slab 或伙伴系统

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

- [ ] 页故障处理 (Page Fault)
- [ ] 通用保护故障 (GPF)
- [ ] 除零错误
- [ ] 堆栈溢出检测
- [ ] 蓝屏/崩溃信息

### 1.3 字符串格式化输出
**目标**：支持 `printf` 风格格式化

- [ ] `sprintf()` / `printf()` 实现
- [ ] 支持 `%d`, `%x`, `%s`, `%c`, `%p`
- [ ] 格式化数字显示（十进制/十六进制）

---

## 第二阶段：多任务 (v0.4 - v0.5)

### 2.1 进程/线程管理
**目标**：实现基本的抢占式多任务

- [ ] **任务控制块 (TCB)**
  ```c
  struct task {
      uint32_t pid;
      uint32_t esp, ebp;
      uint32_t eip;
      uint32_t eax, ebx, ecx, edx;
      uint32_t flags;
      struct task* next;
  };
  ```

- [ ] **上下文切换**
  - 保存/恢复寄存器
  - 切换页目录
  - 切换内核栈

- [ ] **调度器**
  - 时间片轮转 (Round Robin)
  - 基于定时器中断 (IRQ0)
  - 优先级调度（可选）

- [ ] **系统调用**
  - `fork()` - 创建进程
  - `exit()` - 退出进程
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
**目标**：从内核模式切换到用户模式运行程序

- [ ] **特权级切换**
  - Ring 0 (内核) -> Ring 3 (用户)
  - 系统调用门 (System Call Gate)
  - `int 0x80` 或 `sysenter`

- [ ] **用户程序加载**
  - ELF 格式解析
  - 动态链接（可选）

### 4.2 Shell
**目标**：命令行解释器

- [ ] 命令解析
- [ ] 内建命令：`echo`, `ls`, `cat`, `ps`, `kill`
- [ ] 程序执行：`fork` + `exec`
- [ ] 管道支持：`cmd1 | cmd2`

### 4.3 标准库
**目标**：C 标准库子集

- [ ] `libc` 基础：`malloc`, `free`, `strlen`, `strcpy`
- [ ] 文件操作：`fopen`, `fread`, `fwrite`, `fclose`
- [ ] 进程控制：`fork`, `exec`, `wait`, `exit`

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

- [ ] **VGA 图形模式**
  - 320x200x256 (Mode 13h)
  - 或 VESA 高分辨率

- [ ] **窗口系统**
  - 窗口管理器
  - 事件驱动（鼠标、键盘）
  - 基本控件（按钮、标签、文本框）

### 5.3 高级功能

- [ ] **动态链接器**
- [ ] **多核/SMP 支持**
- [ ] **USB 驱动**
- [ ] **声音驱动**
- [ ] **PCI 总线枚举**

---

## 技术演进路径

```
v0.1 (当前) ──> v0.2 ──> v0.3 ──> v0.4 ──> v0.5 ──> v0.6 ──> v0.7 ──> v0.8 ──> v1.0
  │              │         │         │         │         │         │         │
  ▼              ▼         ▼         ▼         ▼         ▼         ▼         ▼
引导+VGA      内存管理   异常处理   多任务    调度器    VFS      用户态   网络
键盘中断      分页机制   printf    TCB      系统调用   RAMFS    Shell    GUI
定时器        kmalloc   蓝屏      上下文    fork/exec  FAT12    libc     TCP/IP
```

---

## 每个阶段的检查清单

### 完成标准

| 阶段 | 必须完成 | 验证方法 |
|------|----------|----------|
| v0.2 | kmalloc/kfree 工作 | 分配内存并读写测试 |
| v0.3 | 页故障正确处理 | 访问无效地址触发蓝屏 |
| v0.4 | 两个任务交替运行 | 任务A和B轮流打印 |
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
