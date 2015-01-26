
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <stdio.h>
#include <string.h>
#include "ossome.h"
#include "net_module.h"
#include "lxnet.h"
#include "net_buf.h"
#include "pool.h"
#include "msgbase.h"
#include "log.h"
#include "crosslib.h"
#include "lxnet_datainfo.h"

struct datainfomgr {
	bool isinit;
	int64 lasttime;
	struct datainfo datatable[enum_netdata_end];
};

static struct datainfomgr s_datamgr = {false};

struct infomgr {
	bool isinit;
	struct poolmgr *encrypt_pool;
	spin_lock_struct encrypt_lock;

	struct poolmgr *socket_pool;
	spin_lock_struct socket_lock;
	
	struct poolmgr *listen_pool;
	spin_lock_struct listen_lock;
};
static struct infomgr s_infomgr = {false};

struct encryptinfo {
	enum {
		enum_encrypt_len = 32,
	};

	int maxidx;
	int nowidx;
	char buf[enum_encrypt_len];
};

static inline void on_sendmsg(size_t len) {
	assert(s_datamgr.isinit);
	s_datamgr.datatable[enum_netdata_total].sendmsgnum += 1;
	s_datamgr.datatable[enum_netdata_total].sendbytes += len;

	s_datamgr.datatable[enum_netdata_now].sendmsgnum += 1;
	s_datamgr.datatable[enum_netdata_now].sendbytes += len;
}

static inline void on_recvmsg(size_t len) {
	assert(s_datamgr.isinit);

	s_datamgr.datatable[enum_netdata_total].recvmsgnum += 1;
	s_datamgr.datatable[enum_netdata_total].recvbytes += len;

	s_datamgr.datatable[enum_netdata_now].recvmsgnum += 1;
	s_datamgr.datatable[enum_netdata_now].recvbytes += len;
}

static void datarun() {
	assert(s_datamgr.isinit);
	int64 currenttime = get_millisecond();
	if (currenttime - s_datamgr.lasttime < 1000)
		return;

	s_datamgr.lasttime = currenttime;

	time_t curtm = time(NULL);
	if (s_datamgr.datatable[enum_netdata_max].sendmsgnum <= s_datamgr.datatable[enum_netdata_now].sendmsgnum) {
		s_datamgr.datatable[enum_netdata_max].sendmsgnum = s_datamgr.datatable[enum_netdata_now].sendmsgnum;
		s_datamgr.datatable[enum_netdata_max].tm_sendmsgnum = curtm;
	}

	if (s_datamgr.datatable[enum_netdata_max].recvmsgnum <= s_datamgr.datatable[enum_netdata_now].recvmsgnum) {
		s_datamgr.datatable[enum_netdata_max].recvmsgnum = s_datamgr.datatable[enum_netdata_now].recvmsgnum;
		s_datamgr.datatable[enum_netdata_max].tm_recvmsgnum = curtm;
	}

	if (s_datamgr.datatable[enum_netdata_max].sendbytes <= s_datamgr.datatable[enum_netdata_now].sendbytes) {
		s_datamgr.datatable[enum_netdata_max].sendbytes = s_datamgr.datatable[enum_netdata_now].sendbytes;
		s_datamgr.datatable[enum_netdata_max].tm_sendbytes = curtm;
	}

	if (s_datamgr.datatable[enum_netdata_max].recvbytes <= s_datamgr.datatable[enum_netdata_now].recvbytes) {
		s_datamgr.datatable[enum_netdata_max].recvbytes = s_datamgr.datatable[enum_netdata_now].recvbytes;
		s_datamgr.datatable[enum_netdata_max].tm_recvbytes = curtm;
	}

	s_datamgr.datatable[enum_netdata_now].sendmsgnum = 0;
	s_datamgr.datatable[enum_netdata_now].recvmsgnum = 0;
	s_datamgr.datatable[enum_netdata_now].sendbytes = 0;
	s_datamgr.datatable[enum_netdata_now].recvbytes = 0;
}

