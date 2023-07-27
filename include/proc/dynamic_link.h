#ifndef _DYNAMIC_LINK_H
#define _DYNAMIC_LINK_H
#include <types.h>

typedef struct stack_arg stack_arg_t;
typedef struct thread thread_t;

// 解析ELF文件信息，以填充辅助数组
void parseElf(thread_t *td, const void *binary, size_t size, stack_arg_t *parg);
#endif
