# Sd

## sd初始化

​	在对SD进行命令设置前，首先需要对跟Sd相关的寄存器进行设置

- `fmt` 寄存器设置为 0x80000
- `csdef` 寄存器设置为 1
- `csid` 寄存器设置为 0
- `sckdiv` 寄存器设置为 3000
- `csmode` 片选模式设置为 OFF，等待 10 个周期，再将片选模式改为 AUTO

此后按照下面的顺序发送 CMD 命令

- CMD0 

  ​	传输命令 `sd_cmd(0x40，0，0x95)`，用于重置 SD 卡状态

- CMD8

  ​	传输命令 `sd_cmd(0x48, 0x000001AA, 0x87)`， 如果卡设备有response，说明此卡为SD2.0以上

- ACMD41

  首先需要先发送 CMD55 表明下一个命令是特定的应用进程，传输命令 `sd_cmd(0x77, 0, 0x65)`，再传输命令 `sd_cmd(0x69, 0x40000000, 0x77)`，该指令是用来探测卡设备的工作电压是否符合host端的要求

- CMD58

  ​	传输命令 `sd_cmd(0x7A, 0, 0xFD)`，读取 OCR 寄存器

- CMD16

  ​	传输命令 `sd_cmd(0x50, 0x200, 0x15)`用于设置块大小，FarmOs中块大小设置为 512 Bytes  

## sd读写

​	在Farm Os中，使用CMD18 和 CMD24进行数据块的传输

## Sd驱动测试

```c
int sdTest() {
	// sdInit();
	for (int i = 0; i < 1024; i++) {
		binary[i] = i & 10;
	}
	sdWrite(binary, 0, 2);
	for (int i = 0; i < 1024; i++) {
		binary[i] = 0;
	}
	sdRead(binary, 0, 2);
	for (int i = 0; i < 1024; i++) {
		if (binary[i] != (i & 10)) {
			error("sd read or write is wrong, index = %d, value = %d", i, binary[i]);
			break;
		}
	}
	printf("sd test past!\n");
	return 0;
}

```