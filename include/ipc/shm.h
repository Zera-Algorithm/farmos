#ifndef _SHM_H
#define _SHM_H
#include <types.h>
#include <lib/queue.h>

typedef struct shm {
	u64 key;
	u64 shmid;
	size_t size;
	int shm_flags;
	u64 kaddr;
	LIST_ENTRY(shm) shm_link;
} shm_t;

#define IPC_PRIVATE 0
#define IPC_CREAT	01000

// for shmctl cmd
#define IPC_RMID	0

void shm_init();
int shmget(u64 key, u64 size, int shmflg);
void *shmat(int shmid, u64 shmaddr, int shmflg);
int shmctl(int shmid, int cmd, u64 arg_buf);
#endif