static bool infomgr_init(size_t socketnum, size_t listennum) {
	if (s_infomgr.isinit)
		return false;

	s_infomgr.encrypt_pool = poolmgr_create(sizeof(struct encryptinfo), 8, socketnum * 2, 1, "encrypt buffer pool");
	s_infomgr.socket_pool = poolmgr_create(sizeof(lxnet::Socketer), 8, socketnum, 1, "Socketer obj pool");
	s_infomgr.listen_pool = poolmgr_create(sizeof(lxnet::Listener), 8, listennum, 1, "Listen obj pool");
	if (!s_infomgr.socket_pool || !s_infomgr.encrypt_pool || !s_infomgr.listen_pool) {
		poolmgr_release(s_infomgr.socket_pool);
		poolmgr_release(s_infomgr.encrypt_pool);
		poolmgr_release(s_infomgr.listen_pool);
		return false;
	}

	spin_lock_init(&s_infomgr.encrypt_lock);
	spin_lock_init(&s_infomgr.socket_lock);
	spin_lock_init(&s_infomgr.listen_lock);
	s_infomgr.isinit = true;

	s_datamgr.isinit = true;
	s_datamgr.lasttime = 0;
	for (int i = 0; i < enum_netdata_end; ++i) {
		s_datamgr.datatable[i].sendmsgnum = 0;
		s_datamgr.datatable[i].recvmsgnum = 0;
		s_datamgr.datatable[i].sendbytes = 0;
		s_datamgr.datatable[i].recvbytes = 0;
		s_datamgr.datatable[i].tm_sendmsgnum = 0;
		s_datamgr.datatable[i].tm_recvmsgnum = 0;
		s_datamgr.datatable[i].tm_sendbytes = 0;
		s_datamgr.datatable[i].tm_recvbytes = 0;
	}
	return true;
}

static void infomgr_release() {
	if (!s_infomgr.isinit)
		return;
	s_infomgr.isinit = false;
	s_datamgr.isinit = false;
	poolmgr_release(s_infomgr.socket_pool);
	poolmgr_release(s_infomgr.encrypt_pool);
	poolmgr_release(s_infomgr.listen_pool);
	spin_lock_delete(&s_infomgr.encrypt_lock);
	spin_lock_delete(&s_infomgr.socket_lock);
	spin_lock_delete(&s_infomgr.listen_lock);
}

static void encrypt_info_release(void *info) {
	if (!s_infomgr.isinit)
		return;

	spin_lock_lock(&s_infomgr.encrypt_lock);
	poolmgr_freeobject(s_infomgr.encrypt_pool, info);
	spin_lock_unlock(&s_infomgr.encrypt_lock);
}


