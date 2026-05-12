# 共享内存（Shared Memory）

> 沉淀日期: 2026-05-12

## 一、为什么需要共享内存

进程间通信（IPC）有多种方式：管道、消息队列、socket 等，但它们都需要**数据拷贝**：

```
进程A用户空间 → 内核缓冲区 → 进程B用户空间  (至少两次拷贝)
```

当数据量大、通信频繁时，这些拷贝成为性能瓶颈。共享内存的动机就是：**让多个进程直接访问同一块物理内存，零拷贝通信。**

典型场景：音视频处理、数据库缓存（如 PostgreSQL shared buffers）、自动驾驶多模块间传大帧数据。

## 二、原理

Linux 中每个进程有独立的虚拟地址空间，通过页表映射到物理内存：

```
进程A虚拟地址空间          物理内存           进程B虚拟地址空间
┌──────────┐          ┌──────────┐          ┌──────────┐
│ 0x7f...  │──────────│          │──────────│ 0x7f...  │
│ (映射区)  │    页表    │ 同一块物理页 │    页表    │ (映射区)  │
└──────────┘          └──────────┘          └──────────┘
```

核心步骤：
1. **内核分配一块物理内存**（或在 tmpfs 上创建一个文件对象）
2. **各进程将该物理内存映射到自己的虚拟地址空间**（通过 `mmap` 或 `shmat`）
3. 之后进程对该地址的读写就是直接操作同一块物理页，**不经过内核中转**

因为没有内核参与数据搬运，所以需要用户自己做**同步**（信号量、互斥锁、原子操作等），否则会有竞态。

## 三、使用 Demo（POSIX 共享内存）

Linux 推荐使用 POSIX API（`shm_open` + `mmap`），比 System V 的 `shmget/shmat` 更现代。

**写端（producer.c）：**

```c
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define SHM_NAME "/my_shm"
#define SHM_SIZE 4096

int main() {
    // 1. 创建共享内存对象（本质是 /dev/shm 下的一个文件）
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, SHM_SIZE);

    // 2. 映射到本进程地址空间
    char *ptr = mmap(NULL, SHM_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    // 3. 直接写
    const char *msg = "Hello from producer!";
    memcpy(ptr, msg, strlen(msg) + 1);
    printf("Written: %s\n", msg);

    munmap(ptr, SHM_SIZE);
    return 0;
}
```

**读端（consumer.c）：**

```c
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

#define SHM_NAME "/my_shm"
#define SHM_SIZE 4096

int main() {
    // 1. 打开已有的共享内存对象
    int fd = shm_open(SHM_NAME, O_RDONLY, 0);

    // 2. 映射
    char *ptr = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    // 3. 直接读
    printf("Read: %s\n", ptr);

    munmap(ptr, SHM_SIZE);
    shm_unlink(SHM_NAME); // 用完删除
    return 0;
}
```

**编译运行：**

```bash
gcc producer.c -o producer -lrt
gcc consumer.c -o consumer -lrt
./producer    # 先写
./consumer    # 再读，输出 "Read: Hello from producer!"
```

## 四、对比总结

| 方式 | 拷贝次数 | 需要同步 | 适合场景 |
|------|---------|---------|---------|
| 管道/socket | 2次（用户→内核→用户） | 内核保证顺序 | 小数据、简单通信 |
| 共享内存 | 0次 | 用户自己做 | 大数据、高频通信 |

一句话：共享内存用"多个进程的页表指向同一物理页"实现零拷贝，代价是你得自己处理并发安全。
