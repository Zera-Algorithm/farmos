# FarmOS 同步互斥

## 概述

FarmOS 支持多核，因此需要实现多核同步互斥机制。FarmOS 中的同步互斥机制主要通过互斥锁实现，主要相关文件目录如下：

```text
├── include
|   ├── lock
|   |   ├── lock.h
|   |   └── mutex.h
|   └── proc
|       └── sleep.h
└── kernel
    ├── lock
    |   ├── lock.c
    |   └── mutex.c
    └── proc
        └── sleep.c
```

## 互斥锁机制

FarmOS 的同步互斥机制主要通过互斥锁实现，锁和互斥锁的定义如下：

```c
typedef struct lock_object {
	const char *lo_name;
	u64 lo_locked;
	void *lo_data;
} lock_object_t;

typedef struct mutex {
	lock_object_t mtx_lock_object;
	thread_t *mtx_owner; // 仅在睡眠锁中使用
	bool mtx_debug;	     // 是否输出调试信息
	u8 mtx_type;	     // 锁的类型
	u8 mtx_depth;	     // 锁的深度（意义与类型相关）
} mutex_t;
```

### 锁

每个互斥锁结构体中都含有一个锁，该锁的操作使用了原子指令，是互斥锁的基础。锁提供以下几种功能接口：

```c
// 中断使能栈操作
void lo_critical_enter();
void lo_critical_leave();

// 原子操作接口
void lo_acquire(lock_object_t *lo);
bool lo_try_acquire(lock_object_t *lo);
void lo_release(lock_object_t *lo);
bool lo_acquired(lock_object_t *lo) __attribute__((warn_unused_result));
```

进入或离开临界区时，使用 `lo_critical_enter()` 和 `lo_critical_leave()` 操作中断使能栈，禁止中断；`lo_acquire()` 和 `lo_try_acquire()` 用于获取锁，`lo_release()` 用于释放锁。`lo_acquired()` 用于判断锁是否被获取。

### 互斥锁

对于互斥锁，根据使用场景的不同，可以进行不同的配置，在使用互斥锁前需要对其进行初始化，初始化接口如下：

```c
#define MTX_SPIN 0x01
#define MTX_SLEEP 0x02
#define MTX_RECURSE 0x04

void mtx_init(mutex_t *m, const char *name, bool debug, u8 type);
```

可以根据需要开关互斥锁的调试信息，设置互斥锁的类型，选择睡眠锁或自旋锁，以及是否允许递归获取锁。

## 同步与互斥

### 线程调度

线程调度有关的锁主要有：
- 线程锁
- 可运行队列锁

在线程调度时，锁的操作如下：
- 获取线程锁，进入 `schedule()` 函数
- 获取可运行队列锁，可能将当前线程加入可运行队列，随后释放线程锁
- 获取可运行队列中下一个线程的线程锁，随后释放可运行队列锁
- 运行下一个线程，带有线程锁并离开 `schedule()` 函数

### 线程睡眠和唤醒

线程睡眠有关的锁主要有：
- 线程锁
- 睡眠队列锁
- 可运行队列锁
- 某个传入的自旋互斥锁

在线程睡眠时，锁的操作如下：
- 获取睡眠队列锁
- 释放某个传入的自旋互斥锁
- 获取线程锁，修改线程状态为睡眠
- 释放睡眠队列锁
- 带有线程锁并调用 `schedule()` 函数让出 CPU

在线程被唤醒时，锁的操作如下：
- 获取睡眠队列锁
- 遍历睡眠队列，获取线程锁，检查状态，若需要唤醒则移出睡眠队列
- 释放线程锁
- 获取可运行队列锁，将线程加入可运行队列
- 释放睡眠队列锁
- 释放可运行队列锁

在睡眠与唤醒机制中，保证不丢失唤醒的关键在于睡眠队列锁。在获取睡眠队列锁后，保证其它线程无法进行唤醒操作。唤醒者需要先获取睡眠队列锁，再获取线程锁。睡眠者只有在让出 CPU 后才会释放线程锁，此时唤醒者才能获取线程锁，检查线程状态并进行唤醒。

### 进程等待

进程等待有关的锁主要有：
- 等待锁
- 进程锁

在进程等待时，锁的操作如下：
- 获取等待锁
- 获取进程锁，遍历进程的子进程，对子进程加锁
- 找到需要等待的子进程
    - 若子进程已退出，则释放进程锁，释放等待锁，返回
    - 若子进程未退出，则释放子进程锁，释放进程锁，持有等待锁进入睡眠

在进程结束时，锁的操作如下：
- 获取等待锁
- 获取进程锁
- 设置进程状态为僵尸
- 释放进程锁
- （后续与等待无关的操作）
- 唤醒父进程
- 释放等待锁

在等待机制中，保证不丢失等待的关键在于等待锁。在获取等待锁后，保证其它线程不会在此期间变为僵尸态，等待者会携带等待锁进入睡眠，此时唤醒者才能获取等待锁，将自己设置为僵尸态并唤醒父进程。