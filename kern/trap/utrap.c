#include <dev/timer.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <mm/memlayout.h>
#include <proc/cpu.h>
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

#define entry_user_ret ((void (*)(u64))(TRAMPOLINE + (userRet - trampoline)))

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
	// 获取中断或异常的原因并输出
	register_t cause = utrap_info();
	register_t exc_code = (cause & SCAUSE_EXC_MASK);
	thread_t *td = cpu_this()->cpu_running;

	// 切换内核异常入口
	w_stvec((uint64)kernelvec);

	// 处理中断或异常
	if (UTRAP_IS_INT(cause)) {
		// 用户态中断
		if (exc_code == INTERRUPT_TIMER) {
			// 时钟中断
			log(LEVEL_GLOBAL, "Timer Int On Hart %d\n", cpu_this_id());
			// 先设置下次时钟中断的触发时间，再进行调度
			timerSetNextTick();
			yield();
		} else {
			warn("unknown interrupt %d, ignored.\n", exc_code);
		}
	} else {
		// 用户态异常
		if (exc_code == EXCCODE_SYSCALL) {
			// 系统调用，属于内核线程范畴，允许中断 todo
			syscall_entry(td->td_trapframe);
		} else if (exc_code == EXCCODE_PAGE_FAULT) {
			// 页错误，属于内核线程范畴，允许中断 todo
			if (page_fault_handler(r_stval() & ~(PAGE_SIZE - 1))) {
				// 页错误处理失败，杀死进程
				warn("page fault on pid = %d, kill it.\n", td->td_pid);
				sys_exit(-1);// errcode todo
			}
		} else {
			printReg(td->td_trapframe);
			error("uncaught exception.\n"
			      "\tcpu: %d\n"
			      "\tExcCode: %d\n"
			      "\tCause: %s\n"
			      "\tSepc: 0x%016lx (kern/kernel.asm)\n"
			      "\tStval(bad memory address): 0x%016lx\n",
			      "Curenv: pid = 0x%08lx, name = %s\n", cpu_this_id(), exc_code,
			      excCause[exc_code], r_sepc(), r_stval(), td->td_pid, td->td_name);
		}
	}

	// 中断或异常处理完毕，从现场恢复用户态
	utrap_return();
}

/**
 * @brief 从内核态返回某个用户的用户态
 * @note 需要已存储了用户态下的所有寄存器现场，并已加载内核页表并处在内核栈。
 */
void utrap_return() {
	// 当前应关闭中断
	assert(intr_get() == 0);

	// 获取当前应该返回的用户线程
	thread_t *td = cpu_this()->cpu_running;

	// ue5: 将内核页表地址存入 TRAPFRAME
	td->td_trapframe->kernel_satp = r_satp();

	// ue4: 将内核号存入 TRAPFRAME
	td->td_trapframe->hartid = cpu_this_id();

	// ue3: 将内核线程入口、内核栈地址、内核号存入 TRAPFRAME
	td->td_trapframe->trap_handler = (u64)utrap_entry;
	td->td_trapframe->kernel_sp = td->td_kstack + TD_KSTACK_SIZE;

	// ue0: 为硬件行为做好准备，切换用户异常入口
	u64 trampolineUserVec = TRAMPOLINE + (userVec - trampoline);
	w_stvec(trampolineUserVec);

	// ue0: 为硬件行为做好准备，设置 sret 后开启中断并返回用户态
	u64 sstatus = r_sstatus();
	sstatus &= ~SSTATUS_SPP; // SPP = 0: 用户状态
	sstatus |= SSTATUS_SPIE; // SPIE = 1: 开中断
	w_sstatus(sstatus);

	// ue5: 计算用户页表 SATP 并跳转至汇编用户态异常出口
	u64 user_satp = MAKE_SATP(td->td_pt);
	entry_user_ret(user_satp);
}

void utrap_firstsched() {
	mtx_unlock(&cpu_this()->cpu_running->td_lock);
	utrap_return();
}
