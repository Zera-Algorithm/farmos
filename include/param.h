#ifndef _PARAM_H
#define _PARAM_H

#ifndef NCPU
#define NCPU 8 // maximum number of CPUs
#endif	       // !NCPU

#define NPROC 1024		  // maximum number of processes
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

#define QEMU

#endif
