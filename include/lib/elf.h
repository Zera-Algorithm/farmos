#ifndef _ELF_H
#define _ELF_H
// Format of an ELF executable file
#include <types.h>

/**
 * @brief 将 `src` 处长度为 `len` 的数据映射到 `va + offset` 上，并设置权限为 `perm`
 */
typedef int (*ElfMapper)(void *data, u64 va, size_t offset, u64 perm, const void *src, size_t len);

#define ELF_MAGIC 0x464C457FU // "\x7FELF" in little endian

// File header
typedef struct ElfHeader {
	u32 e_magic; // must equal ELF_MAGIC
	u8 e_elf[12];
	u16 e_type;	 // 目标文件类型
	u16 e_machine;	 // 架构
	u32 e_version;	 // 版本
	u64 e_entry;	 // 入口点
	u64 e_phoff;	 // 程序头表偏移
	u64 e_shoff;	 // 节头表偏移
	u32 e_flags;	 // 处理器相关的标志
	u16 e_ehsize;	 // ELF头的大小
	u16 e_phentsize; // 程序头表表项的大小
	u16 e_phnum;	 // 程序头的个数
	u16 e_shentsize; // 节头表表项大小
	u16 e_shnum;	 // 节头表表项个数
	u16 e_shstrndx;	 // 节头名字符串的缓冲区偏移
} ElfHeader;

// Program section header
typedef struct ProgramHeader {
	u32 p_type;  // 程序段的类型
	u32 p_flags; //
	u64 p_off;   // 段的文件内偏移
	u64 p_vaddr;
	u64 p_paddr;
	u64 p_filesz;
	u64 p_memsz;
	u64 p_align;
} ProgramHeader;

const ElfHeader *getElfFrom(const void *binary, size_t size);
int loadElfSegment(ProgramHeader *ph, const void *binary, ElfMapper mapPage, void *data);
int loadDataMapper(void *data, u64 va, size_t offset, u64 perm, const void *src,
			  size_t len);

/**
 * @brief 遍历ELF头的每一个段头的ph_off
 * @param ph_off 循环变量，此次遍历得到的段头偏移。需提前定义，类型为size_t
 * @param elfHeader elf头，ElfHeader * 类型
 */
#define ELF_FOREACH_PHDR_OFF(phOff, elfHeader)                                                     \
	(phOff) = (elfHeader)->e_phoff;                                                            \
	for (int _ph_idx = 0; _ph_idx < (elfHeader)->e_phnum;                                      \
	     ++_ph_idx, (phOff) += (elfHeader)->e_phentsize)

// Values for Proghdr type
#define ELF_PROG_LOAD 1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4
#endif
