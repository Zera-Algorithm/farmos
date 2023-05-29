#include <fs/buf.h>
#include <fs/cluster.h>
#include <fs/fat32.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/error.h>

#define min(a,b) ((a) < (b) ? (a) : (b))

// 初始化
err_t fatInit(FileSystem *fs) {
	log(FAT_MODULE, "Fat32 FileSystem Init Start\n");
	// 读取 BPB
	Buffer *buf = fs->get(fs, 0);
	if (buf == NULL) {
		return -E_DEV_ERROR;
	}

	// 从 BPB 中读取信息
	FAT32BootParamBlock *bpb = (FAT32BootParamBlock *)(buf->data->data);

	if (bpb == NULL || strcmp((char *)bpb->BS_FilSysType, "FAT32")) {
		log(FAT_MODULE, "Not FAT32 File System\n");
		return -E_UNKNOWN_FS;
	}
	fs->superBlock.bpb.bytes_per_sec = bpb->BPB_BytsPerSec;
	fs->superBlock.bpb.sec_per_clus = bpb->BPB_SecPerClus;
	fs->superBlock.bpb.rsvd_sec_cnt = bpb->BPB_RsvdSecCnt;
	fs->superBlock.bpb.fat_cnt = bpb->BPB_NumFATs;
	fs->superBlock.bpb.hidd_sec = bpb->BPB_HiddSec;
	fs->superBlock.bpb.tot_sec = bpb->BPB_TotSec32;
	fs->superBlock.bpb.fat_sz = bpb->BPB_FATSz32;
	fs->superBlock.bpb.root_clus = bpb->BPB_RootClus;

	// 填写超级块
	fs->superBlock.first_data_sec = bpb->BPB_RsvdSecCnt + bpb->BPB_NumFATs * bpb->BPB_FATSz32;
	fs->superBlock.data_sec_cnt = bpb->BPB_TotSec32 - fs->superBlock.first_data_sec;
	fs->superBlock.data_clus_cnt = fs->superBlock.data_sec_cnt / bpb->BPB_SecPerClus;
	fs->superBlock.bytes_per_clus = bpb->BPB_SecPerClus * bpb->BPB_BytsPerSec;
	if (BUF_SIZE != fs->superBlock.bpb.bytes_per_sec) {
		log(FAT_MODULE, "BUF_SIZE != fs->superBlock.bpb.bytes_per_sec\n");
		return -E_DEV_ERROR;
	}

	// 释放缓冲区
	bufRelease(buf);
}

// 簇的扇区号计算

/**
 * @return 返回簇号 cluster 所在的第一个扇区号
 */
static u64 clusterSec(FileSystem *fs, u64 cluster) {
	const int cluster_first_sec = 2;
	return fs->superBlock.first_data_sec +
	       (cluster - cluster_first_sec) * fs->superBlock.bpb.sec_per_clus;
}

/**
 * @return 返回簇号 cluster 所在的 FAT 表的扇区号 
 */
static u64 clusterFatSec(FileSystem *fs, u64 cluster, u8 fatno) {
	const int fat32_entry_sz = 4;
	return fs->superBlock.bpb.rsvd_sec_cnt + fatno * fs->superBlock.bpb.fat_sz +
	       cluster * fat32_entry_sz / fs->superBlock.bpb.bytes_per_sec;
}

/**
 * @return 返回簇号 cluster 在 FAT 表中的偏移量
 */
static u64 clusterFatSecIndex(FileSystem *fs, u64 cluster) {
	const int fat32_entry_sz = 4;
	return ((cluster * fat32_entry_sz) % fs->superBlock.bpb.bytes_per_sec) / fat32_entry_sz;
}


