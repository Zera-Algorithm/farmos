#include <dev/virtio.h>
#include <fs/buf.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/string.h>
#include <mm/memlayout.h>
#include <param.h>
#include <riscv.h>
#include <types.h>

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))
#define availOffset (sizeof(struct virtq_desc) * NUM)
void *virtioDriverBuffer;

static struct disk {

	struct virtq_desc *desc;
	struct virtq_avail *avail;
	struct virtq_used *used;
	char free[NUM];
	uint16 used_idx;

	struct {
		// struct buf *b;
		Buffer *b;
		char status;
	} info[NUM];

	struct virtio_blk_req ops[NUM];

} disk;

void virtio_disk_init(void) {

	uint32 status = 0;

	// 检查设备的魔术值、版本、设备ID和厂商ID，确保找到了virtio磁盘设备。如果条件不满足，会触发panic
	if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 || *R(VIRTIO_MMIO_VERSION) != 1 ||
	    *R(VIRTIO_MMIO_DEVICE_ID) != 2 || *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
		panic("could not find virtio disk");
	}

	// 将状态寄存器置零，用于重置设备
	*R(VIRTIO_MMIO_STATUS) = status;

	// 设置ACKNOWLEDGE和DRIVER状态位，表示驱动程序已经意识到设备的存在，并准备开始驱动
	status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
	*R(VIRTIO_MMIO_STATUS) = status;

	status |= VIRTIO_CONFIG_S_DRIVER;
	*R(VIRTIO_MMIO_STATUS) = status;

	// 协商设备和驱动程序所支持的特性，
	// 将驱动程序支持的特性写入VIRTIO_MMIO_DRIVER_FEATURES寄存器。
	uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
	features &= ~(1 << VIRTIO_BLK_F_RO);
	features &= ~(1 << VIRTIO_BLK_F_SCSI);
	features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
	features &= ~(1 << VIRTIO_BLK_F_MQ);
	features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
	features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
	features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
	*R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

	// 这些是verison2的操作,删去
	//   // 设置FEATURES_OK状态位，告诉设备特性协商已完成。
	//   status |= VIRTIO_CONFIG_S_FEATURES_OK;
	//   *R(VIRTIO_MMIO_STATUS) = status;

	//   // 重新读取状态寄存器，确保FEATURES_OK状态位已设置。如果没有设置，触发panic。
	//   status = *R(VIRTIO_MMIO_STATUS);
	//   if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
	//     panic("virtio disk FEATURES_OK unset");

	// 选择队列0进行初始化
	*R(VIRTIO_MMIO_QUEUE_SEL) = 0;

	// 确保队列0未被使用。如果队列0已被使用，触发panic
	if (*R(VIRTIO_MMIO_QUEUE_PFN) != 0)
		panic("virtio disk should not be ready");

	// 检查最大队列大小。如果最大队列大小为0，触发panic。如果最大队列大小小于NUM（预定义的队列大小），触发panic
	uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (max == 0)
		panic("virtio disk has no queue 0");
	if (max < NUM)
		panic("virtio disk max queue too short");

	// 输入page size
	*R(VIRTIO_MMIO_PAGE_SIZE) = PAGE_SIZE;

	// 分配内存用于存储队列相关的描述符（desc）、可用环（avail）和已使用环（used）。如果分配失败，触发panic。然后使用memset将分配的内存清零
	disk.desc = (void *)virtioDriverBuffer;
	disk.avail = (void *)((uint64)disk.desc + availOffset);
	disk.used = (void *)((uint64)disk.desc + PAGE_SIZE);

	// 这里后续考虑用一次alloc()

	if (!disk.desc || !disk.used)
		panic("virtio disk kalloc");
	memset(disk.desc, 0, PAGE_SIZE);
	memset(disk.used, 0, PAGE_SIZE);

	// 设置队列的大小
	*R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

	// 设置Queue Align
	*R(VIRTIO_MMIO_QUEUE_ALIGN) = PAGE_SIZE;

	// 设置QUEUE PFN
	*R(VIRTIO_MMIO_QUEUE_PFN) = (uint64)disk.desc >> 12;

	/* 这是version1的内容
	// 将队列相关数据结构的物理地址写入相应的寄存器
	  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
	  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
	  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
	  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
	  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
	  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;
	*/

	/* 这是verison2的内容
	 *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;
	 */
	// 设置队列为就绪状态.

	// 将所有的NUM个描述符设置为未使用状态
	for (int i = 0; i < NUM; i++)
		disk.free[i] = 1;

	// 设置DRIVER_OK状态位，告诉设备驱动程序已准备完毕
	status |= VIRTIO_CONFIG_S_DRIVER_OK;
	*R(VIRTIO_MMIO_STATUS) = status;
}

static int alloc_desc() {
	for (int i = 0; i < NUM; i++) {
		if (disk.free[i]) {
			disk.free[i] = 0;
			return i;
		}
	}
	return -1;
}

static void free_desc(int i) {
	if (i >= NUM)
		panic("free number is above maximu");
	if (disk.free[i])
		panic("desc is already freed");
	disk.desc[i].addr = 0;
	disk.desc[i].len = 0;
	disk.desc[i].flags = 0;
	disk.desc[i].next = 0;
	disk.free[i] = 1;
}

