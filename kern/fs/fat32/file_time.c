// 管理文件的时间戳

#include <dev/timer.h>
#include <fs/fat32.h>
#include <fs/file_time.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lock/mutex.h>
#include <sys/time.h>

extern mutex_t mtx_file;

/**
 * @brief 从file中获取到文件的时间戳，存储到kstat结构体中
 */
void file_get_timestamp(Dirent *file, struct kstat *kstat) {
	mtx_lock_sleep(&mtx_file);

	kstat->st_atime_sec = file->time.st_atime_sec;
	kstat->st_atime_nsec = file->time.st_atime_nsec;
	kstat->st_mtime_sec = file->time.st_mtime_sec;
	kstat->st_mtime_nsec = file->time.st_mtime_nsec;
	kstat->st_ctime_sec = file->time.st_ctime_sec;
	kstat->st_ctime_nsec = file->time.st_ctime_nsec;

	mtx_unlock_sleep(&mtx_file);
}

/**
 * @brief 更新文件的时间戳为当前时间，type表示要更新的类型
 */
void file_update_timestamp(Dirent *file, int type) {
	timespec_t ts = time_rtc_ts();
	u64 tv_sec = ts.tv_sec;
	u64 tv_nsec = ts.tv_nsec;

	mtx_lock_sleep(&mtx_file);

	if (type & ACCESS_TIME) {
		file->time.st_atime_sec = tv_sec;
		file->time.st_atime_nsec = tv_nsec;
	}

	if (type & MODIFY_TIME) {
		file->time.st_mtime_sec = tv_sec;
		file->time.st_mtime_nsec = tv_nsec;
	}

	if (type & STATUS_CHANGE_TIME) {
		file->time.st_ctime_sec = tv_sec;
		file->time.st_ctime_nsec = tv_nsec;
	}

	mtx_unlock_sleep(&mtx_file);
}

// 主要由utimensat系统调用使用
void file_set_timestamp(Dirent *file, int type, struct timespec *ts) {
	if (ts->tv_nsec == UTIME_NOW) {
		file_update_timestamp(file, type);
		return;
	} else if (ts->tv_nsec == UTIME_OMIT) {
		return;
	}

	mtx_lock_sleep(&mtx_file);

	if (type & ACCESS_TIME) {
		file->time.st_atime_sec = ts->tv_sec;
		file->time.st_atime_nsec = ts->tv_nsec;
	}

	if (type & MODIFY_TIME) {
		file->time.st_mtime_sec = ts->tv_sec;
		file->time.st_mtime_nsec = ts->tv_nsec;
	}

	if (type & STATUS_CHANGE_TIME) {
		file->time.st_ctime_sec = ts->tv_sec;
		file->time.st_ctime_nsec = ts->tv_nsec;
	}

	mtx_unlock_sleep(&mtx_file);
}
