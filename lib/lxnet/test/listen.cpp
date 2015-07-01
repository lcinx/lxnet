
#include <stdlib.h>
#include <stdio.h>
#include "lxnet.h"
#include "msgbase.h"
#include "log.h"

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

	lxnet::Listener *list = lxnet::Listener::Create();
	if (!list->Listen(30012, 10))
	{
		printf("listen error\n");
	}
	if (list)
	{
		MessagePack sendpack;
		MessagePack *recvpack;
		char neirong[1024*30]="a1234567";
		//char recvneirong[32*1024];
		int size = sizeof(neirong);
		sendpack.PushBlock(neirong, size);


		printf("listen succeed!\n");
		lxnet::Socketer *newclient = NULL;
		while (1)
		{
#ifdef WIN32
			Sleep(100);
#else
			usleep(100000);
#endif
			lxnet::net_run();
			if (!list->CanAccept())
				continue;
			if (!(newclient = list->Accept()))
				continue;

			printf("accept succeed!\n");
			//newclient->UseCompress();
			//newclient->UseUncompress();
			//newclient->UseDecrypt();
			//newclient->UseEncrypt();
			newclient->SetSendLimit(-1);
			//newclient->SetRecvLimit(16*1024);

			newclient->CheckRecv();
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
				}
				else
#ifdef WIN32
					Sleep(0);
#else
					sleep(0);
#endif

				lxnet::net_run();
				if (newclient->IsClose())
				{
					system("pause");
					lxnet::Socketer::Release(newclient);
					goto s_exit;
				}
			}
		}
	}
	else
	{
		printf("listen failed!\n");
	}

s_exit:
	system("pause");
	printf("begin ... release..\n");
#ifdef WIN32
	Sleep(1000);
#else
	usleep(1000000);
#endif
	lxnet::Listener::Release(list);
	lxnet::net_release();
	system("pause");
	return 0;
}
