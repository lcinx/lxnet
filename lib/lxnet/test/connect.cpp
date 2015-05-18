
#include <stdlib.h>
#include <stdio.h>
#include "lxnet.h"
#include "msgbase.h"
#include "crosslib.h"

#ifdef WIN32
#include <windows.h>
#else
#include<unistd.h>
#define system(a) \
do{	printf("请按任意键继续\n");getchar();\
}	\
while(0)
#endif
int main()
{
	if(!lxnet::net_init(512, 1, 1024*32, 100, 1, 4, 1))
	{
		printf("init network error!\n");
		system("pause");
		return 0;
	}

	lxnet::Socketer *newclient = lxnet::Socketer_create();
	//newclient->UseUncompress();
	//newclient->UseCompress();
	
	while (!newclient->Connect("127.0.0.1", 30012))
	{
#ifdef WIN32
			Sleep(100);
#else
			usleep(100000);
#endif
	}

	printf("connect succeed!\n");

	MessagePack sendpack;
	MessagePack *recvpack;
	char neirong[1024*30]="a1234567";
	//char recvneirong[32*1024];
	int size = sizeof(neirong);
	sendpack.PushBlock(neirong, size);

	int sendnum = 0;
	int64 begin, end;
	begin = get_millisecond();
	newclient->SendMsg(&sendpack);
	newclient->CheckSend();
	newclient->CheckRecv();
	sendnum++;
	while(1)
	{
		recvpack = (MessagePack *)newclient->GetMsg();
		if (recvpack)
		{
			//recvpack->Begin();
			//recvpack->GetBlock(recvneirong, size);
			//if (memcmp(recvneirong, neirong, size) != 0)
			//{
			//	printf("data error!\n");
			//	system("pause");
			//}
			newclient->SendMsg(&sendpack);
			newclient->CheckSend();
			sendnum++;
			if (sendnum == 10000)
			{
				end = get_millisecond();
				printf("end - begin:%lld\n", end-begin);
				system("pause");
			}
		}
		else
#ifdef WIN32
			Sleep(0);
#else
			sleep(0);
#endif
		if (newclient->IsClose())
		{
			system("pause");
			goto s_exit;
		}
		lxnet::net_run();
	}
s_exit:
	system("pause");
	printf("begin ... release..\n");
#ifdef WIN32
	Sleep(1000);
#else
	usleep(1000000);
#endif
	lxnet::Socketer_release(newclient);
	lxnet::net_release();
	system("pause");
	return 0;
}

