#ifndef _PARAM_H
#define _PARAM_H
#include <feature.h>

#ifndef NCPU
#error NCPU not defined
#endif	       // !NCPU

#ifdef FEATURE_LESS_MEMORY
#define NPROC 480		  // FarmOS 支持的最大进程数
#define MAX_DIRENT 160000
#else
#define NPROC 10240		  // maximum number of processes
#define MAX_DIRENT 160000
#endif

#define MAXARG 256		  // max exec arguments
#define MAXARGLEN 256		  // max exec argument length
#define MAXPATH 128		  // maximum file path name
#define MAX_PROC_NAME_LEN (MAXPATH + 1)

// FarmOS 参数
#define NTHREAD NPROC       // FarmOS 支持的最大线程数
#define NPROCSIGNALS 128     // FarmOS 支持的最大信号数
#define NSIGEVENTS 512      // FarmOS 支持的最大信号事件数

#endif
