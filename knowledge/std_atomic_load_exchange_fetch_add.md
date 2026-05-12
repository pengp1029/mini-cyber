# std::atomic 核心操作：load / exchange / fetch_add

> 沉淀日期: 2026-05-12

## 1. load — 原子读取

**功能**：安全地读取原子变量的当前值，保证不会读到"写了一半"的中间状态。

**原理**：在 x86 上，对齐的 load 本身就是原子的，但 `load` 的关键作用是**内存序约束**——它告诉编译器和 CPU 不要将这次读操作与其他操作乱序。底层可能插入 fence 指令或阻止编译器重排。

**使用**：

```cpp
std::atomic<int> counter{42};

// 默认 memory_order_seq_cst（最强序，全局一致）
int val = counter.load();

// 放松序——只保证原子性，不保证与其他变量的可见顺序
int val2 = counter.load(std::memory_order_relaxed);

// acquire 序——保证"此 load 之后的读写"不会被重排到 load 之前
// 常用于读锁/读标志位
int val3 = counter.load(std::memory_order_acquire);
```

---

## 2. exchange — 原子交换（读-改-写）

**功能**：将原子变量设为新值，同时**返回旧值**。整个"读旧+写新"是一个不可分割的操作。

**原理**：x86 上编译为 `XCHG` 指令，该指令自带隐式 lock 前缀，独占缓存行完成交换。ARM/RISC-V 上用 LL/SC（load-linked / store-conditional）循环实现。

**使用**：

```cpp
std::atomic<bool> lock_flag{false};

// 自旋锁的加锁：把 flag 设为 true，如果旧值是 false 说明抢到了
while (lock_flag.exchange(true, std::memory_order_acquire)) {
    // 旧值为 true → 别人持有锁，自旋等待
}

// 解锁
lock_flag.store(false, std::memory_order_release);
```

典型场景：自旋锁、单次初始化标志、swap 指针（无锁数据结构里换头节点）。

---

## 3. fetch_add — 原子加（读-改-写）

**功能**：将原子变量加上指定值，**返回加之前的旧值**。整个"读旧值 → 计算新值 → 写回"不可被打断。

**原理**：x86 上编译为 `LOCK XADD`（带 lock 前缀的交换加法），锁住缓存行完成操作。ARM 上同样用 LL/SC 循环：`LDXR` 读 → 加 → `STXR` 写，若期间被其他核修改则重试。

**使用**：

```cpp
std::atomic<int> seq{0};

// 多线程安全地分配递增 ID
int my_id = seq.fetch_add(1, std::memory_order_relaxed);
// my_id 拿到的是加之前的值（0, 1, 2, ...）

// 引用计数减一，判断是否最后一个持有者
std::atomic<int> ref_count{3};
if (ref_count.fetch_add(-1, std::memory_order_acq_rel) == 1) {
    // 旧值为 1 → 减完变成 0 → 我是最后一个，负责释放资源
    delete resource;
}
```

---

## 对比总结

| 操作 | 类型 | 返回值 | 典型用途 |
|------|------|--------|----------|
| `load` | 只读 | 当前值 | 读状态/标志位 |
| `exchange` | 读-改-写 | 旧值 | 自旋锁、swap 指针 |
| `fetch_add` | 读-改-写 | 加之前的旧值 | 计数器、ID 分配、引用计数 |

**共同点**：都接受一个 `memory_order` 参数控制可见性语义。默认 `seq_cst` 最安全但最慢；性能敏感路径按需降级到 `relaxed`/`acquire`/`release`。

---

## 补充：memory_order 速查

| 内存序 | 含义 | 适用场景 |
|--------|------|----------|
| `relaxed` | 只保证原子性，无顺序约束 | 纯计数器、统计 |
| `acquire` | 本次 load 之后的读写不会重排到之前 | 读锁、读标志 |
| `release` | 本次 store 之前的读写不会重排到之后 | 写锁、发布数据 |
| `acq_rel` | 同时 acquire + release | 读-改-写居中节点 |
| `seq_cst` | 全局全序，所有线程看到相同顺序 | 默认值，简单但最慢 |
