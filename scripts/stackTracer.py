import os
import sys
import argparse

def init_symtable():
	global asm, symbol_list
	symbol_list = []
	pos = 0
	for i in range(len(asm)):
		if asm[i].startswith("SYMBOL TABLE:"):
			pos = i

	if pos == 0:
		print("找不到符号表")
		exit(0)

	for i in range(pos+1, len(asm)):
		if asm[i].startswith("\n"):
			break
		elif asm[i].startswith("0000000000000000"):
			# 不是有效的函数符号
			continue
		else:
			split = asm[i].split()
			if len(split) < 6:
				continue
			addr = "0x" + split[0]
			func = split[5]
			symbol_list.append((int(addr, base=0), func))

	symbol_list.sort(key=lambda x: x[0])

def get_func_by_pc(pc):
	global symbol_list
	if pc < symbol_list[0][0]:
		print("invalid pc %x" % pc)
		return

	for i in range(len(symbol_list)):
		if pc < symbol_list[i][0]:
			return symbol_list[i-1][1]

	# 如果最后仍没有找到地址比pc大的符号起始处，就默认返回最后一个符号的名称
	return symbol_list[-1][1]

def get_ra_offset(func):
	global asm
	to_search = f"<{func}>:"
	on = False
	for line in asm:
		if on:
			if "addi\tsp,sp,-" in line:
				off = int(line.split(",")[2][1:-1])
				return off - 8
		elif to_search in line:
			on = True
	print("func %s not found in asm" % func)
	exit(0)

def get_func_start(func):
    global symbol_list
    for item in symbol_list:
        if item[1] == func:
            return item[0]
    print("func %s not found in symbol table" % func)
    exit(0)

def fetch_mem_in_u64(addr):
    global mem, start_sp
    assert addr % 8 == 0
    assert start_sp % 8 == 0
    s = addr-start_sp
    e = addr-start_sp+8
    ra_bytes = bytes(mem[s:e])
    return int.from_bytes(ra_bytes, sys.byteorder)

def main():
	global asm, mem, start_sp
	parser = argparse.ArgumentParser(description='用户程序栈分析器')
	parser.add_argument("start_sp", type=str, help="用户程序栈起始地址，需要为十六进制数")
	parser.add_argument("mem", type=str, help="用户程序栈的内容")
	parser.add_argument("asm", type=str, help="用户程序的汇编代码，需要使用objdump -xS 反汇编得到")
	parser.add_argument("epc", type=str, help="用户程序epc，需要是十六进制数")

	args = parser.parse_args()

	start_sp = int(args.start_sp, base=0)
	mem = eval(args.mem)
	asm_path = args.asm
	epc = int(args.epc, base=0)

	with open(asm_path, 'r') as f:
		asm = f.readlines()

	init_symtable()
	print("USTACK From TOP:")

	pc = epc
	sp = start_sp

	while True:
		func = get_func_by_pc(pc)
		ra_offset = get_ra_offset(func)
		func_start = get_func_start(func)
		print("func %s: pc = %x (%s+%x)" % (func, pc, func, pc-func_start))
		print(f"ra_off = {ra_offset}")

		# 计算本函数存储的ra的位置（即上层函数pc存放的位置）
		ra_addr = sp + ra_offset
		pc = fetch_mem_in_u64(ra_addr)
		# 计算上一个函数的起始sp
		sp = sp + ra_offset + 8

		# 最多追溯到main函数
  		# 如果是muslc写的程序，可以追溯到__libc_start_main
		if func == "main":
			break

if __name__ == '__main__':
	main()

