#include <types.h>
#include <proc/procarg.h>
#include <param.h>
#include <proc/proc.h>
#include <lib/transfer.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/printf.h>

// 初始化栈区域
/**
 * @brief 将内核的数据压到用户栈上
 * @param argpt 用户页表，即压入数据的页表
 * @param align 是否需要对其（16字节对齐）
 * @return 返回新的用户态栈指针
 */
u64 push_data(pte_t *argpt, u64 *sp, void *data, u64 len, int align) {
	// 将数据拷贝到用户栈
	*sp -= len;
	if (align) {
		*sp -= *sp % 16;
	}
	copy_out(argpt, *sp, data, len);
	return *sp;
}

/**
 * @brief
 * 从in_pt对应的页表上取数据（通过arg_array指针），存入数组kstr_array中。
 * 其中数组中的字符串都通过kmalloc分配，需要释放
 * @param arg_array 指向参数字符串的用户地址空间指针，若为NULL表示没有参数
 * @return 返回内核字符串数组的长度
 */
static int copyin_uarg_array(pte_t *in_pt, char **arg_array, char *kstr_array[]) {
	int arg_count = 0;

	if (arg_array != NULL) {
		while (1) {
			// 1. 拷贝指向参数字符串的用户地址空间指针
			char *arg;
			char *buf = kmalloc(MAXARGLEN + 1);
			copy_in(in_pt, (u64)(&arg_array[arg_count]), &arg, sizeof(char *));
			if (arg == NULL) {
				kfree(buf);
				break;
			}

			// 2. 从用户地址空间拷贝参数字符串
			copy_in_str(in_pt, (u64)arg, buf, MAXARGLEN);
			buf[MAXARGLEN] = '\0';

			size_t len = strlen(buf) + 1;
			buf[len - 1] = '\0';

			// 4. 记录内核字符串指针
			kstr_array[arg_count] = buf;

			arg_count += 1;
			assert(arg_count < MAXARG);
		}
	}
	// 5. 参数数组以NULL表示结束
	kstr_array[arg_count] = NULL;

	return arg_count;
}

/**
 * @brief 拷入(追加)来自内核的参数数组（arg_array来自内核）
 * @note 传入的数组arg_buf需要以NULL结尾，arg_array也需要以NULL结尾
 * @return 返回arg_buf的长度
 */
int push_karg_array(pte_t *out_pt, char **karg_array, u64 *p_sp, char *arg_buf[]) {
	int arg_count = 0;
	while (arg_buf[arg_count] != NULL) {
		arg_count += 1;
		assert(arg_count < MAXARG);
	}

	// 目前arg_buf[arg_count] == NULL
	for (int i = 0; karg_array[i] != NULL; i++) {
		// 1. 从内核拷贝参数字符串
		char *buf = karg_array[i];
		size_t len = strlen(buf) + 1;
		// log(PROC_GLOBAL, "push str: %s\n", buf);

		// 3. 向栈上压入argv字符串
		push_data(out_pt, p_sp, buf, len, true);

		// 4. 记录参数字符串的用户地址空间指针
		arg_buf[arg_count] = (char *)*p_sp;

		arg_count += 1;
		assert(arg_count < MAXARG);
	}

	// 5. 参数数组以NULL表示结束
	arg_buf[arg_count] = NULL;
	return arg_count + 1;
}

/**
 * @brief 释放以kmalloc形式分配的内核字符串数组
 */
static void free_kstr_array(char *kstr_arr[]) {
	for (int i = 0; kstr_arr[i] != NULL; i++) {
		kfree(kstr_arr[i]);
	}
}

/**
 * @brief
 * 从in_pt对应的页表上取数据（通过arg_array指针），压到out_pt对应的栈上（栈指针为*p_sp）。新的栈上地址存储在arg_buf数组中
 * @param arg_array 指向参数字符串的用户地址空间指针，若为NULL表示没有参数
 * @param callback 用于处理内核字符串参数数组的回调函数
 * @return 返回arg_buf的长度
 */
int push_uarg_array(pte_t *in_pt, pte_t *out_pt, char **arg_array, u64 *p_sp,
			   char *arg_buf[], argv_callback_t callback) {
	char *kstr_arr[MAXARG + 1];
	int arg_count;

	copyin_uarg_array(in_pt, arg_array, kstr_arr);
	if (callback)
		callback(kstr_arr);
	arg_buf[0] = NULL;
	arg_count = push_karg_array(out_pt, kstr_arr, p_sp, arg_buf);
	free_kstr_array(kstr_arr);

	return arg_count;
}

/**
 * @brief 将辅助数组的内容追加到arg_buf中
 * @param auxiliary_vector 是一个二元组
 * @param p_len 表示arg_buf的长度
 */
void append_auxiliary_vector(char *arg_buf[], u64 auxiliary_vector[], u64 *p_len) {
	arg_buf[*p_len] = (char *)auxiliary_vector[0];
	arg_buf[*p_len + 1] = (char *)auxiliary_vector[1];
	*p_len += 2;
}
