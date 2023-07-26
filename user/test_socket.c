#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

#define AF_INET		2
#define PORT 23
#define BACKLOG 5 // 最大监听数

/* Address to accept any incoming messages.  */
#define	INADDR_ANY		((uint) 0x00000000)

char msg_send[] = "Sender's Message: Hello, Client!";

int server() {
	int iSocketFD = 0;
	int iRecvLen = 0;
	int new_fd = 0;
	char buf[4096] = {0};

	SocketAddr localAddr;
	SocketAddr remoteAddr;

	iSocketFD = socket(AF_INET, SOCK_STREAM, 0); //建立socket
	if(0 > iSocketFD)
	{
		printf("failed to create socket\n");
		return 0;
	}

	localAddr.family = AF_INET;    /*该属性表示接收本机或其他机器传输*/
	localAddr.port = PORT;   /*端口号*/
	localAddr.addr = INADDR_ANY;   /*IP，括号内容(0.0.0.0)表示本机IP*/

	//绑定地址结构体和socket
	if(0 > bind(iSocketFD, (void *)&localAddr, sizeof(localAddr)))
	{
		printf("bind error!\n");
		return 0;
	}

	//开启监听 ，第二个参数是最大监听数
	if(0 > listen(iSocketFD, BACKLOG))
	{
		printf("listen error\n");
		return 0;
	}

	printf("iSocketFD: %d\n", iSocketFD);

	//在这里阻塞直到接收到消息，参数分别是socket句柄，接收到的地址信息以及大小
	new_fd = accept(iSocketFD, &remoteAddr);
	if(0 > new_fd)
	{
		printf("accept error!\n");
		return 0;
	}else{
		printf("accpet success\n");
		printf("remote: port = %d, addr = %x\n", remoteAddr.port, remoteAddr.addr);

		//发送内容，参数分别是连接句柄，内容，大小，其他信息（设为0即可）
		write(new_fd, msg_send, sizeof(msg_send));
	}

	printf("accept produces new_fd: %d\n", new_fd);
	iRecvLen = read(new_fd, buf, sizeof(buf));
	if(0 >= iRecvLen)    //对端关闭连接 返回0
	{
		printf("接收失败或者对端关闭连接！\n");
	}else{
		printf("buf: %s\n", buf);
	}

	close(new_fd);
	close(iSocketFD);
	return 0;
}

int client() {
	int iSocketFD = 0; //socket句柄
	SocketAddr remoteAddr = {0}; //对端，即目标地址信息
	int ret;
	char buf[4096] = {0}; //存储接收到的数据

	iSocketFD = socket(AF_INET, SOCK_STREAM, 0); //建立socket
	if(0 > iSocketFD)
	{
		printf("create socket of client error!\n");
		return 0;
	}

	remoteAddr.family = AF_INET;
	remoteAddr.port = PORT;
	remoteAddr.addr = 0; // 目标地址：本机

	// 等待server启动
	sleep(2);

	//连接方法： 传入句柄，目标地址，和大小
	if ((ret = connect(iSocketFD, &remoteAddr, sizeof(remoteAddr))) < 0)
	{
		printf("connect failed: %d", ret);//失败时也可打印errno
	} else {
		printf("Connect Success!\n");
		read(iSocketFD, buf, sizeof(buf)); // 将接收数据打入buf，参数分别是句柄，储存处，最大长度，其他信息（设为0即可）。
		printf("Received: %s\n", buf);
	}

	close(iSocketFD);//关闭socket
	return 0;
}

int main() {
	int pid = fork();
	if (pid == 0) {
		server();
	} else {
		client();
	}
	return 0;
}
