#include <types.h>
#include <builtin.h>

int strlen(const char *s);

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
		int sum = 8 - (((u64)cdst) & (8-1));
		for (i = 0; i < sum; i++) {
			cdst[i] = ch;
		}
		cdst = (char *)ROUNDUP((u64)cdst, 8);
		n -= sum;
	}
	p = (u64 *)cdst;

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

void *slow_memset(void *dst, int c, uint n) {
	char *cdst = (char *)dst;
	int i;
	for (i = 0; i < n; i++) {
		cdst[i] = c;
	}
	return dst;
}

int memcmp(const void *v1, const void *v2, uint n) {
	const uchar *s1, *s2;

	s1 = v1;
	s2 = v2;
	while (n-- > 0) {
		if (*s1 != *s2) {
			return *s1 - *s2;
		}
		s1++, s2++;
	}

	return 0;
}

void *memmove(void *dst, const void *src, uint n) {
	const char *s;
	char *d;

	if (n == 0) {
		return dst;
	}

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		while (n-- > 0) {
			*--d = *--s;
		}
	} else {
		while (n-- > 0) {
			*d++ = *s++;
		}
	}

	return dst;
}

// memcpy exists to placate GCC.  Use memmove.
void *memcpy(void *dst, const void *src, uint n) {
	return memmove(dst, src, n);
}

int strncmp(const char *p, const char *q, uint n) {
	while (n > 0 && *p && *p == *q) {
		n--, p++, q++;
	}
	if (n == 0 || (*p == 0 && *q == 0)) {
		return 0;
	}
	return (uchar)*p - (uchar)*q;
}

/**
 * @brief 在字符串buf前面插入字符串s。保证buf数组有足够的空间
 */
void strins(char *buf, const char *str) {
	int lbuf = strlen(buf);
	int i;
	int len = strlen(str);
	for (i = lbuf; i >= 0; i--) {
		buf[i + len] = buf[i];
	}
	for (i = 0; i < len; i++) {
		buf[i] = str[i];
	}
}

/**
 * @brief 在字符串buf前面插入字符串str。保证buf数组有足够的空间
 */
void strnins(char *buf, const char *str, int len) {
	int lbuf = strlen(buf);
	int i;
	for (i = lbuf; i >= 0; i--) {
		buf[i + len] = buf[i];
	}
	for (i = 0; i < len; i++) {
		buf[i] = str[i];
	}
}

/**
 * @brief 在字符串buf后面插入字符串s。保证buf数组有足够的空间
 */
void strcat(char *buf, const char *str) {
	int lbuf = strlen(buf);
	int i;
	int len = strlen(str);
	for (i = 0; i <= len; i++) {
		buf[lbuf + i] = str[i];
	}
}

char *strncpy(char *s, const char *t, int n) {
	char *os;

	os = s;
	while (n-- > 0 && (*s++ = *t++) != 0) {
		;
	}
	// 清空剩余空间
	// while (n-- > 0) {
	// 	*s++ = 0;
	// }
	return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char *safestrcpy(char *s, const char *t, int n) {
	char *os;

	os = s;
	if (n <= 0) {
		return os;
	}
	while (--n > 0 && (*s++ = *t++) != 0) {
		;
	}
	if (*(s - 1) != 0) {
		*(s - 1) = 0;
	}
	return os;
}

int strlen(const char *s) {
	int n;

	for (n = 0; s[n]; n++) {
		;
	}
	return n;
}

// strchr
// Path: lib/string.c
const char *strchr(const char *s, int c) {
	for (; *s; s++) {
		if (*s == c) {
			return s;
		}
	}
	return 0;
}
