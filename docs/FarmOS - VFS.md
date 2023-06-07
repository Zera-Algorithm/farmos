# VFS

FarmOS实现了一个类似Linux的多层文件系统，实现了丰富的抽象。FarmOS的VFS分为驱动层、缓冲区层、簇层、文件系统层、文件描述符层共5个层次。

## VFS 驱动层

### 概述

VFS 驱动层是访问磁盘的最底层接口，负责按块读写磁盘。本层中，FarmOS 实现了 VirtIO Legacy 驱动。

VirtIO 驱动的逻辑是首先向磁盘发出读或写的请求，请求中记录要读或写的缓冲区的内存地址，然后设置等待标志位，等待磁盘完成请求的操作后发送中断。在中断处理程序中，VirtIO 驱动会将等待的标志位取消，告知前台等待的程序磁盘交互已完成。这样，就完成了一次磁盘的读或写操作。

### VirtIO 驱动接口

VirtIO 驱动为上层提供了2个接口：

* `void virtio_disk_init(void)`: 初始化磁盘，检查磁盘的型号是否与驱动一致。如果一致，就继续之后的初始化工作；如不一致，则直接报错
	> 具体而言，这里的版本检查主要是检查VirtIO设备是否是磁盘设备，且是否是Legacy的VirtIO设备

* `void virtio_disk_rw(Buffer *b, int write)`: 进程磁盘读写操作，依赖于 `write` 参数，将Buffer中的数据写入磁盘，或者从磁盘读取数据写入Buffer


## VFS 缓冲区层

### 概述

缓冲区层为磁盘 IO 提供了缓存，建立在磁盘驱动层之上，为上层屏蔽了硬件操作，提供了统一的磁盘访问接口，目前采用 Write-Through 策略，写操作时直接同步回磁盘。

缓冲区使用缓冲区链表维护，为提高并发性能，使用缓冲区时按照磁盘块序号对缓冲区进行分组，每个缓冲区组有一个缓冲区组锁，缓冲区组锁保护缓冲区组的缓冲区链表。

缓冲区使用 LRU 算法进行缓冲区替换，每次一个缓冲区使用结束时将其更新到链表头部。当没有空闲的缓冲区控制块时，释放当前最接近链表尾部且不在使用的缓冲区控制块。

### 缓冲区结构

每个缓存块都有一个缓冲区控制块，用于描述缓存块的属性。缓冲区控制块的定义如下：

```c
typedef struct BufferData {
	u8 data[BUF_SIZE];
} BufferData;

typedef struct Buffer {
	// 缓冲区控制块属性
	u64 blockno;
	i32 dev;
	bool valid;
	u16 disk;
	u16 refcnt;
	BufferData *data;
	struct sleeplock lock;
	TAILQ_ENTRY(Buffer) link;
} Buffer;
```

若干个缓冲区被组成一个缓冲区组，缓冲区组的定义如下：

```c

typedef struct BufferGroup {
	BufList list; // 缓冲区双向链表（越靠前使用越频繁）
	Buffer buf[BGROUP_BUF_NUM];
	struct spinlock lock;
} BufferGroup;

```

### 缓冲区接口

缓冲区为上层提供了三个接口：

- `bufRead`：从磁盘读取一个缓冲区，并对缓冲区数据加锁
- `bufWrite`：对于给定的当前进程持有锁的缓冲区，将缓冲区写入磁盘
- `bufRelease`：对于给定的当前进程持有锁的缓冲区，释放缓冲区

## VFS 簇层

### 概述

FAT32 文件系统簇层的主要功能是管理簇，并向 FAT32 文件系统的上层屏蔽对缓冲区的操作，提供统一的簇读写接口。

上层文件层仅通过簇层接口访问文件系统中的簇，而不直接访问缓冲区。

### 簇层接口

簇层为文件层提供了簇的读写、簇的分配与释放、FAT 表项的读取等接口。

簇层接口函数声明如下：

```c
void clusterRead(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser);
void clusterWrite(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser);

u64 clusterAlloc(FileSystem *fs, u64 prevCluster) __attribute__((warn_unused_result));
void clusterFree(FileSystem *fs, u64 cluster, u64 prevCluster);

u32 fatRead(FileSystem *fs, u64 cluster);
i64 fileBlockNo(FileSystem *fs, u64 firstclus, u64 fblockno);
```

接口函数意义如下：
- `clusterRead`：在给定的文件系统中读取对应簇的数据
- `clusterWrite`：在给定的文件系统中写入对应簇的数据
- `clusterAlloc`：在给定的文件系统中分配一个空闲簇
- `clusterFree`：在给定的文件系统中释放一个簇
- `fatRead`：在给定的文件系统中读取对应簇的 FAT 表项
- `fileBlockNo`：在给定的文件系统中计算文件中给定文件块号对应的簇号，一般用于 `mount`。

## VFS 文件系统层

FAT32 文件系统层的主要功能有两个：

* 以文件系统 `FileSystem` 结构体为单位，管理FarmOS里的各个文件系统。FileSystem为所有文件系统定义了共同的唯一的接口 `struct Buffer *(*get)(struct FileSystem *fs, u64 blockNum);`，只要实现该接口，无论文件系统的载体是具体设备，还是文件，甚至是一块内存，都能挂载为文件系统。

* 以目录项 `Dirent` 为单位，实现 **内核级** 的文件打开、新建、读写、链接、新建文件夹、获取文件状态、获取目录项等操作。`Dirent` 是 FAT32 文件系统表示文件和目录的通用方式，原始的 `Fat32Dirent` 所包含的信息量较少，仅包含文件名、文件属性、首簇等信息，且需要多个目录项组合才能实现长文件名。我们在内核中维护一个扩展版的 `Dirent`，其支持连续的长文件名、`Dirent` 父子关系连接，在内存中以 **root为根的多叉树形式** 维护，每一个磁盘中的 `Fat32Dirent` 在内存中都有至多一个扩展版的 `Dirent` 副本，并且对于内存中 `Dirent` 的读写始终会同步到内存中，保证了内存数据与磁盘数据的一致性。

### 文件系统接口

文件系统 `FileSystem` 结构体的定义：

```c
struct FileSystem {
	bool valid; // 是否有效
	char name[8];
	SuperBlock superBlock;	   // 超级块
	Dirent root;		   // root项
	struct Dirent *image;	   // mount对应的文件描述符
	struct Dirent *mountPoint; // 挂载点
	int deviceNumber;	   // 对应真实设备的编号，暂不使用
	struct Buffer *(*get)(struct FileSystem *fs, u64 blockNum); // 读取FS的一个Buffer
	// 强制规定：传入的fs即为本身的fs
	// 稍后用read返回的这个Buffer指针进行写入和释放动作
	// 我们默认所有文件系统（不管是挂载的，还是从virtio读取的），都需要经过缓存层
};
```

该结构体会维护超级块、根目录、绑定设备、挂载点的信息，对于从文件挂载的文件系统，还维护文件镜像的 `Dirent`。`get` 字段是该文件系统定义的读写方式，对于一般的文件系统，该字段指向的函数为：

```c
static Buffer *getBlock(FileSystem *fs, u64 blockNum) {
	if (fs->image == NULL) {
		// 是挂载了根设备，直接读取块缓存层的数据即可
		return bufRead(fs->deviceNumber, blockNum);
	} else {
		// 处理挂载了文件的情况
		Dirent *img = fs->image;
		FileSystem *parentFs = fs->image->fileSystem;
		int blockNo = fileBlockNo(parentFs, img->firstClus, blockNum);
		return bufRead(parentFs->deviceNumber, blockNo);
	}
}
```

