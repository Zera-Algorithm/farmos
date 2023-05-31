#ifndef _SYSCALL_H
#define _SYSCALL_H

struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

struct tms {
	uint64 tms_utime;
	uint64 tms_stime;
	uint64 tms_cutime;
	uint64 tms_cstime;
};

struct timespec {
	uint64 second;
	long usec;
};

struct DirentUser {
	// TODO
};

#endif

