#ifndef _UTSNAME_H
#define _UTSNAME_H

#ifndef _UTSNAME_SYSNAME_LENGTH
#define _UTSNAME_SYSNAME_LENGTH 65
#endif

#ifndef _UTSNAME_NODENAME_LENGTH
#define _UTSNAME_NODENAME_LENGTH 65
#endif

#ifndef _UTSNAME_RELEASE_LENGTH
#define _UTSNAME_RELEASE_LENGTH 65
#endif

#ifndef _UTSNAME_VERSION_LENGTH
#define _UTSNAME_VERSION_LENGTH 65
#endif

#ifndef _UTSNAME_MACHINE_LENGTH
#define _UTSNAME_MACHINE_LENGTH 65
#endif

typedef struct utsname {
	char sysname[_UTSNAME_SYSNAME_LENGTH];	 // 当前操作系统名
	char nodename[_UTSNAME_NODENAME_LENGTH]; // 网络上的名称
	char release[_UTSNAME_RELEASE_LENGTH];	 // 当前发布级别
	char version[_UTSNAME_VERSION_LENGTH];	 // 当前发布版本
	char machine[_UTSNAME_MACHINE_LENGTH];	 // 当前硬件体系类型
} utsname_t;

#endif // _UTSNAME_H
