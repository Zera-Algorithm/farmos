#ifndef _PROCARG_H
#define _PROCARG_H
#include <types.h>

// 处理argv参数的回调函数
typedef void (*argv_callback_t)(char *kstr_arr[]);

u64 push_data(pte_t *argpt, u64 *sp, void *data, u64 len, int align);
void append_auxiliary_vector(char *arg_buf[], u64 auxiliary_vector[], u64 *p_len);
int push_karg_array(pte_t *out_pt, char **arg_array, u64 *p_sp, char *arg_buf[]);
int push_uarg_array(pte_t *in_pt, pte_t *out_pt, char **arg_array, u64 *p_sp,
			   char *arg_buf[], argv_callback_t callback);

// argvbuf(need kfree), total_len, argc
typedef struct stack_arg {
	char **argvbuf;
	u64 total_len;
	u64 argc;
} stack_arg_t;

#endif
