## 概述

我们之前的memset实现是通过char字符赋值来实现的，编译后的指令为 `sb`。这样的拷贝要比`sd`的拷贝要慢整整8倍！
因此，我们使用u64类型的赋值即`sd`来加快memset的速度。此外，考虑到不对齐的情况，我们使用unlikely宏进行了处理，因为不对齐是小概率事件，大部分时间都是对齐的。

改前代码：
```c
void *memset(void *dst, int c, uint n) {
	char *cdst = (char *)dst;
	int i;
	for (i = 0; i < n; i++) {
		cdst[i] = c;
	}
	return dst;
}
```

改后代码：
```c
void *memset(void *dst, int c, uint n) {
	u8 ch = c;
	int i;
	u64 data;
	u64 *p;

	if (likely(c == 0)) {
		data = 0;
	} else {
		data = ((u64)ch << 56) | ((u64)ch << 48) | ((u64)ch << 40) | ((u64)ch << 32)
				| ((u64)ch << 24) | ((u64)ch << 16) | ((u64)ch << 8) | (u64)ch;
	}

	char *cdst = (char *)dst;

	// 开始的部分不是8对齐的
	if (unlikely(((u64)cdst) % 8 != 0)) {
		int sum = 8 - (((u64)cdst) & 8);
		for (i = 0; i < sum; i++) {
			cdst[i] = ch;
		}
		p = (u64 *)ROUNDUP((u64)cdst, 8);
		n -= sum;
	} else {
		p = (u64 *)cdst;
	}

	// 以8字节为单位拷贝
	for (i = 0; i <= n - 8; i += 8) {
		*p++ = data;
	}

	// 剩下的部分不是8对齐的
	if (unlikely(i != n)) {
		for (; i < n; i++) {
			cdst[i] = ch;
		}
	}

	return dst;
}
```

## Benchmark

我们测试的方式是：每次调用memset初始化4K字节的内存空间，总共的数据量为4G，测定总时间。

| 类别 | 4G对齐数据赋值时间/us | 4G非对齐数据赋值时间/us |
| ---- | ---- | ---- |
| slow_memset | 1011727 | 1055719 |
| fast_memset | 964920 | 1002177 |

对于对齐数据，优化过的memset时间减少了约4.6%。
对于非对齐数据，优化过的memset时间减少了约5%。

提升幅度有，但没有想象中提升那么大，推测memset的瓶颈更多是在cache。

## 测试代码

```c
#include <lib/profiling.h>
void *memset(void *dst, int c, uint n);
void *slow_memset(void *dst, int c, uint n);
char array[4096 * 10 + 20] __attribute__((aligned(4096)));

static void slow_memset_test() {
	PROFILING_START
	for (int i = 0; i < 10; i++) {
		slow_memset(array + i * 4096 + i*i % 8, 0, 4096);
	}
	PROFILING_END
}

static void fast_memset_test() {
	PROFILING_START
	for (int i = 0; i < 10; i++) {
		memset(array + i * 4096 + i*i % 8, 0, 4096);
	}
	PROFILING_END
}

void memset_test() {
	for (int i = 0; i <= 100000; i++) {
		slow_memset_test();
		fast_memset_test();
	}
}
```
