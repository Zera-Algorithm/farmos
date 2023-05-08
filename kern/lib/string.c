#include <lib/string.h>

void *memcpy(void *dst, const void *src, size_t n) {
    char *d = dst;
    const char *s = src;
    while (n-- > 0) {
        *d++ = *s++;
    }
    return dst;
}


void *memset(void *dst, int c, size_t n) {
    char *d = dst;
    while (n-- > 0) {
        *d++ = c;
    }
    return dst;
}


size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++ != '\0') {
        n++;
    }
    return n;
}


char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++) != '\0') {
        ;
    }
    return dst;
}


const char *strchr(const char *s, int c) {
    while (*s != '\0') {
        if (*s == c) {
            return s;
        }
        s++;
    }
    return NULL;
}


int strcmp(const char *p, const char *q) {
    while (*p && *p == *q) {
        p++, q++;
    }
    return (unsigned char) *p - (unsigned char) *q;
}

