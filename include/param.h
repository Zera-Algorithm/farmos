#ifndef _PARAM_H
#define _PARAM_H

#ifndef NCPU
#error NCPU not defined
#endif	       // !NCPU


#define NOFILE 16		  // open files per process
#define NFILE 100		  // open files per system
#define NINODE 50		  // maximum number of active i-nodes
#define NDEV 10			  // maximum major device number
#define ROOTDEV 1		  // device number of file system root disk
#define MAXARG 256		  // max exec arguments
#define MAXARGLEN 256		  // max exec argument length
#define MAXOPBLOCKS 10		  // max # of blocks any FS op writes
#define LOGSIZE (MAXOPBLOCKS * 3) // max data blocks in on-disk log
#define NBUF (MAXOPBLOCKS * 3)	  // size of disk block cache
#define FSSIZE 2000		  // size of file system in blocks
#define MAXPATH 128		  // maximum file path name
#define MAX_PROC_NAME_LEN (MAXPATH + 1)

// FarmOS 参数
#define NPROC 480		    // FarmOS 支持的最大进程数
#define NTHREAD NPROC       // FarmOS 支持的最大线程数
#define NPROCSIGNALS 128     // FarmOS 支持的最大信号数
#define NSIGEVENTS 512      // FarmOS 支持的最大信号事件数

#endif
