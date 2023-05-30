/**
 * 此文件是用户程序的入口
 */

	.section .text
	.globl _userStart

_userStart:
	mv a0, sp
	j libMain

