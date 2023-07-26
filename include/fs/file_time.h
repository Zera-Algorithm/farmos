#ifndef _FILE_TIME_H
#define _FILE_TIME_H

typedef struct file_time {
	long st_atime_sec;
	long st_atime_nsec;
	long st_mtime_sec;
	long st_mtime_nsec;
	long st_ctime_sec;
	long st_ctime_nsec;
} file_time_t;

#define ACCESS_TIME 1
#define MODIFY_TIME 2
#define STATUS_CHANGE_TIME 4

struct timespec;
typedef struct Dirent Dirent;
struct kstat;

void file_get_timestamp(Dirent *file, struct kstat *kstat);
void file_update_timestamp(Dirent *file, int type);
void file_set_timestamp(Dirent *file, int type, struct timespec *ts);

#define UTIME_NOW ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)

#endif