namespace lxnet {


/* 监听*/
bool Listener::Listen(unsigned short port, int backlog) {
	return listener_listen(m_self, port, backlog);
}

/* 关闭用于监听的套接字，停止监听*/
void Listener::Close() {
	listener_close(m_self);
}

/* 测试是否已关闭*/
bool Listener::IsClose() {
	return listener_isclose(m_self);
}

/* 在指定的监听socket上接受连接*/
Socketer *Listener::Accept(bool bigbuf) {
	struct socketer *sock = listener_accept(m_self, bigbuf);
	if (!sock)
		return NULL;
	spin_lock_lock(&s_infomgr.socket_lock);
	Socketer *self = (Socketer *)poolmgr_getobject(s_infomgr.socket_pool);
	spin_lock_unlock(&s_infomgr.socket_lock);
	if (!self) {
		socketer_release(sock);
		return NULL;
	}

	self->m_encrypt = NULL;
	self->m_decrypt = NULL;
	self->m_self = sock;
	return self;
}

/* 检测是否有新的连接*/
bool Listener::CanAccept() {
	return listener_can_accept(m_self);
}

/* 设置接收数据字节的临界值，超过此值，则停止接收，若小于等于0，则视为不限制*/
void Socketer::SetRecvCritical(long size) {
	socketer_set_recv_critical(m_self, size);
}

/* 设置发送数据字节的临界值，若缓冲中数据长度大于此值，则断开此连接，若为0，则视为不限制*/
void Socketer::SetSendCritical(long size) {
	socketer_set_send_critical(m_self, size);
}

/* （对发送数据起作用）设置启用压缩，若要启用压缩，则此函数在创建socket对象后即刻调用*/
void Socketer::UseCompress() {
	socketer_use_compress(m_self);
}

/* （慎用）（对接收的数据起作用）启用解压缩，网络库会负责解压缩操作，仅供客户端使用*/
void Socketer::UseUncompress() {
	socketer_use_uncompress(m_self);
}

/* 设置加密/解密函数， 以及特殊用途的参与加密/解密逻辑的数据。
 * 若加密/解密函数为NULL，则保持默认。
 * */
void Socketer::SetEncryptDecryptFunction(void (*encryptfunc)(void *logicdata, char *buf, int len), void (*release_encrypt_logicdata)(void *), void *encrypt_logicdata, void (*decryptfunc)(void *logicdata, char *buf, int len), void (*release_decrypt_logicdata)(void *), void *decrypt_logicdata) {
	socketer_set_encrypt_function(m_self, encryptfunc, release_encrypt_logicdata, encrypt_logicdata);
	socketer_set_decrypt_function(m_self, decryptfunc, release_decrypt_logicdata, decrypt_logicdata);
}

static void encrypt_decrypt_as_key_do_func(void *logicdata, char *buf, int len) {
	struct encryptinfo *o = (struct encryptinfo *)logicdata;

	int i;
	for (i = 0; i < len; i++) {
		if (o->nowidx >= o->maxidx)
			o->nowidx = 0;

		buf[i] ^= o->buf[o->nowidx];
		o->nowidx++;
	}
}

/* 设置加密key */
void Socketer::SetEncryptKey(const char *key, int key_len) {
	if (!key || key_len < 0)
		return;

	if (!m_encrypt) {
		spin_lock_lock(&s_infomgr.encrypt_lock);
		m_encrypt = (struct encryptinfo *)poolmgr_getobject(s_infomgr.encrypt_pool);
		spin_lock_unlock(&s_infomgr.encrypt_lock);

		if (m_encrypt) {
			m_encrypt->maxidx = 0;
			m_encrypt->nowidx = 0;
			memset(m_encrypt->buf, 0, sizeof(m_encrypt->buf));
			socketer_set_encrypt_function(m_self, encrypt_decrypt_as_key_do_func, encrypt_info_release, m_encrypt);
		}
	}

	if (m_encrypt) {
		m_encrypt->maxidx = key_len > encryptinfo::enum_encrypt_len ? encryptinfo::enum_encrypt_len : key_len;
		memcpy(&m_encrypt->buf, key, m_encrypt->maxidx);
	}
}

/* 设置解密key */
void Socketer::setDecryptKey(const char *key, int key_len) {
	if (!key || key_len < 0)
		return;

	if (!m_decrypt) {
		spin_lock_lock(&s_infomgr.encrypt_lock);
		m_decrypt = (struct encryptinfo *)poolmgr_getobject(s_infomgr.encrypt_pool);
		spin_lock_unlock(&s_infomgr.encrypt_lock);

		if (m_decrypt) {
			m_decrypt->maxidx = 0;
			m_decrypt->nowidx = 0;
			memset(m_decrypt->buf, 0, sizeof(m_decrypt->buf));
			socketer_set_decrypt_function(m_self, encrypt_decrypt_as_key_do_func, encrypt_info_release, m_decrypt);
		}
	}

	if (m_decrypt) {
		m_decrypt->maxidx = key_len > encryptinfo::enum_encrypt_len ? encryptinfo::enum_encrypt_len : key_len;
		memcpy(&m_decrypt->buf, key, m_decrypt->maxidx);
	}
	
}

/* （启用加密）*/
void Socketer::UseEncrypt() {
	socketer_use_encrypt(m_self);
}

/* （启用解密）*/
void Socketer::UseDecrypt() {
	socketer_use_decrypt(m_self);
}

/* 启用TGW接入 */
void Socketer::UseTGW() {
	socketer_use_tgw(m_self);
}

/* 关闭用于连接的socket对象*/
void Socketer::Close() {
	socketer_close(m_self);
}

/* 连接指定的服务器*/
bool Socketer::Connect(const char *ip, short port) {
	return socketer_connect(m_self, ip, port);
}

/* 测试socket套接字是否已关闭*/
bool Socketer::IsClose() {
	return socketer_isclose(m_self);
}

/* 获取此客户端ip地址*/
void Socketer::GetIP(char *ip, size_t len) {
	socketer_getip(m_self, ip, len);
}

/* 发送数据，仅仅是把数据压入包队列中，adddata为附加到pMsg后面的数据，当然会自动修改pMsg的长度，addsize指定adddata的长度*/
bool Socketer::SendMsg(Msg *pMsg, void *adddata, size_t addsize) {
	if (!pMsg)
		return false;
	if (adddata && addsize == 0) {
		assert(false);
		return false;
	}

	if (!adddata && addsize != 0) {
		assert(false);
		return false;
	}

	if (pMsg->GetLength() <= (int)sizeof(int))
		return false;

	if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN) {
		assert(false && "if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN)");
		log_error("	if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN)");
		return false;
	}

