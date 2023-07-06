#include <fs/buf.h>
#include <fs/cluster.h>
#include <fs/dirent.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/wchar.h>

#define MAX_CLUS_SIZE (128 * BUF_SIZE)

/**
 * 定义FAT32的一些公共基础函数
 * finished.
 */

/**
 * @brief 计数文件的簇数
 */
int countClusters(struct Dirent *file) {
	log(LEVEL_GLOBAL, "count Cluster begin!\n");

	int clus = file->first_clus;
	int i = 0;
	if (clus == 0) {
		log(LEVEL_GLOBAL, "cluster is 0!\n");
		return 0;
	}
	// 如果文件不包含任何块，则直接返回0即可。
	else {
		while (FAT32_NOT_END_CLUSTER(clus)) {
			log(LEVEL_GLOBAL, "clus is %d\n", clus);
			clus = fatRead(file->file_system, clus);
			i += 1;
		}
		log(LEVEL_GLOBAL, "count Cluster end!\n");
		return i;
	}
}

int get_entry_count_by_name(char *name) {
	int len = (strlen(name) + 1);

	if (len > 11) {
		int cnt = 1; // 包括短文件名项
		if (len % BYTES_LONGENT == 0) {
			cnt += len / BYTES_LONGENT;
		} else {
			cnt += len / BYTES_LONGENT + 1;
		}
		return cnt;
	} else {
		// 自己一个，自己的长目录项一个
		return 2;
	}
}

// FAT32 官方文档中的函数

//-----------------------------------------------------------------------------
// ChkSum()
// Returns an unsigned byte checksum computed on an unsigned byte
// array. The array must be 11 bytes long and is assumed to contain
// a name stored in the format of a MS-DOS directory entry.
// Passed: pFcbName Pointer to an unsigned byte array assumed to be
// 11 bytes long.
// Returns: Sum An 8-bit unsigned checksum of the array pointed
// to by pFcbName.
//------------------------------------------------------------------------------
unsigned char checkSum(unsigned char *pFcbName) {
	short FcbNameLen;
	unsigned char Sum;
	Sum = 0;
	for (FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--) {
		// NOTE: The operation is an unsigned char rotate right
		Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
	}
	return (Sum);
}