static void free_chain(int i) {
	while (1) {
		int flag = disk.desc[i].flags;
		int nxt = disk.desc[i].next;
		free_desc(i);
		if (flag & VRING_DESC_F_NEXT)
			i = nxt;
		else
			break;
	}
}

static int alloc3_desc(int *idx) {
	for (int i = 0; i < 3; i++) {
		idx[i] = alloc_desc();
		if (idx[i] < 0) {
			for (int j = 0; j < i; j++)
				free_desc(idx[j]);
			return -1;
		}
	}
	return 0;
}

/**
 * @brief virtio读写接口，此函数使用忙等策略，若磁盘未准备好，则驱动会一直阻塞等待到磁盘就绪。
 * 		  调用示例可以参见virtioTest函数
 * @param b
 * 要读或写的缓冲区描述符（定义在fs/buf.h）。在调用之前，b->blockno需要设置为要读或写的扇区号，
 * 		  每个扇区512字节。若为读取，调用此函数后，b->data的内容就是对应扇区的数据；若为写入，则b->data
 * 		  需要提前写入要写入的数据
 * @param write 是否读。设为0表示读取，1表示写入
 */
void virtio_disk_rw(Buffer *b, int write) {
	uint64 sector = b->blockno * (BUF_SIZE / 512);

	// the spec's Section 5.2 says that legacy block operations use
	// three descriptors: one for type/reserved/sector, one for the
	// data, one for a 1-byte status result.

	// allocate the three descriptors.
	int idx[3];
	while (1) {
		if (alloc3_desc(idx) == 0) {
			break;
		}
		panic("there is no free des");
	}

	// format the three descriptors.
	// qemu's virtio-blk.c reads them.

	struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

	if (write)
		buf0->type = VIRTIO_BLK_T_OUT; // write the disk
	else
		buf0->type = VIRTIO_BLK_T_IN; // read the disk
	buf0->reserved = 0;
	buf0->sector = sector;

	disk.desc[idx[0]].addr = (uint64)buf0;
	disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
	disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
	disk.desc[idx[0]].next = idx[1];

	disk.desc[idx[1]].addr = (uint64)b->data;
	disk.desc[idx[1]].len = BUF_SIZE;
	if (write)
		disk.desc[idx[1]].flags = 0; // device reads b->data
	else
		disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
	disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
	disk.desc[idx[1]].next = idx[2];

	disk.info[idx[0]].status = 0xff; // device writes 0 on success
	disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
	disk.desc[idx[2]].len = 1;
	disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
	disk.desc[idx[2]].next = 0;

	// record struct buf for virtio_disk_intr().
	b->disk = 1;
	disk.info[idx[0]].b = b;

	// tell the device the first index in our chain of descriptors.
	disk.avail->ring[disk.avail->idx % NUM] = idx[0];

	__sync_synchronize();

	// tell the device another avail ring entry is available.
	disk.avail->idx += 1; // not % NUM ...

	__sync_synchronize();

	*R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

	log(LEVEL_MODULE, "enter virtio wait!\n");
	while (b->disk == 1) {
		__sync_synchronize();
		continue;
	}
	log(LEVEL_MODULE, "exit virtio wait!\n");

	disk.info[idx[0]].b = 0;
	free_chain(idx[0]);
}

/**
 * @brief virtio驱动的中断处理函数
 */
void virtio_disk_intr() {
	*R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

	__sync_synchronize();

	// the device increments disk.used->idx when it
	// adds an entry to the used ring.

	while (disk.used_idx != disk.used->idx) {
		__sync_synchronize();
		int id = disk.used->ring[disk.used_idx % NUM].id;

		if (disk.info[id].status != 0)
			panic("virtio_disk_intr status");

		Buffer *b = disk.info[id].b;
		__sync_synchronize();
		assert(b->disk == 1);
		b->disk = 0; // disk is done with buf
		__sync_synchronize();

		disk.used_idx += 1;
	}

	log(LEVEL_MODULE, "finish virtio intr\n");
}

/**
 * @brief 进行virtio驱动的读写测试
 */
void virtioTest() {
	log(LEVEL_GLOBAL, "begin virtio test!\n");
	Buffer bufR, bufW;
	BufferData bufDataR, bufDataW;
	bufR.data = &bufDataR;
	bufW.data = &bufDataW;

	// 测试写入0号扇区（块）
	bufW.blockno = 0;
	for (int i = 0; i < BUF_SIZE; i++) {
		bufW.data->data[i] = '0' + i % 10;
	}
	bufW.data->data[BUF_SIZE - 1] = 0;
	virtio_disk_rw(&bufW, 1); // write

	// 测试读出0号扇区
	bufR.blockno = 0;
	virtio_disk_rw(&bufR, 0); // read
	assert(strncmp((const char *)bufR.data, (const char *)bufW.data, BUF_SIZE) == 0);

	// 测试写入1号扇区
	bufW.blockno = 1;
	for (int i = 0; i < BUF_SIZE; i++) {
		bufW.data->data[i] = '2' + i % 6;
	}
	virtio_disk_rw(&bufW, 1); // write

	// 测试读出1号扇区
	bufR.blockno = 1;
	virtio_disk_rw(&bufR, 0); // read
	assert(strncmp((const char *)bufR.data, (const char *)bufW.data, BUF_SIZE) == 0);

	log(LEVEL_MODULE, "buf:\n%s", bufR.data);

	log(LEVEL_GLOBAL, "virtio driver test passed!\n");
}