	if (socketer_send_islimit(m_self, pMsg->GetLength()+addsize)) {
		Close();
		return false;
	}

	on_sendmsg(pMsg->GetLength() + addsize);

	if (adddata && addsize != 0) {
		bool res1, res2;
		int onesend = pMsg->GetLength();
		pMsg->SetLength(onesend + addsize);
		res1 = socketer_sendmsg(m_self, pMsg, onesend);
		
		//这里切记要修改回去。 例：对于同一个包遍历发送给一个列表，然后每次都附带不同尾巴。。。这种情景，那么必须如此恢复。
		pMsg->SetLength(onesend);
		res2 = socketer_sendmsg(m_self, adddata, addsize);
		return (res1 && res2);
	} else {
		return socketer_sendmsg(m_self, pMsg, pMsg->GetLength());
	}
}

/* 对as3发送策略文件 */
bool Socketer::SendPolicyData() {
	//as3套接字策略文件
	char buf[512] = "<cross-domain-policy> <allow-access-from domain=\"*\" secure=\"false\" to-ports=\"*\"/> </cross-domain-policy> ";
	size_t datasize = strlen(buf);
	if (socketer_send_islimit(m_self, datasize)) {
		Close();
		return false;
	}

	on_sendmsg(datasize + 1);
	return socketer_sendmsg(m_self, buf, datasize + 1);
}

/* 发送TGW信息头 */
bool Socketer::SendTGWInfo(const char *domain, int port) {
	char buf[1024] = {};
	size_t datasize;
	snprintf(buf, sizeof(buf) - 1, "tgw_l7_forward\r\nHost: %s:%d\r\n\r\n", domain, port);
	buf[sizeof(buf) - 1] = '\0';
	datasize = strlen(buf);
	if (socketer_send_islimit(m_self, datasize)) {
		Close();
		return false;
	}
	socketer_set_raw_datasize(m_self, datasize);
	on_sendmsg(datasize);
	return socketer_sendmsg(m_self, buf, datasize);
}

/* 触发真正的发送数据*/
void Socketer::CheckSend() {
	socketer_checksend(m_self);
}

/* 尝试投递接收操作*/
void Socketer::CheckRecv() {
	socketer_checkrecv(m_self);
}

/* 接收数据*/
Msg *Socketer::GetMsg(char *buf, size_t bufsize) {
	Msg *obj = (Msg *)socketer_getmsg(m_self, buf, bufsize);
	if (obj) {
		if (obj->GetLength() < (int)sizeof(Msg)) {
			Close();
			return NULL;
		}
		on_recvmsg(obj->GetLength());
	}
	return obj;
}

/* 发送数据 */
bool Socketer::SendData(const char *data, size_t datasize) {
	if (!data)
		return false;

	if (socketer_send_islimit(m_self, datasize)) {
		Close();
		return false;
	}

	on_sendmsg(datasize);

	return socketer_sendmsg(m_self, (void *)data, datasize);
}

/* 接收数据 */
char *Socketer::GetData(char *buf, size_t bufsize, int *datalen) {
	char *data = (char *)socketer_getdata(m_self, buf, bufsize, datalen);
	if (data) {
		on_recvmsg(*datalen);
	}

	return data;
}


/* 初始化网络，
 * bigbufsize指定大块的大小，bigbufnum指定大块的数目，
 * smallbufsize指定小块的大小，smallbufnum指定小块的数目
 * listen num指定用于监听的套接字的数目，socket num用于连接的总数目
 * threadnum指定网络线程数目，若设置为小于等于0，则会开启cpu个数的线程数目
 */
bool net_init(size_t bigbufsize, size_t bigbufnum, size_t smallbufsize, size_t smallbufnum,
		size_t listenernum, size_t socketnum, int threadnum) {
	if (!infomgr_init(socketnum, listenernum))
		return false;
	if (!netinit(bigbufsize, bigbufnum, smallbufsize, smallbufnum,
			listenernum, socketnum, threadnum)) {
		infomgr_release();
		return false;
	}
	return true;
}

/* 获取此进程所在的机器名*/
const char *GetHostName() {
	static char buf[1024*16];
	buf[0] = '\0';
	socketer_gethostname(buf, sizeof(buf) - 1);
	return buf;
}

