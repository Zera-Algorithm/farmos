/**
 * 此文件是用户程序的入口
 */

	.section .text
	.globl _userStart

_userStart:
	// 保证初始时，a0 = argc, a1 = argv
	j libMain

