#ifndef _PARAM_H
#define _PARAM_H
#include <feature.h>

#ifndef NCPU
#define NCPU 8 // maximum number of CPUs
#endif	       // !NCPU

#ifdef FEATURE_LESS_MEMORY
#define NPROC 480		  // maximum number of processes
#define MAX_DIRENT 160000
#else
#define NPROC 10240		  // maximum number of processes
#define MAX_DIRENT 160000
#endif

#define MAXARG 256		  // max exec arguments
#define MAXARGLEN 256		  // max exec argument length
#define MAXPATH 128		  // maximum file path name
#define MAX_PROC_NAME_LEN (MAXPATH + 1)

#endif
