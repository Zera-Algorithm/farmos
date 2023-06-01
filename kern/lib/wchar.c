// 处理 wchar 相关事务
#include <lib/string.h>
#include <types.h>

int wstrlen(const wchar *s) {
	int n;

	for (n = 0; s[n]; n++) {
		;
	}
	return n;
}

/**
 * @brief 在wchar字符串buf前面插入字符串s。保证buf数组有足够的空间
 */
void wstrnins(wchar *buf, const wchar *str, int len) {
	int lbuf = wstrlen(buf);
	int i;
	for (i = lbuf; i >= 0; i--) {
		buf[i + len] = buf[i];
	}
	for (i = 0; i < len; i++) {
		buf[i] = str[i];
	}
}

/**
 * @brief 将wide char字符串转化为char字符串
 * @return 返回写入字符串的长度
 */
int wstr2str(char *dst, const wchar *src) {
	int i;
	for (i = 0; src[i]; i++) {
		dst[i] = (char)(src[i] & 0xff);
	}
	dst[i] = 0;
	return i;
}
