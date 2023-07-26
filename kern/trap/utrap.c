#include <dev/plic.h>
#include <dev/timer.h>
#include <dev/virtio.h>
#include <fs/dirent.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/proc.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <riscv.h>
#include <sys/syscall.h>
#include <trap/trap.h>

extern void kernelvec();

extern void yield();

#define SCAUSE_EXCEPTION 0
#define SCAUSE_INTERRUPT 1
#define INTERRUPT_TIMER 5
#define INTERRUPT_EXTERNEL 9

#define SCAUSE_INT_MASK (1ul << 63)
#define SCAUSE_EXC_MASK ((1ul << 63) - 1)
#define UTRAP_IS_INT(scause) ((scause)&SCAUSE_INT_MASK)

extern char trampoline[], userVec[], userRet[];

static char *excCause[] = {"Instruction address misaligned",
			   "Instruction access fault",
			   "Illegal instruction",
			   "Breakpoint",
			   "Load address misaligned",
			   "Load access fault",
			   "Store address misaligned",
			   "Store access fault",
			   "Environment call from U-mode",
			   "Environment call from S-mode",
			   "Undefined Exception",
			   "Environment call from M-mode",
			   "Instruction page fault",
			   "Load page fault",
			   "Undefined Exception",
			   "Store page fault"};

#define hart_tf_uva(hartid) (((trapframe_t *)TRAPFRAME) + (hartid))

#define entry_user_ret(tf, satp)                                                                   \
	(((void (*)(trapframe_t *, u64))(TRAMPOLINE + (userRet - trampoline)))(tf, (satp)))

static register_t utrap_info() {
	// 获取中断或异常的原因并输出
	uint64 scause = r_scause();
	// uint64 type = (scause >> 63ul);
	// uint64 exc_code = (scause & SCAUSE_EXC_MASK);
	// log(LEVEL_GLOBAL, "user trap: cpu = %d, type = %ld, exc_code = %ld(%s)\n", cpuid(), type,
	//     exc_code, excCause[exc_code]);
	return scause;
}

/**
 * @brief 在用户态下，触发中断或异常时的 C 函数入口。
 * @note 仅被 tranpoline.S 中的 userVec 调用
 * @note
 * 调用本函数之前已存储了用户态下的所有寄存器现场，并已加载内核页表并切换到对应内核线程的内核栈。
 */
void utrap_entry() {
	// 保存 TRAPFRAME 至线程控制块
	thread_t *td = cpu_this()->cpu_running;
	td->td_trapframe = td->td_proc->p_trapframe[cpu_this_id()];
	// 获取中断或异常的原因并输出
	register_t cause = utrap_info();
	register_t exc_code = (cause & SCAUSE_EXC_MASK);
	// 切换内核异常入口
	w_stvec((uint64)kernelvec);

	// 处理中断或异常
	if (UTRAP_IS_INT(cause)) {
		// 用户态中断
		if (exc_code == INTERRUPT_TIMER) {
			// 时钟中断
			utrap_timer();
		} else if (exc_code == INTERRUPT_EXTERNEL) {
			// 外部中断
			trap_device();
		} else {
			warn("unknown interrupt %d, ignored.\n", exc_code);
		}
	} else {
		// 用户态异常
		if (exc_code == EXCCODE_SYSCALL) {
			// 系统调用，属于内核线程范畴，允许中断 todo
			syscall_entry(&td->td_trapframe);
		} else if (exc_code == EXCCODE_PAGE_FAULT) {
			// 页错误，属于内核线程范畴，允许中断 todo
			trap_pgfault();
		} else {
			// 其他情况
			warn("uncaught exception.\n");
			printReg(&td->td_trapframe);
			warn("Curenv: pid = 0x%08lx, name = %s\n", td->td_tid, td->td_name);
			// 不是很清楚为什么传入td->td_pid和td->td_name两个参数之后，
			// cpu的输出变为乱码，访问excCause数组出现load page fault
			// 可能与参数的数目过多有关系，因此将输出分拆为两段
			warn("\tcpu: %d\n"
			     "\tExcCode: %d\n"
			     "\tCause: %s\n"
			     "\tSepc: 0x%016lx (kern/kernel.asm)\n"
			     "\tStval(bad memory address): 0x%016lx\n",
			     cpu_this_id(), exc_code, excCause[exc_code], r_sepc(), r_stval());
			sys_exit(-1); // errcode todo
		}
	}

	// 中断或异常处理完毕，从现场恢复用户态
	log(LEVEL_MODULE, "before %s return to user\n", td->td_name);
	utrap_return();
}

