#ifndef _SYSCALL_FS_H
#define _SYSCALL_FS_H

// Mmap 的一些宏定义
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0X01000000
#define PROT_GROWSUP 0X02000000

#define MAP_FILE 0
#define MAP_SHARED 0x01   /* Share changes.  */
#define MAP_PRIVATE 0X02  /* Changes are private.  */
#define MAP_FAILED ((void *)-1)

// MMAP的标志位flags
// 取自/usr/include/bits/mman-linux.h
/* Other flags.  */
#define MAP_FIXED	0x10		/* Interpret addr exactly.  */
#define MAP_ANONYMOUS	0x20		/* Don't use a file.  */

#endif
