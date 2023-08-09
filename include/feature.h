/**
 * 本文件定义不同的架构下的特性
 */

#ifdef QEMU
#define FEATURE_LESS_MEMORY
#else
#define FEATURE_MORE_MEMORY
#endif

#ifdef VIRT
#define FEATURE_TIMER_FREQ 10000000ul
#define FEATURE_DISK_VIRTIO
#endif

#ifdef SIFIVE
#define FEATURE_DISK_SD
#define MMU_AD_ENABLE
#define FEATURE_TIMER_FREQ 1000000ul
#endif
