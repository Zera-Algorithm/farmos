#include <fs/dirent.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <types.h>

char buf[8192];
void fat32Test() {
	// 测试读取文件
	Dirent *file = getFile(NULL, "/text.txt");
	assert(file != NULL);
	panic_on(file_read(file, 0, (u64)buf, 0, file->file_size) < 0);
	printf("%s\n", buf);

	// 测试写入文件
	char *str = "Hello! I\'m "
		    "zrp!"
		    "\n3233333333233333333233333333233333333233333333233333333233333333233333333233"
		    "333333233333333233333333233333333233333333233333333233333333233333333233333333"
		    "233333333233333333222222222233233333333233333333233333333233333333233333333233"
		    "333333233333333233333333233333333233333333233333333233333333233333333233333333"
		    "233333333233333333222222222233233333333233333333233333333233333333233333333233"
		    "333333233333333233333333233333333233333333233333333233333333233333333233333333"
		    "233333333233333333222222222233233333333233333333233333333233333333233333333233"
		    "333333233333333233333333233333333233333333233333333233333333233333333233333333"
		    "23333333323333333322222222222222222222222222\n This is end!";
	int len = strlen(str) + 1;
	panic_on(file_write(file, 0, (u64)str, 0, len) < 0);

	// 读出文件
	panic_on(file_read(file, 0, (u64)buf, 0, file->file_size) < 0);
	printf("%s\n", buf);

	// TODO: 写一个删除文件的函数
	// 测试创建文件
	panic_on(createFile(NULL, "/zrp123456789zrp.txt", &file) < 0);
	char *str2 = "Hello! I\'m zrp!\n";
	panic_on(file_write(file, 0, (u64)str2, 0, strlen(str2) + 1) < 0);

	// 读取刚创建的文件
	file = getFile(NULL, "/zrp123456789zrp.txt");
	assert(file != NULL);
	panic_on(file_read(file, 0, (u64)buf, 0, file->file_size) < 0);
	printf("file zrp.txt: %s\n", buf);

	log(LEVEL_GLOBAL, "FAT32 Test Passed!\n");
}
