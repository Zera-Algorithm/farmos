#ifndef _ELF_H
#define _ELF_H
// Format of an ELF executable file
#include <types.h>

/**
 * @brief 将 `src` 处长度为 `len` 的数据映射到 `va + offset` 上，并设置权限为 `perm`
 */
typedef int (*ElfMapper)(void *data, u64 va, size_t offset, u64 perm, const void *src, size_t len);

#define ELF_MAGIC 0x464C457FU // "\x7FELF" in little endian

// 以下是辅助数组的类型取值表
#define AT_NULL 0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_NOTELF 10
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_CLKTCK 17

#define AT_PLATFORM 15
#define AT_HWCAP 16

#define AT_FPUCW 18

#define AT_DCACHEBSIZE 19
#define AT_ICACHEBSIZE 20
#define AT_UCACHEBSIZE 21

#define AT_IGNOREPPC 22

#define AT_SECURE 23

#define AT_BASE_PLATFORM 24

#define AT_RANDOM 25

#define AT_HWCAP2 26

#define AT_EXECFN 31

#define AT_SYSINFO 32
#define AT_SYSINFO_EHDR 33

#define AT_L1I_CACHESHAPE 34
#define AT_L1D_CACHESHAPE 35
#define AT_L2_CACHESHAPE 36
#define AT_L3_CACHESHAPE 37
#define AT_L1I_CACHESIZE 40
#define AT_L1I_CACHEGEOMETRY 41
#define AT_L1D_CACHESIZE 42
#define AT_L1D_CACHEGEOMETRY 43
#define AT_L2_CACHESIZE 44
#define AT_L2_CACHEGEOMETRY 45
#define AT_L3_CACHESIZE 46
#define AT_L3_CACHEGEOMETRY 47
#define AT_MINSIGSTKSZ 51

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
int loadDataMapper(void *data, u64 va, size_t offset, u64 perm, const void *src, size_t len);

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
#define ELF_PROG_LOAD 1 // deprecated
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7               /* Thread local storage segment */
#define PT_LOOS    0x60000000      /* OS-specific */
#define PT_HIOS    0x6fffffff      /* OS-specific */
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7fffffff
#define PT_GNU_EH_FRAME		0x6474e550


// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4
#endif
