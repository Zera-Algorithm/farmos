# VFS

FarmOS实现了一个类似Linux的多层文件系统，实现了丰富的抽象。FarmOS的VFS分为驱动层、缓冲区层、簇层、文件系统层、文件描述符层共5个层次。

分层的结构不仅有利于多人协作开发，还降低了各层的实现复杂度，并增加了文件系统的扩展能力。分层的结构还帮助我们能更加轻松地实现**多文件系统**（从文件挂载的文件系统与从设备挂载的文件系统并存）、丰富的**文件描述符抽象**（管道、控制台、文件均可通过文件描述符管理）。

## VFS 驱动层

### 概述

VFS 驱动层是访问磁盘的最底层接口，负责按块读写磁盘。本层中，FarmOS 实现了 VirtIO Legacy 驱动。

VirtIO 驱动的逻辑是首先向磁盘发出读或写的请求，请求中记录要读或写的缓冲区的内存地址，然后设置等待标志位，等待磁盘完成请求的操作后发送中断。在中断处理程序中，VirtIO 驱动会将等待的标志位取消，告知前台等待的程序磁盘交互已完成。这样，就完成了一次磁盘的读或写操作。

### VirtIO 驱动接口

VirtIO 驱动为上层提供了2个接口：

* `void virtio_disk_init(void)`: 初始化磁盘，检查磁盘的型号是否与驱动一致。如果一致，就继续之后的初始化工作；如不一致，则直接报错。
	> 具体而言，这里的版本检查主要是检查VirtIO设备是否是磁盘设备，且是否是Legacy的VirtIO设备

* `void virtio_disk_rw(Buffer *b, int write)`: 进程磁盘读写操作，依赖于 `write` 参数，将Buffer中的数据写入磁盘，或者从磁盘读取数据写入Buffer。要读写的块号需要在 `Buffer` 结构体里面标明。


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

对于读写，分别需要按如下方式访问簇层：
* 读：`bufRead` 获取缓冲区，数据处理，`bufRelease` 释放缓冲区。
* 写：`bufRead` 获取缓冲区，将准备好的数据写入 `Buffer` 中，`bufWrite`，`bufRelease` 释放缓冲区。


## VFS 簇层

### 概述

FAT32 文件系统簇层的主要功能是管理簇，并向 FAT32 文件系统的上层屏蔽对缓冲区的操作，提供统一的簇读写接口。

我们注意到FAT32文件系统可以根据镜像大小取不同的簇大小，因此我们创建簇层，其目的就是对上面的文件系统层屏蔽由于簇大小不同而造成的文件访问差异，使得上层能够专心处理文件事务而无需考虑底层的簇大小。这样，上层可以仅仅以簇号来读写磁盘，而无需考虑底层的块号。

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

### 概述

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

文件系统支持文件系统的分配、释放、查找、挂载和初始化这几个功能：

`fs.h`:

```c
void allocFs(struct FileSystem **pFs);
void deAllocFs(struct FileSystem *fs);
FileSystem *findFsBy(findfs_callback_t findfs, void *data);
```

`vfs.h`

```c
void initRootFs();
int mountFs(char *special, Dirent *baseDir, char *dirPath);
int umountFs(char *dirPath, Dirent *baseDir);
```

接口函数意义如下：

* `allocFs`：分配一个空的文件系统结构体
* `deAllocFs`：回收一个文件系统结构体
* `findFsBy`：找到使回调函数返回值不为0的第一个FileSystem结构体
* `initRootFs`：初始化根文件系统
* `mountFs`：解析设备或文件中的文件系统信息，并将文件系统挂载在某个目录上
* `umountFs`：将某个目录上挂载的文件系统卸载

### 文件接口

文件接口的功能是以目录项 `Dirent` 为单位管理文件。在本层，我们实现了FAT32文件系统的全部功能，能够在FAT32文件系统的框架下实现按路径获取文件、文件读写、新建文件夹等功能。

FarmOS文件系统特性：

* 支持FAT32的现代特性：长文件名
* 支持链接文件
	> FAT32并不支持软链接文件。我们创新地使用FAT32的文件属性 `DIR_Attr` 的保留位作为判别是否是链接文件的依据。除此之外，链接文件与普通文件并无二致。链接文件的内容是被链接文件的路径。利用这一点，我们可以很方便的实现从链接文件到被链接文件的跳转。
* 获取文件时支持按绝对路径和相对路径寻址

下面是文件接口的声明：

`vfs.h`

```c
struct Dirent *getFile(struct Dirent *baseDir, char *path);
int createFile(struct Dirent *baseDir, char *path, Dirent **file);
int fileRead(struct Dirent *file, int user, u64 dst, uint off, uint n);
int fileWrite(struct Dirent *file, int user, u64 src, uint off, uint n);
int linkAt(struct Dirent *oldDir, char *oldPath, struct Dirent *newDir, char *newPath);
int unLinkAt(struct Dirent *dir, char *path);
int makeDirAt(Dirent *baseDir, char *path, int mode);
void fileStat(struct Dirent *file, struct kstat *pKStat);
int dirGetDentFrom(Dirent *dir, u64 offset, struct Dirent **file, int *next_offset,
		   longEntSet *longSet);
void fileGetPath(Dirent *dirent, char *path);
```

