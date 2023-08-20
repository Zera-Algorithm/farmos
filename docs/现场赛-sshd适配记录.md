为了支持sshd for linux运行，我们做了若干改动：

1. 添加fchmod、setgroups、getrandom

2. 我们发现部分syscall的返回值错误地定义为void型，导致返回不确定值，导致用户程序出错。如uname应返回0但返回一个非零数导致用户程序崩溃

3. 修复输入字符不识别的问题（'\r' -> '\n'）

4. 添加用户文件/etc/passwd

5. 修改host密钥的权限为0600（为0777时用户程序报告权限过于开放），/var/empty目录的权限改为0711（只允许owner(sshd)访问）

6. sshd：修改信号handler的判断范围，允许小于0x10000地址的handler函数

7. 加了一些必要文件：hosts、services、sshd_config、/etc/init.d/ssh；二进制文件：ssh、sshd、ssh-keygen

8. 预先执行 `busybox --install /bin`，安装一些 ssh-key-id 缺失的linux工具，以支持 ssh-key-id 脚本运行

做完更改后，我们支持运行 `ssh`、`sshd`，并支持一部分 `ssh` 到 `sshd` 的通信。


