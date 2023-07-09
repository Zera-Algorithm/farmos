#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/syscall.h>
#include <lib/transfer.h>
#include <lib/log.h>

void sys_exit(err_t code) {
	thread_t *td = cpu_this()->cpu_running;
	log(LEVEL_GLOBAL, "thread %s exit with code %d\n", td->td_name, code);
	td->td_exitcode = code;
	mtx_lock(&td->td_lock);
	td_destroy();
}

typedef u64 fileid_t;
extern fileid_t file_load(const char *path, void **bin, size_t *size);
extern void file_unload(fileid_t file);

static u64 argc_count(pte_t *pt, char **argv) {
	u64 argc = 0;
	void *ptr;
	do {
		copy_in(pt, (u64)argv[argc++], &ptr, sizeof(ptr));
	} while (ptr != NULL);
	return argc - 1;
}

void sys_exec(u64 path, char **argv, u64 envp) {
	// 拷贝可执行文件路径
	thread_t *td = cpu_this()->cpu_running;
	char buf[MAX_PROC_NAME_LEN];
	copy_in_str(td->td_pt, path, buf, MAX_PROC_NAME_LEN);
	
	// 从旧的用户栈拷贝参数到新的用户栈
	td_initustack(td, TD_TEMPUSTACK);
	td_setustack(td, argc_count(td->td_pt, argv), argv);
	// 将旧的用户栈回收，新的用户栈生效
	td->td_trapframe->sp += TD_TEMPUSTACK_OFFSET;
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 stackva = TD_USTACK + i * PAGE_SIZE;
		u64 tmpva = TD_TEMPUSTACK + i * PAGE_SIZE;
		// 解除旧的用户栈映射
		panic_on(ptUnmap(td->td_pt, stackva));
		// 映射新的用户栈（先映射防止引用计数为零被回收）
		u64 pa = pteToPa(ptLookup(td->td_pt, tmpva));
		panic_on(ptMap(td->td_pt, stackva, pa, PTE_R | PTE_W | PTE_U));
		// 解除临时用户栈映射
		panic_on(ptUnmap(td->td_pt, tmpva));
	}

	// 回收先前的代码段
	for (u64 va = 0; va < td->td_brk; va += PAGE_SIZE) {
		if (ptLookup(td->td_pt, va) & PTE_V) {
			panic_on(ptUnmap(td->td_pt, va));
		}
	}
	td->td_brk = 0;

	// 加载可执行文件到内核
	void *bin;
	size_t size;
	fileid_t file = file_load(buf, &bin, &size);
	// 加载代码段
	td_initucode(td, bin, size);
	file_unload(file);
}