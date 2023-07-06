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

struct FileSystem *fatFs;
static void write_back_dirent(Dirent *dirent);
static int countClusters(struct Dirent *file);
