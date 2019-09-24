#include <stdlib.h>
#include <stdio.h>
#include "lxnet.h"
#include "msgbase.h"
#include "log.h"

#ifdef _WIN32
	#include <windows.h>

	#define delaytime(v)	Sleep(v)
#else
	#include <unistd.h>

	#define delaytime(v)	usleep(v * 1000)
	#define system(a)
#endif


int main() {

	if (!lxnet::net_init(512, 1, 32 * 1024, 100, 1, 4, 1)) {
		printf("init network error!\n");
		system("pause");
		return 0;
	}

	lxnet::Listener *list = lxnet::Listener::Create();
	if (!list || !list->Listen(30012, 10)) {
		printf("listen error\n");
		return 0;
	}

	printf("listen succeed!\n");

	MessagePack sendpack;
	MessagePack *recvpack;
	char neirong[30 * 1024]="a1234567";
	//char recvneirong[32 * 1024];
	int size = sizeof(neirong);
	sendpack.PushBlock(neirong, size);

	lxnet::Socketer *newclient = NULL;
	while (1) {
		delaytime(100);

		lxnet::net_run();
		if (!list->CanAccept())
			continue;

		if (!(newclient = list->Accept()))
			continue;

		printf("accept succeed!\n");
		break;
	}

	//newclient->UseCompress();
	//newclient->UseUncompress();
	//newclient->UseDecrypt();
	//newclient->UseEncrypt();
	newclient->SetSendLimit(-1);
	//newclient->SetRecvLimit(16 * 1024);
	newclient->CheckRecv();

	while (1) {
		recvpack = (MessagePack *)newclient->GetMsg();
		if (recvpack) {
			//recvpack->Begin();
			//recvpack->GetBlock(recvneirong, size);
			//if (memcmp(recvneirong, neirong, size) != 0) {
			//	printf("data error!\n");
			//	break;
			//}
			newclient->SendMsg(&sendpack);
			newclient->CheckSend();
		} else {
			delaytime(0);
		}

		if (newclient->IsClose()) {
			lxnet::Socketer::Release(newclient);
			newclient = NULL;
			break;
		}

		lxnet::net_run();
	}


	delaytime(1000);

	lxnet::Listener::Release(list);
	lxnet::net_release();
	system("pause");
	return 0;
}