/* 根据域名获取ip地址 */
const char *GetHostIPByName(const char *hostname) {
	static char buf[128];
	buf[0] = '\0';
	socketer_gethostbyname(hostname, buf, sizeof(buf));
	return buf;
}

/* 启用/禁用接受的连接导致的错误日志，并返回之前的值 */
bool SetEnableErrorLog(bool flag) {
	return buf_set_enable_errorlog(flag);
}

/* 获取当前启用或禁用接受的连接导致的错误日志 */
bool GetEnableErrorLog() {
	return buf_get_enable_errorlog();
}

/* 创建一个用于监听的对象*/
Listener *Listener_create() {
	if (!s_infomgr.isinit) {
		assert(false && "Listener_create not init!");
		return NULL;
	}
	struct listener *ls = listener_create();
	if (!ls)
		return NULL;
	spin_lock_lock(&s_infomgr.listen_lock);
	Listener *self = (Listener *)poolmgr_getobject(s_infomgr.listen_pool);
	spin_lock_unlock(&s_infomgr.listen_lock);
	if (!self) {
		listener_release(ls);
		return NULL;
	}
	self->m_self = ls;
	return self;
}

/* 释放一个用于监听的对象*/
void Listener_release(Listener *self) {
	if (!self)
		return;
	if (self->m_self) {
		listener_release(self->m_self);
		self->m_self = NULL;
	}
	spin_lock_lock(&s_infomgr.listen_lock);
	poolmgr_freeobject(s_infomgr.listen_pool, self);
	spin_lock_unlock(&s_infomgr.listen_lock);
}

/* 创建一个Socketer对象*/
Socketer *Socketer_create(bool bigbuf) {
	if (!s_infomgr.isinit) {
		assert(false && "Csocket_create not init!");
		return NULL;
	}
	struct socketer *so = socketer_create(bigbuf);
	if (!so)
		return NULL;
	spin_lock_lock(&s_infomgr.socket_lock);
	Socketer *self = (Socketer *)poolmgr_getobject(s_infomgr.socket_pool);
	spin_lock_unlock(&s_infomgr.socket_lock);
	if (!self) {
		socketer_release(so);
		return NULL;
	}

	self->m_encrypt = NULL;
	self->m_decrypt = NULL;
	self->m_self = so;
	return self;
}

/* 释放Socketer对象，会自动调用关闭等善后操作*/
void Socketer_release(Socketer *self) {
	if (!self)
		return;
	
	if (self->m_self) {
		socketer_release(self->m_self);
		self->m_self = NULL;
	}

	self->m_encrypt = NULL;
	self->m_decrypt = NULL;

	spin_lock_lock(&s_infomgr.socket_lock);
	poolmgr_freeobject(s_infomgr.socket_pool, self);
	spin_lock_unlock(&s_infomgr.socket_lock);
}

/* 释放网络相关*/
void net_release() {
	infomgr_release();
	netrelease();
}

/* 执行相关操作，需要在主逻辑中调用此函数*/
void net_run() {
	datarun();
	netrun();
}

static char s_memory_info[1024*64];

/* 获取socket对象池，listen对象池，大块池，小块池的使用情况*/
const char *net_memory_info() {
	size_t index = 0;

	snprintf(&s_memory_info[index], sizeof(s_memory_info)-1-index, "%s", "lxnet lib memory pool info:\n<+++++++++++++++++++++++++++++++++++++++++++++++++++++>");
	index = strlen(s_memory_info);

	spin_lock_lock(&s_infomgr.encrypt_lock);
	poolmgr_getinfo(s_infomgr.encrypt_pool, &s_memory_info[index], sizeof(s_memory_info)-1-index);
	spin_lock_unlock(&s_infomgr.encrypt_lock);

	index = strlen(s_memory_info);

	spin_lock_lock(&s_infomgr.socket_lock);
	poolmgr_getinfo(s_infomgr.socket_pool, &s_memory_info[index], sizeof(s_memory_info)-1-index);
	spin_lock_unlock(&s_infomgr.socket_lock);
	
	index = strlen(s_memory_info);

	spin_lock_lock(&s_infomgr.listen_lock);
	poolmgr_getinfo(s_infomgr.listen_pool, &s_memory_info[index], sizeof(s_memory_info)-1-index);
	spin_lock_unlock(&s_infomgr.listen_lock);

	index = strlen(s_memory_info);
	netmemory_info(&s_memory_info[index], sizeof(s_memory_info)-1-index);

	index = strlen(s_memory_info);
	snprintf(&s_memory_info[index], sizeof(s_memory_info)-1-index, "%s", "<+++++++++++++++++++++++++++++++++++++++++++++++++++++>");
	return s_memory_info;
}