对所有文件或目录进行操作之前首先需要使用 `getFile` 获取其对应的 `Dirent`，然后使用 `Dirent` 完成后续的操作。

接口函数意义如下：

* `getFile`：根据文件的绝对路径或者相对路径获取文件的 `Dirent`
* `createFile`：创建一个文件。如果文件已存在，则返回错误
* `fileRead`：给定文件内偏移和读取长度，读取一个文件
* `fileWrite`：给定文件内偏移和写入长度，写入一个文件
* `linkAt`：为文件创建一个链接
* `unLinkAt`：取消文件的链接，即删除文件
* `makeDirAt`：新建文件夹
* `fileStat`：获取文件的状态信息，存储到 `struct kstat` 结构体中
* `dirGetDentFrom`：从某个目录中读取目录项。当读取一个目录项后，下一次读取的是它的后继目录项。读到目录结束时，返回0
* `fileGetPath`：根据 `Dirent`，获取文件的路径

## VFS 文件描述符层

### 概述

文件描述符层是VFS的最上层，负责通过系统调用直接与用户进程交互。文件描述符层以用户的文件描述符 `Fd` 为载体与用户进程交互，支持用户打开或创建文件、读写文件和新建文件夹等。

在文件描述符层，我们维护两个数据结构。一个是进程控制块中的进程Fd列表 `fdList`，以数组形式存储，记录指向的内核文件描述符 `kernFd` 的编号；另一个是内核描述符 `kernFd` 表，存储内核文件描述符。其结构如下所示：

```c
struct Fd {
	Dirent *dirent;
	struct Pipe *pipe;
	int type;
	uint offset;
	uint flags;
	struct kstat stat;
};
```

文件描述符结构体中的 `type` 描述了文件的类型，为文件、管道、控制台其中一种；`offset` 记录了当前读或写的偏移；`flags` 记录了文件的权限。

下面的图展示了用户Fd与内核fd的关系：

![img](./assets/%E5%86%85%E6%A0%B8fd%E4%B8%8E%E7%94%A8%E6%88%B7fd.drawio.png)

在进程fork时，父进程要将其所有有效的文件描述符按照相同的fd编号克隆给自己的子进程，使其与自己指向同一个 `kernFd`。dup的原理也是如此。因此，我们为 `kernFd` 维护了一个引用计数，表明指向此 `kernFd` 的进程fd个数。当引用计数归0时，将该 `kernFd` 回收。

两个进程共享 `kernFd` 时，同时也共享了读写的偏移 `offset`。当一方使用 `read` 或 `write` 读写自己的文件描述符时，也会改变对应的 `kernFd` 的偏移，另一个进程会感知到 `offset` 的变化。但是，两个进程仅共享 `Dirent` 而不共享 `kernFd` 时，由于使用两个不同的 `kernFd`，它们读写文件的偏移是独立的。

### 文件描述符层接口

接口声明如下：

`pipe.h`：

```c
int pipe(int fd[2]);
```

`file.h`：

```c
int openat(int fd, u64 filename, int flags, mode_t mode);
```

`fd.h`：
```c
int fdAlloc();
int closeFd(int fd);
void cloneAddCite(uint i);
int read(int fd, u64 buf, size_t count);
int write(int fd, u64 buf, size_t count);
int readConsoleAlloc();
int writeConsoleAlloc();
int errorConsoleAlloc();
int dup(int fd);
int dup3(int old, int new);
void freeFd(uint i);
int getdents64(int fd, u64 buf, int len);
int makeDirAtFd(int dirFd, u64 path, int mode);
int linkAtFd(int oldFd, u64 pOldPath, int newFd, u64 pNewPath, int flags);
int unLinkAtFd(int dirFd, u64 pPath);
int fileStatFd(int fd, u64 pkstat);
int getDirentByFd(int fd, Dirent **dirent, int *kernFd);
```

这些接口都是对进程开放的，使用的fd编号是属于特定进程的fd编号，内核若要访问文件请使用文件系统层的接口。

文件描述符层的接口的功能如下：

* `pipe`：创建管道，读端和写端各返回一个文件描述符
* `openat`：打开或创建一个文件
* `fdAlloc`：分配内核文件描述符
* `closeFd`：关闭进程fd
* `cloneAddCite`：进程在clone时，会复制文件描述符，需调用此函数将内核fd的数目加一
* `read`、`write`：读写文件，并将内核文件描述符中的 `offset` 字段加上读或写的字节数
* `readConsoleAlloc`、`writeConsoleAlloc`、`errorConsoleAlloc`：为进程分配标准输入、标准输出、错误输出的 `kernFd`
* `dup`、`dup3`：复制进程描述符，但复制后的文件描述符仍指向同一个 `kernFd`
* `freeFd`：释放内核文件描述符
* `getdents64`：从目录中获取其中的目录项
* `makeDirAtFd`：基于某个文件夹fd的位置创建文件夹
* `linkAtFd`、`unLinkAtFd`：创建/删除链接
* `fileStatFd`：获取文件信息
* `getDirentByFd`：通过进程的fd获取其所对应的文件 `Dirent`
