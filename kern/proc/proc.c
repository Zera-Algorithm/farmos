#include <lib/log.h>
#include <lib/string.h>
#include <proc/proc.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <signal/signal.h>

mutex_t pid_lock;

static u64 pid_alloc(proc_t *p) {
	static u64 cnt = 0; // todo tid lock
	mtx_lock(&pid_lock);
	cnt += 1;
	u64 new_pid = PID_GENERATE(cnt, p - procs);
	mtx_unlock(&pid_lock);
	return new_pid;
}

/**
 * @brief 分配一个进程，该进程具有一个页表。
 * @note 页表中仅有 Trapframe 和 Trampoline 两个映射。
 */
proc_t *proc_alloc() { // static
	// 从空闲进程队列中取出一个进程
	plist_critical_enter(&proc_freelist);
	if (LIST_EMPTY(&proc_freelist.pl_list)) {
		plist_critical_exit(&proc_freelist);
		error("no free proc");
	}

	proc_t *p = LIST_FIRST(&proc_freelist.pl_list);
	LIST_REMOVE(p, p_list);
	proc_lock(p);
	plist_critical_exit(&proc_freelist);
	// 已从空闲进程队列中取出进程，需要初始化进程的各个字段

	// 初始化进程字段
	p->p_status = USED;
	p->p_pid = pid_alloc(p);
	memset(&p->p_startzero, 0, rangeof(proc_t, p_startzero, p_endzero));

	// 初始化进程用户态页表（完成 Trampoline/Trapframe 映射）
	proc_initupt(p);

	return p;
}

static void proc_uvminit(proc_t *p, thread_t *inittd, const char *name, const void *bin, size_t size) {
	// 初始化用户地址空间
	proc_initustack(p, inittd, TD_USTACK);

	// 加载代码段
	proc_initucode_by_binary(p, inittd, bin, size, NULL);

	// 初始化用户线程信息
	assert(strlen(name) <= MAXPATH);
	safestrcpy(inittd->td_name, name, sizeof(inittd->td_name));
}

void proc_addtd(proc_t *p, thread_t *td) {
	// 需要已获取二者的锁
	td->td_proc = p;
	TAILQ_INSERT_HEAD(&p->p_threads, td, td_plist);
}

void proc_create(const char *name, const void *bin, size_t size) {
	// 获取一个进程和一个线程
	proc_t *p = proc_alloc();
	thread_t *inittd = td_alloc();

	// 将线程与进程控制块绑定
	proc_addtd(p, inittd);

	// 初始化用户地址空间
	proc_uvminit(p, inittd, name, bin, size);

	// 初始化进程的文件系统结构体
	init_thread_fs(&p->p_fs_struct);

	// 设置初始化线程
	inittd->td_status = RUNNABLE;

	// Note：ProcCreate不需要将参数压栈
	// // 初始化参数
	// proc_setustack(inittd, p->p_pt, 0, NULL, 0, NULL);

	// 将初始线程加入调度队列
	tdq_critical_enter(&thread_runq);
	TAILQ_INSERT_TAIL(&thread_runq.tq_head, inittd, td_runq);
	mtx_unlock(&inittd->td_lock);
	proc_unlock(p);
	tdq_critical_exit(&thread_runq);
}

void proc_free(proc_t *p) {
	assert(proc_hold(p));
	// 清空进程的非资源字段
	p->p_status = UNUSED;
	p->p_pid = 0;
	p->p_parent = NULL;

	// 将进程加入空闲进程队列
	plist_critical_enter(&proc_freelist);
	LIST_INSERT_HEAD(&proc_freelist.pl_list, p, p_list);
	plist_critical_exit(&proc_freelist);
}

static void proc_recycle(proc_t *p) {
	// 回收进程的fs资源
	// 需要保证thread里面的fs结构在进程结束时不会被访问，以保证原子性（现状是，只有本进程不处于结束状态时才会通过自己的系统调用访问自己的fs资源）
	// 放在td_recycle前面是因为要避免因为睡眠唤醒，把进程的td_status改为RUNNABLE，而不是维持ZOMBIE
	recycle_thread_fs(&p->p_fs_struct);
	proc_recycleupt(p);
	p->p_status = ZOMBIE;
}

void proc_destroy(proc_t *p, err_t exitcode) {
	p->p_exitcode = exitcode;

	// 拿等待锁，防止其它线程在此期间调用 wait
	proc_unlock(p);

	// 此时进程内只有一个线程，不需要对回收进程资源加锁
	proc_recycle(p);

	mtx_lock(&wait_lock);

	// 处理子进程资源
	proc_t *child;
	// 此时进程内只有一个线程，不需要对进程资源加锁
	LIST_UNTIL_EMPTY(child, &p->p_children) {
		proc_lock(child);
		LIST_REMOVE(child, p_sibling);
		// 根据子进程状态处理
		if (child->p_status == ZOMBIE) {
			proc_free(child);
		} else {
			warn("haven't implement init, child %d is still alive\n", child->p_pid);
			child->p_parent = &procs[PID_TO_INDEX(PID_INIT)];
			// child->td_parent = TID_INIT;
			// todo: insert to init's childlist and wake up init
		}
		proc_unlock(child);
	}

	// 通知父进程（父进程 wait 时等待的是父线程(self)的指针）
	if (p->p_pid != PID_INIT) {
		warn("proc %08x destroying, send SIGCHLD to parent %08x\n", p->p_pid, p->p_parent->p_pid);
		sig_send_proc(p->p_parent, SIGCHLD);
		wakeup(p->p_parent);
	}

	proc_lock(p);
	mtx_unlock(&wait_lock);
}