//获取当前时间。格式为"2010-09-16 23:20:20"
static const char *CurrentTimeStr(time_t tval, char *buf, size_t buflen) {
	if (buflen < 64)
		return "null";

	struct tm tm_result;
	struct tm *currTM = safe_localtime(&tval, &tm_result);
	snprintf(buf, buflen-1, "%d-%02d-%02d %02d:%02d:%02d", currTM->tm_year+1900, currTM->tm_mon+1, currTM->tm_mday, currTM->tm_hour, currTM->tm_min, currTM->tm_sec);
	buf[buflen-1] = '\0';
	return buf;
}

/* 获取网络库通讯详情*/
const char *net_datainfo() {
	static char infostr[1024*8];
	if (!s_datamgr.isinit)
		return "not init!";

	double numunit = 1000*1000;
	double bytesunit = 1024*1024;
	double totalsendmsgnum = (double)(s_datamgr.datatable[enum_netdata_total].sendmsgnum / numunit);
	double totalsendbytes = double(s_datamgr.datatable[enum_netdata_total].sendbytes / bytesunit);
	double totalrecvmsgnum = (double)(s_datamgr.datatable[enum_netdata_total].recvmsgnum / numunit);
	double totalrecvbytes = double(s_datamgr.datatable[enum_netdata_total].recvbytes / bytesunit);

	double maxsendmsgnum = (double)(s_datamgr.datatable[enum_netdata_max].sendmsgnum);
	double maxsendbytes = double(s_datamgr.datatable[enum_netdata_max].sendbytes / bytesunit);
	double maxrecvmsgnum = (double)(s_datamgr.datatable[enum_netdata_max].recvmsgnum);
	double maxrecvbytes = double(s_datamgr.datatable[enum_netdata_max].recvbytes / bytesunit);

	double nowsendmsgnum = (double)(s_datamgr.datatable[enum_netdata_now].sendmsgnum);
	double nowsendbytes = double(s_datamgr.datatable[enum_netdata_now].sendbytes / bytesunit);
	double nowrecvmsgnum = (double)(s_datamgr.datatable[enum_netdata_now].recvmsgnum);
	double nowrecvbytes = double(s_datamgr.datatable[enum_netdata_now].recvbytes / bytesunit);

	char buf_sendmsgnum[128] = {};
	char buf_sendbytes[128] = {};
	char buf_recvmsgnum[128] = {};
	char buf_recvbytes[128] = {};
	CurrentTimeStr(s_datamgr.datatable[enum_netdata_max].tm_sendmsgnum, buf_sendmsgnum, sizeof(buf_sendmsgnum));
	CurrentTimeStr(s_datamgr.datatable[enum_netdata_max].tm_sendbytes, buf_sendbytes, sizeof(buf_sendbytes));
	CurrentTimeStr(s_datamgr.datatable[enum_netdata_max].tm_recvmsgnum, buf_recvmsgnum, sizeof(buf_recvmsgnum));
	CurrentTimeStr(s_datamgr.datatable[enum_netdata_max].tm_recvbytes, buf_recvbytes, sizeof(buf_recvbytes));

	snprintf(infostr, sizeof(infostr) - 1, "total: send msg num:%lfM, send bytes:%lfMB, recv msg num:%lfM, recv bytes:%lfMB\nmax:\nsend msg num:%lf, time:%s\nsend bytes:%lfMB, time:%s\nrecv msg num:%lf, time:%s\nrecv bytes:%lfMB, time:%s\nnow: send msg num:%lf, send bytes:%lfMB, recv msg num:%lf, recv bytes:%lfMB\n", totalsendmsgnum, totalsendbytes, totalrecvmsgnum, totalrecvbytes, maxsendmsgnum, buf_sendmsgnum, maxsendbytes, buf_sendbytes, maxrecvmsgnum, buf_recvmsgnum, maxrecvbytes, buf_recvbytes, nowsendmsgnum, nowsendbytes, nowrecvmsgnum, nowrecvbytes);

	return infostr;
}

/* 获取网络库的指定类型的详情*/
struct datainfo *net_datainfo_bytype(int type) {
	if (type < 0 || type >= (int)enum_netdata_end)
		return NULL;

	return &s_datamgr.datatable[type];
}

}


