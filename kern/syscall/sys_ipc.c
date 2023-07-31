#include <ipc/shm.h>

int sys_shmget(u64 key, u64 size, int shmflg) {
	return shmget(key, size, shmflg);
}

void *sys_shmat(int shmid, u64 shmaddr, int shmflg) {
	return shmat(shmid, shmaddr, shmflg);
}

int sys_shmctl(int shmid, int cmd, u64 arg_buf) {
	return shmctl(shmid, cmd, arg_buf);
}

