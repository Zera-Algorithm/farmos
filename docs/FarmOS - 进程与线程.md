# FarmOS 进程与线程

## 概述

FarmOS 支持进程与线程机制，线程作为 CPU 调度的基本单位，进程作为线程的资源容器。主要相关文件目录如下：

```text
├── include
|   └── proc
|       ├── proc.h
|       ├── thread.h
|       ├── sched.h
|       └── sleep.h
|   
├── kern
|   └── proc
|       ├── Makefile
|       ├── procinit.c
|       ├── proc.c
|       ├── thread.c
|       ├── sleep.c
|       └── wait.c
```

## 进程与线程的控制块

在 FarmOS 中，线程是 CPU 调度的基本单位，线程控制块中存储了与进程运行相关的数据，定义如下：

```c
typedef struct thread {
	mutex_t td_lock; // 线程锁（已被全局初始化）
	proc_t *td_proc; // 线程所属进程（不保护，线程初始化后只读）
	TAILQ_ENTRY(thread) td_plist;  // 所属进程的线程链表链接（进程锁保护）
	TAILQ_ENTRY(thread) td_runq;   // 运行队列链接（线程锁保护）
	TAILQ_ENTRY(thread) td_sleepq; // 睡眠队列链接（线程锁保护）
	TAILQ_ENTRY(thread) td_freeq;  // 空闲队列链接（空闲队列锁保护）
	pid_t td_tid;		       // 线程 id（不保护，线程初始化后只读）
	state_t td_status;	       // 线程状态（线程锁保护）

#define td_startzero td_name // 清零属性区域开始指针
	char td_name[MAXPATH + 1]; // 线程名（不保护，线程初始化后只读） todo fork时溢出
	ptr_t td_wchan;		   // 线程睡眠等待的地址（线程锁保护）
	const char *td_wmesg; // 线程睡眠等待的原因（线程锁保护）
	u64 td_exitcode;      // 线程退出码（线程锁保护）
	sigevent_t *td_sig;   // 线程当前正在处理的信号（线程锁保护）
	trapframe_t td_trapframe; // 用户态上下文（不保护，该指针的值线程初始化后只读）
	context_t td_context;	// 内核态上下文（不保护，只被当前线程访问）
	bool td_killed;		// 线程是否被杀死（线程锁保护）
	sigset_t td_cursigmask; // 线程正在处理的信号屏蔽字（线程锁保护）
	u64 td_ctid;		// 线程 `clear tid flag`（线程锁保护）
#define td_startcopy td_sigmask
	sigset_t td_sigmask; // 线程信号屏蔽字（线程锁保护）
#define td_endcopy td_kstack
#define td_endzero td_kstack // 清零属性区域结束指针

	ptr_t td_kstack;	 // 内核栈所在页的首地址（已被全局初始化）
	sigeventq_t td_sigqueue; // 待处理信号队列（线程锁保护）
} thread_t;
```

在 FarmOS 中，进程是线程的资源容器，进程控制块中存储了与进程运行相关的数据，定义如下：

```c
typedef struct proc {
	mutex_t p_lock;		 // 进程锁（已被全局初始化）
	LIST_ENTRY(proc) p_list; // 空闲列表链接（空闲列表锁保护）
	TAILQ_HEAD(thread_tailq_head, thread) p_threads; // 拥有的线程队列（进程锁保护）

	state_t p_status;	  // 进程状态（进程锁保护）
	pid_t p_pid;		  // 进程 id（不保护，进程初始化后只读）
	ptr_t p_brk;		  // 进程堆顶
	pte_t *p_pt;		  // 线程用户态页表（不保护，进程初始化后只读）
	trapframe_t *p_trapframe; // 用户态上下文头指针（不保护，进程初始化后只读）

#define p_startzero p_times
	err_t p_exitcode; // 进程退出码（进程锁保护）
	times_t p_times;  // 线程运行时间（进程锁保护）
#define p_endzero p_fs_struct

	thread_fs_t p_fs_struct; // 文件系统相关字段（不保护）

	struct proc *p_parent;	      // 父线程（不保护，只会由父进程修改）
	LIST_HEAD(, proc) p_children; // 子进程列表（由进程锁保护）
	LIST_ENTRY(proc) p_sibling;   // 子进程列表链接（由父进程锁保护）
} proc_t;
```

## 线程的调度

在 FarmOS 中，线程是 CPU 调度的基本单位，线程的调度流程如下：

- 线程调用 `schedule()` 函数
    - 在 `schedule()` 函数中，线程调用 `ctx_switch()` 函数
        - 在 `ctx_switch()` 函数中，将 callee-save 寄存器的值保存到线程的 `context` 结构体中
- 此时，切换到内核的启动栈，并调用 `sched_switch()` 函数
    - 在 `sched_switch()` 函数中，调用 `sched_runnable()` 函数，获取下一个可运行的线程
        - 在 `sched_runnable()` 函数中，若上一个进程仍然可用，将其加入可运行队列
        - 然后判断是否有可运行的线程，若有，将其从可运行队列中取出并返回
    - 之后，将下一个线程的 `context` 结构体指针返回给 `ctx_switch()` 函数
- 在 `ctx_switch()` 函数中，将 `context` 结构体中的值恢复到 callee-save 寄存器中
- 最后，返回到线程的栈中，继续执行

对于调度时的并发与同步机制设计，此处不再赘述，在对应文档中进行叙述。

### 进程与线程的创建

在 FarmOS 中，创建进程有两种途径，第一种是在内核初始化时创建初始化进程，第二种是通过 `clone()` 系统调用实现 `fork` 来创建进程。

- 使用 `proc_create()` 创建初始化进程
    - 第一步，申请一个进程控制块、一个线程控制块
    - 第二步，将线程加入到进程中
    - 第三步，加载进程的用户代码
    - 第四步，初始化进程的文件系统相关内容
    - 第五步，将初始线程加入运行队列，完成进程的创建

- 使用 `proc_fork()` 复制进程
	- 第一步，申请一个进程控制块、一个线程控制块
	- 第二步，将线程加入到进程中
	- 第三步，遍历进程页表，分情况使用写时复制复制页表项
	- 第四步，将父进程的文件系统相关内容复制到子进程中
	- 第五步，创建父子关系、设置子进程初始现场
	- 第六步，将子进程加入运行队列，完成进程的创建

- 在申请线程控制块时，在函数内部会初始化好内核线程部分，即初始化线程的内核现场

### 进程和线程的回收

在 FarmOS 中，线程的回收是通过 `td_destroy()` 实现的。有以下几种情况会触发线程的回收：

- 线程执行 `exit()` 系统调用
- 线程被杀死（SIGKILL）
- 线程在用户态触发了未捕获的异常

在 `td_destroy()` 函数中，会将线程从进程中移除，当进程中没有线程时，会触发进程的回收。进程的回收是通过 `proc_destroy()` 实现的。在 `proc_destroy()` 中会回收进程的资源，而会保留进程控制块中的一些信息，以便父进程获取，在父进程退出时，会读取这些信息，以便父进程获取子进程的退出码，随后父进程会释放掉该僵尸进程的进程控制块。