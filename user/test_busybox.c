#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

int main() {
	// 执行 make sdrun 来测试busybox
	printf("test_busybox started!\n");
	int wstatus = 0;

	char *const *argvs[] = {
		// time-test
	    // (char *const[]){"/time-test", NULL},

	    // (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "socket", NULL},
		// iperf
	    (char *const[]){"/iperf3", "-s", "-p", "5001", "-D", NULL}, // server
	    (char *const[]){"/iperf3", "-c", "127.0.0.1", "-p", "5001", "-t", "2", "-i", "0", /* "-u", "-b", "1000G" ,*/ NULL}, // client
	    // (char *const[]) {"/busybox", "ash", "iperf_testcode.sh", NULL},

		// libc-bench测试
	    // (char *const[]){"/libc-bench", NULL},

	    // busybox测试
	    // (char *const[]) {"/busybox", "ash", "busybox_testcode.sh", NULL},
	    // libc-test的static测试点和dynamic测试点
	    // (char *const[]) {"/busybox", "ash", "run-static.sh", NULL},
	    // (char *const[]) {"/busybox", "ash", "run-dynamic.sh", NULL},
	    // (char *const[]) {"./runtest.exe", "-w", "entry-dynamic.exe", "tls_get_new_dtv", NULL},

		// lua测试：pass
	    // (char *const[]){"/busybox", "ash", "lua_testcode.sh", NULL},


	    // 命令行测试
	    // (char *const[]) {"/busybox", "ash", NULL},
	    // (char *const[]) {"/busybox", "ash", "cyclictest_testcode.sh", NULL},

	    // lmbench测试
	    // (char *const[]) {"/busybox", "ash", "lmbench_testcode.sh", NULL},
	    NULL};

	/*
	// libc-test的static测试点
	char *const *argvs[] = {
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "wcsstr_false_negative", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "wcsncpy_read_overflow", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "uselocale_0", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "syscall_sign_extend", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "strverscmp", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "statvfs", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "sscanf_eof", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "sigprocmask_internal", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "setvbuf_unget", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "scanf_nullbyte_char", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "scanf_match_literal_eof", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "scanf_bytes_consumed", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "rlimit_open_files", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "rewind_clear_error", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "regexec_nosub", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "regex_negated_range", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "regex_escaped_high_byte", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "regex_ere_backref", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "regex_bracket_icase", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "regex_backref_0", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "putenv_doublefree", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_n", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_g_zeros", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_g_round", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "printf_1e9_oob", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "mkstemp_failure", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "mkdtemp_failure", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "memmem_oob", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "memmem_oob_read", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "mbsrtowcs_overflow", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "malloc_0", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "lseek_large", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "lrand48_signextend", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "iswspace_null", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "inet_pton_empty_last_field", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "inet_ntop_v4mapped", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "iconv_roundtrips", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "getpwnam_r_errno", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "getpwnam_r_crash", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "ftello_unflushed_append", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "fpclassify_invalid_ld80", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "fgetwc_buffering", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "fgets_eof", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "fflush_exit", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "dn_expand_ptr_0", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "dn_expand_empty", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "daemon_failure", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "pleval", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "wcstol", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "wcsstr", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "utime", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "ungetc", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "udiv", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "tls_align", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "time", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "tgmath", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "swprintf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "strtold", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "strtol", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "strtof", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "strtod_simple", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "strtod", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "strptime", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "string_strstr", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "string_strcspn", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "string_strchr", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "string_memset", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "string_memmem", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "string_memcpy", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "string", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "strftime", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "stat", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "sscanf_long", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "sscanf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "socket", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "snprintf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "setjmp", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "search_tsearch", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "search_lsearch", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "search_insque", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "search_hsearch", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "random", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "qsort", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "memstream", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "mbc", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "inet_pton", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "iconv_open", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "fwscanf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "fscanf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "fnmatch", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "fdopen", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "env", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "dirname", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "crypt", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "clock_gettime", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "clocale_mbfuncs", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "basename", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-static.exe", "argv", NULL},
	    NULL,
	};
	*/

	/*
	char *const *argvs[] = {
		(char *const[]) {"./runtest.exe", "-w", "entry-dynamic.exe", "qsort", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "wcsstr_false_negative", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "wcsncpy_read_overflow", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "uselocale_0", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "syscall_sign_extend", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "strverscmp", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "statvfs", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf_eof", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "sigprocmask_internal", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "setvbuf_unget", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_nullbyte_char", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_match_literal_eof", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_bytes_consumed", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "rlimit_open_files", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "rewind_clear_error", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "regexec_nosub", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "regex_negated_range", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "regex_escaped_high_byte", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "regex_ere_backref", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "regex_bracket_icase", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "regex_backref_0", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "putenv_doublefree", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_n", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_g_zeros", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_g_round", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "printf_1e9_oob", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "mkstemp_failure", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "mkdtemp_failure", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "memmem_oob", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "memmem_oob_read", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "mbsrtowcs_overflow", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "malloc_0", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "lseek_large", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "lrand48_signextend", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "iswspace_null", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "inet_pton_empty_last_field", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "inet_ntop_v4mapped", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "iconv_roundtrips", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "getpwnam_r_errno", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "getpwnam_r_crash", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "ftello_unflushed_append", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "fpclassify_invalid_ld80", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "fgetwc_buffering", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "fgets_eof", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "fflush_exit", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "dn_expand_ptr_0", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "dn_expand_empty", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "daemon_failure", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "pleval", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "wcstol", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "wcsstr", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "utime", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "ungetc", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "udiv", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "tls_align", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "time", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "tgmath", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "swprintf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "strtold", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "strtol", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "strtof", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "strtod_simple", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "strtod", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "strptime", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "string_strstr", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "string_strcspn", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "string_strchr", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "string_memset", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "string_memmem", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "string_memcpy", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "string", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "strftime", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "stat", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf_long", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "socket", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "snprintf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "setjmp", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "search_tsearch", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "search_lsearch", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "search_insque", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "search_hsearch", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "random", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "qsort", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "memstream", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "mbc", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "inet_pton", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "iconv_open", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "fwscanf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "fscanf", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "fnmatch", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "fdopen", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "env", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "dirname", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "crypt", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "clock_gettime", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "clocale_mbfuncs", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "basename", NULL},
	    (char *const[]){"./runtest.exe", "-w", "entry-dynamic.exe", "argv", NULL},
	    NULL,
	};
	*/

	// // libc-test for pthread
	// char *const *argvs[] = {
	// (char *const[]) {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel", NULL},
	// 	(char *const[]) {"./runtest.exe", "-w", "entry-static.exe", "pthread_rwlock_ebusy",
	// NULL}, 	(char *const[]) {"./runtest.exe", "-w", "entry-static.exe",
	// "pthread_once_deadlock", NULL}, 	(char *const[]) {"./runtest.exe", "-w",
	// "entry-static.exe", "pthread_exit_cancel", NULL}, 	(char *const[]) {"./runtest.exe", "-w",
	// "entry-static.exe", "pthread_condattr_setclock", NULL}, 	(char *const[]) {"./runtest.exe",
	// "-w", "entry-static.exe", "pthread_cond_smasher", NULL}, 	(char *const[])
	// {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel_sem_wait", NULL}, 	(char
	// *const[]) {"./runtest.exe", "-w", "entry-static.exe", "pthread_robust_detach", NULL},
	// 	(char *const[]) {"./runtest.exe", "-w", "entry-static.exe", "pthread_tsd", NULL},
	// 	(char *const[]) {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond", NULL},
	// 	(char *const[]) {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel_points",
	// NULL}, 	NULL
	// };

	char *const envp[] = {"LD_LIBRARY_PATH=/", NULL};

	int child = fork();
	if (child) {
		wait(&wstatus);
	} else {
		// child
		printf("[test_init]: before execve! I'm %x\n", getpid());

		for (int i = 0; argvs[i] != NULL; i++) {
			int pid = fork();
			if (pid == 0) { // child
				// 打印argvs[i]指向的字符串数组
				printf("\n$ ");
				for (int j = 0; argvs[i][j] != NULL; j++) {
					printf("%s ", argvs[i][j]);
				}
				printf("\n");

				execve(argvs[i][0], argvs[i], envp);
			} else {
				wait(&wstatus);
			}
		}
	}

	// char *const argv[] = {"/libc-bench", NULL};
	return 0;
}