void clusterRead(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser) {
	panic_on(offset + n < fs->superBlock.bytes_per_clus);

	// 计算簇号 cluster 所在的扇区号
	u64 secno = clusterSec(fs, cluster) + offset / fs->superBlock.bpb.bytes_per_sec;
	// 计算簇号 cluster 所在的扇区内的偏移量
	u64 secoff = offset % fs->superBlock.bpb.bytes_per_sec;

	// 判断读写长度是否超过簇的大小
	panic_on(n > fs->superBlock.bytes_per_clus - offset);

	// 读扇区
	for (u64 i = 0; i < n; secno++, secoff = 0) {
		Buffer *buf = fs->get(fs, secno);
		// 计算本次读写的长度
		size_t len = min(fs->superBlock.bpb.bytes_per_sec - secoff, n - i);
		if (isUser) {
			extern void copyOut(u64 uPtr, void *kPtr, int len);
			copyOut((u64)dst + i, buf->data->data, len);
		} else {
			memcpy(dst + i, buf->data->data, len);
		}
		bufRelease(buf);
		i += len;
	}
}

void clusterWrite(FileSystem *fs, u64 cluster, off_t offset, void *src, size_t n, bool isUser) {
	panic_on(offset + n < fs->superBlock.bytes_per_clus);

	// 计算簇号 cluster 所在的扇区号
	u64 secno = clusterSec(fs, cluster) + offset / fs->superBlock.bpb.bytes_per_sec;
	// 计算簇号 cluster 所在的扇区内的偏移量
	u64 secoff = offset % fs->superBlock.bpb.bytes_per_sec;

	// 判断读写长度是否超过簇的大小
	panic_on(n > fs->superBlock.bytes_per_clus - offset);

	// 写扇区
	for (u64 i = 0; i < n; secno++, secoff = 0) {
		Buffer *buf = fs->get(fs, secno);
		// 计算本次读写的长度
		size_t len = min(fs->superBlock.bpb.bytes_per_sec - secoff, n - i);
		if (isUser) {
			extern void copyIn(u64 uPtr, void *kPtr, int len);
			copyIn((u64)src + i, buf->data->data, len);
		} else {
			memcpy(buf->data->data, src + i, len);
		}
		bufWrite(buf);
		bufRelease(buf);
		i += len;
	}
}


static void fatWrite(FileSystem *fs, u64 cluster, u32 content) {
	panic_on(cluster < 2 || cluster > fs->superBlock.data_clus_cnt + 1);

	u64 fatSec = clusterFatSec(fs, cluster, 1);// TODO: SYNCRONIZE OTHER FAT
	Buffer *buf = fs->get(fs, fatSec);
	u32 *fat = (u32 *)buf->data->data;
	// 写入 FAT 表中的内容
	fat[clusterFatSecIndex(fs, cluster)] = content;
	bufWrite(buf);
	bufRelease(buf);
}

u32 fatRead(FileSystem *fs, u64 cluster) {
	if (cluster < 2 || cluster > fs->superBlock.data_clus_cnt + 1) {
		return 0;
	}
	u64 fatSec = clusterFatSec(fs, cluster, 1);// TODO: SYNCRONIZE OTHER FAT
	Buffer *buf = fs->get(fs, fatSec);
	u32 *fat = (u32 *)buf->data->data;
	// 读取 FAT 表中的内容
	u32 content = fat[clusterFatSecIndex(fs, cluster)];
	bufRelease(buf);
	return content;
}


#define FAT32_EOF 0xffffffff

u64 clusterAlloc(FileSystem *fs, u64 prev) {
	for (u64 cluster = prev == 0 ? 2 : prev + 1; cluster < fs->superBlock.data_clus_cnt + 2; cluster++) {
		if (fatRead(fs, cluster) == 0) {
			if (prev != 0) {
				fatWrite(fs, prev, cluster);
			}
			fatWrite(fs, cluster, FAT32_EOF);
			return cluster;
		}
	}
	return 0;
}

void clusterFree(FileSystem *fs, u64 cluster, u64 prev) {
	if (prev == 0) {
		fatWrite(fs, cluster, 0);
	} else {
		fatWrite(fs, prev, FAT32_EOF);
		fatWrite(fs, cluster, 0);
	}
}



