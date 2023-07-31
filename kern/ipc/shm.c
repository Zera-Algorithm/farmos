#include <ipc/shm.h>
#include <mm/kmalloc.h>
#include <lock/mutex.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <mm/mmu.h>
#include <sys/errno.h>
#include <proc/interface.h>
#include <lib/log.h>

LIST_HEAD(, shm) shm_list;
mutex_t shm_list_lock;
static int tot_shmid = 0;
extern Pte *kernPd;

void shm_init() {
	mtx_init(&shm_list_lock, "shm_list", false, MTX_SPIN);
	LIST_INIT(&shm_list);
}

static u64 alloc_kmem(u64 size) {
	static u64 cur_addr = KERNEL_SHM;
	size = PGROUNDUP(size);
	for (u64 addr = cur_addr; addr < cur_addr + size; addr += PAGE_SIZE) {
		u64 pa = vmAlloc();
		panic_on(ptMap(kernPd, addr, pa, PTE_R | PTE_W));
	}
	cur_addr += size;
	return cur_addr - size;
}

static void free_kmem(u64 addr, u64 size) {
	size = PGROUNDUP(size);
	for (u64 va = addr; va < addr + size; va += PAGE_SIZE) {
		panic_on(ptUnmap(kernPd, va));
	}
}

int shmget(u64 key, u64 size, int shmflg) {
	mtx_lock(&shm_list_lock);
	struct shm *shm = NULL;

	if (shmflg == 0 && key != IPC_PRIVATE) {
		// get one
		LIST_FOREACH(shm, &shm_list, shm_link) {
			if (shm->key == key) {
				mtx_unlock(&shm_list_lock);
				return shm->shmid;
			}
		}
		mtx_unlock(&shm_list_lock);
		return -ENOENT;
	} else if (shmflg & IPC_CREAT) {
		// create one
		shm_t *shm = kmalloc(sizeof(shm_t));
		shm->key = key;
		shm->size = size;
		shm->shmid = ++tot_shmid;
		shm->shm_flags = shmflg;
		shm->kaddr = alloc_kmem(size);
		LIST_INSERT_HEAD(&shm_list, shm, shm_link);
		mtx_unlock(&shm_list_lock);

		return shm->shmid;
	} else {
		warn("shmget: invalid key %ld and shmflg %lx\n", key, shmflg);
		mtx_unlock(&shm_list_lock);
		return -EINVAL;
	}
	return 0;
}

void *shmat(int shmid, u64 shmaddr, int shmflg) {
	mtx_lock(&shm_list_lock);
	int is_find = 0;
	shm_t *shm;
	LIST_FOREACH(shm, &shm_list, shm_link) {
		if (shm->shmid == shmid) {
			is_find = 1;
			break;
		}
	}
	if (!is_find) {
		mtx_unlock(&shm_list_lock);
		return (void *)(-EINVAL);
	}

	if (shmaddr == 0) {
		shmaddr = cur_proc_fs_struct()->mmap_addr;
		cur_proc_fs_struct()->mmap_addr += shm->size;
	}

	Pte *pt = cpu_this()->cpu_running->td_proc->p_pt;
	for (u64 va = shmaddr; va < shmaddr + shm->size; va += PAGE_SIZE) {
		u64 pa = pteToPa(ptLookup(kernPd, shm->kaddr + va - shmaddr));
		panic_on(ptMap(pt, va, pa, PTE_R | PTE_W | PTE_U | PTE_SHARED));
	}
	mtx_unlock(&shm_list_lock);
	return (void *)shmaddr;
}

int shmctl(int shmid, int cmd, u64 arg_buf) {
	mtx_lock(&shm_list_lock);
	int is_find = 0;
	shm_t *shm;
	LIST_FOREACH(shm, &shm_list, shm_link) {
		if (shm->shmid == shmid) {
			is_find = 1;
			break;
		}
	}
	if (!is_find) {
		mtx_unlock(&shm_list_lock);
		return -EINVAL;
	}

	if (cmd == IPC_RMID) {
		LIST_REMOVE(shm, shm_link);
		free_kmem(shm->kaddr, shm->size);
		mtx_unlock(&shm_list_lock);
		return 0;
	} else {
		warn("shmctl: invalid cmd %d\n", cmd);
		mtx_unlock(&shm_list_lock);
		return -EINVAL;
	}
}