/**
 * @brief 从内核态返回某个用户的用户态
 * @note 需要已存储了用户态下的所有寄存器现场，并已加载内核页表并处在内核栈。
 */
void utrap_return() {
	// 当前应关闭中断
	assert(intr_get() == 0);

	// 先检查信号
	sig_check();

	// 获取当前应该返回的用户线程
	thread_t *td = cpu_this()->cpu_running;

	// ue5: 将内核页表地址存入 TRAPFRAME
	td->td_trapframe.kernel_satp = r_satp();

	// ue4: 将内核号存入 TRAPFRAME
	td->td_trapframe.hartid = cpu_this_id();

	// ue3: 将内核线程入口、内核栈地址、内核号存入 TRAPFRAME
	td->td_trapframe.trap_handler = (u64)utrap_entry;
	td->td_trapframe.kernel_sp = td->td_kstack + TD_KSTACK_SIZE;
	// ue3+: 拷贝 TRAPFRAME 至用户空间
	trapframe_t *harttf = &td->td_proc->p_trapframe[cpu_this_id()];
	*harttf = td->td_trapframe;

	// ue0: 为硬件行为做好准备，切换用户异常入口
	u64 trampolineUserVec = TRAMPOLINE + (userVec - trampoline);
	w_stvec(trampolineUserVec);

	// ue0: 为硬件行为做好准备，设置 sret 后开启中断并返回用户态
	u64 sstatus = r_sstatus();
	sstatus &= ~SSTATUS_SPP; // SPP = 0: 用户状态
	sstatus |= SSTATUS_SPIE; // SPIE = 1: 开中断
	w_sstatus(sstatus);

	log(LEVEL_GLOBAL, "tid = %lx, before utrap return, a0 = 0x%lx\n",
	    cpu_this()->cpu_running->td_tid, harttf->a0);

	// ue5: 计算用户页表 SATP 并跳转至汇编用户态异常出口
	u64 user_satp = MAKE_SATP(td->td_proc->p_pt);
	entry_user_ret(hart_tf_uva(cpu_this_id()), user_satp);
}

int is_first_thread = 1;
mutex_t first_thread_lock;

void utrap_firstsched() {
	mtx_unlock(&cpu_this()->cpu_running->td_lock);
	assert(cpu_this()->cpu_lk_depth == 0);

	mtx_lock(&first_thread_lock);
	if (is_first_thread == 1) {
		is_first_thread = 0;
		mtx_unlock(&first_thread_lock);

		// 初始化文件系统（需要持有自旋锁）
		virtio_disk_init();
		bufInit();
		dirent_init();
		init_root_fs();
		init_files();
		is_first_thread = 2;
		wakeup(&is_first_thread);
	} else if (is_first_thread == 0) {
		sleep(&is_first_thread, &first_thread_lock, "utrap_firstsched");
		mtx_unlock(&first_thread_lock);
	} else {
		mtx_unlock(&first_thread_lock);
	}

	warn("proc: %s, epc = %lx, sp = %lx, p_brk = %lx\n", cpu_this()->cpu_running->td_name,
	     cpu_this()->cpu_running->td_trapframe.epc, cpu_this()->cpu_running->td_trapframe.sp,
	     cpu_this()->cpu_running->td_brk);
	utrap_return();
}
