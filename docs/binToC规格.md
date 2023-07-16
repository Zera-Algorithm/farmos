### binToC工具：将二进制文件转换为C数组

#### 1. 使用的语言

推荐为Python，也可以是C

#### 2. 输入

二进制文件，示例：test.bin，其内容为 "12345"

如果工具是python程序 `binToC.py`，则命令行为：

```shell
python binToC.py test.bin
```

#### 3. 输出

生成一个对应的 `.b.c` 文件，如 `test.b.c`

其内容为：

```c
char binary_test[] = {
	0x31, 0x32, 0x33, 0x34, 0x35
};
char binary_test_size = 5;
```

> 注 `0x31` ~ `0x35` 依次是'1' 到 '5'的ASCII码。

可以依照MOS编写。