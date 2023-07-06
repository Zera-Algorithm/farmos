#include <fs/dirent.h>
#include <fs/fat32.h>
#include <fs/file_device.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/string.h>

// mount之后，目录中原有的文件将被暂时取代为挂载的文件系统内的内容，umount时会重新出现
int mount_fs(char *special, Dirent *baseDir, char *dirPath) {
	// 1. 寻找mount的目录
	Dirent *dir = getFile(baseDir, dirPath);

	if (dir == NULL) {
		warn("dir %s is not found!\n", dirPath);
		return -1;
	}

	// 2. 寻找mount的文件
	// 特判是否是设备（deprecated）
	Dirent *image;
	if (strncmp(special, "/dev/vda2", 10) == 0) {
		image = NULL;
	} else {
		image = getFile(baseDir, special);
		if (image == NULL) {
			warn("image %s is not found!\n", special);
			file_close(image);
			return -1;
		}
	}

	// 3. 修改mount的目录的属性，并新建一个fs项目
	dir->raw_dirent.DIR_Attr |= ATTR_MOUNT;
	sync_dirent_rawdata_back(dir);

	FileSystem *fs;
	allocFs(&fs);
	fs->image = image;
	fs->deviceNumber = 0;
	fs->mountPoint = dir;
	fat32_init(fs);

	return 0;
}

int umount_fs(char *dirPath, Dirent *baseDir) {
	// 1. 寻找mount的目录
	Dirent *dir = getFile(baseDir, dirPath);
	if (dir == NULL) {
		warn("dir %s is not found!\n", dirPath);
		return -1;
	}

	// 2. 擦除目录的标记
	// 要umount的目录一般使用getFile加载出来的是其文件系统的根目录，不能直接写回
	Dirent *mntPoint = dir->file_system->mountPoint;
	if (mntPoint == NULL || dir->parent_dirent != NULL) {
		warn("unmounted dir!\n");
		return -1;
	}
	mntPoint->raw_dirent.DIR_Attr &= (~ATTR_MOUNT);
	sync_dirent_rawdata_back(mntPoint);

	// 3. 卸载fs
	FileSystem *fs = find_fs_by(find_fs_of_dir, mntPoint);
	if (fs == NULL) {
		warn("can\'t find fs of dir %s!\n", dirPath);
		return -1;
	}
	deAllocFs(fs);
	return 0;
}
