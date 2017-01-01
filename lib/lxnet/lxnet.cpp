
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cthread.h"
#include "net_module.h"
#include "lxnet.h"
#include "net_buf.h"
#include "pool.h"
#include "msgbase.h"
#include "log.h"
#include "crosslib.h"
#include "lxnet_datainfo.h"



struct infomgr {
	bool is_init;
	struct poolmgr *encrypt_pool;
	cspin encrypt_lock;

	struct poolmgr *socket_pool;
	cspin socket_lock;

	struct poolmgr *listen_pool;
	cspin listen_lock;
};

static struct infomgr s_infomgr = {false};
static struct datainfomgr *s_datainfomgr = NULL;
static bool s_datainfo_need_release = false;

struct encrypt_info {
	enum {
		enum_encrypt_len = 32,
	};

	int maxidx;
	int nowidx;
	char buf[enum_encrypt_len];
};

static inline void on_send_msg(struct datainfomgr *infomgr, size_t msg_num, size_t len) {
	if (infomgr) {
		infomgr->data_table[enum_netdata_total].send_msg_num += (int64)msg_num;
		infomgr->data_table[enum_netdata_total].send_bytes += (int64)len;

		infomgr->data_table[enum_netdata_now].send_msg_num += (int64)msg_num;
		infomgr->data_table[enum_netdata_now].send_bytes += (int64)len;
	}
}

static inline void on_recv_msg(struct datainfomgr *infomgr, size_t msg_num, size_t len) {
	if (infomgr) {
		infomgr->data_table[enum_netdata_total].recv_msg_num += (int64)msg_num;
		infomgr->data_table[enum_netdata_total].recv_bytes += (int64)len;

		infomgr->data_table[enum_netdata_now].recv_msg_num += (int64)msg_num;
		infomgr->data_table[enum_netdata_now].recv_bytes += (int64)len;
	}
}

static bool infomgr_init(size_t socketer_num, size_t listener_num) {
	if (s_infomgr.is_init)
		return false;

	s_infomgr.encrypt_pool = poolmgr_create(sizeof(struct encrypt_info), 8, socketer_num * 2, 1, 
																		"encrypt buffer pool");
	s_infomgr.socket_pool = poolmgr_create(sizeof(lxnet::Socketer), 8, socketer_num, 1, 
																	"Socketer obj pool");
	s_infomgr.listen_pool = poolmgr_create(sizeof(lxnet::Listener), 8, listener_num, 1, 
																	"Listen obj pool");
	if (!s_infomgr.socket_pool || !s_infomgr.encrypt_pool || !s_infomgr.listen_pool) {
		poolmgr_release(s_infomgr.socket_pool);
		poolmgr_release(s_infomgr.encrypt_pool);
		poolmgr_release(s_infomgr.listen_pool);
		return false;
	}

	cspin_init(&s_infomgr.encrypt_lock);
	cspin_init(&s_infomgr.socket_lock);
	cspin_init(&s_infomgr.listen_lock);
	s_infomgr.is_init = true;
	return true;
}

static void infomgr_release() {
	if (!s_infomgr.is_init)
		return;

	s_infomgr.is_init = false;
	poolmgr_release(s_infomgr.socket_pool);
	poolmgr_release(s_infomgr.encrypt_pool);
	poolmgr_release(s_infomgr.listen_pool);
	cspin_destroy(&s_infomgr.encrypt_lock);
	cspin_destroy(&s_infomgr.socket_lock);
	cspin_destroy(&s_infomgr.listen_lock);
}

static void encrypt_info_release(void *info) {
	if (!s_infomgr.is_init)
		return;

	cspin_lock(&s_infomgr.encrypt_lock);
	poolmgr_free_object(s_infomgr.encrypt_pool, info);
	cspin_unlock(&s_infomgr.encrypt_lock);
}



namespace lxnet {

/* 创建一个用于监听的对象 */
Listener *Listener::Create() {
	if (!s_infomgr.is_init) {
		assert(false && "Listener Create not init!");
		return NULL;
	}

	struct listener *ls = listener_create();
	if (!ls)
		return NULL;

	cspin_lock(&s_infomgr.listen_lock);
	Listener *self = (Listener *)poolmgr_alloc_object(s_infomgr.listen_pool);
	cspin_unlock(&s_infomgr.listen_lock);
	if (!self) {
		listener_release(ls);
		return NULL;
	}

	self->m_self = ls;
	return self;
}

/* 释放一个用于监听的对象 */
void Listener::Release(Listener *self) {
	if (!self)
		return;

	if (self->m_self) {
		listener_release(self->m_self);
		self->m_self = NULL;
	}

	cspin_lock(&s_infomgr.listen_lock);
	poolmgr_free_object(s_infomgr.listen_pool, self);
	cspin_unlock(&s_infomgr.listen_lock);
}

/* 监听 */
bool Listener::Listen(unsigned short port, int backlog) {
	return listener_listen(m_self, port, backlog);
}

/* 关闭用于监听的套接字，停止监听 */
void Listener::Close() {
	listener_close(m_self);
}

/* 测试是否已关闭 */
bool Listener::IsClose() {
	return listener_is_close(m_self);
}

/* 在指定的监听socket上接受连接 */
Socketer *Listener::Accept(bool bigbuf) {
	struct socketer *sock = listener_accept(m_self, bigbuf);
	if (!sock)
		return NULL;

	cspin_lock(&s_infomgr.socket_lock);
	Socketer *self = (Socketer *)poolmgr_alloc_object(s_infomgr.socket_pool);
	cspin_unlock(&s_infomgr.socket_lock);
	if (!self) {
		socketer_release(sock);
		return NULL;
	}

	self->m_infomgr = s_datainfomgr;
	self->m_encrypt = NULL;
	self->m_decrypt = NULL;
	self->m_self = sock;
	return self;
}

/* 检测是否有新的连接 */
bool Listener::CanAccept() {
	return listener_can_accept(m_self);
}



/* 创建一个Socketer对象 */
Socketer *Socketer::Create(bool bigbuf) {
	if (!s_infomgr.is_init) {
		assert(false && "Socketer Create not init!");
		return NULL;
	}

	struct socketer *so = socketer_create(bigbuf);
	if (!so)
		return NULL;

	cspin_lock(&s_infomgr.socket_lock);
	Socketer *self = (Socketer *)poolmgr_alloc_object(s_infomgr.socket_pool);
	cspin_unlock(&s_infomgr.socket_lock);
	if (!self) {
		socketer_release(so);
		return NULL;
	}

	self->m_infomgr = s_datainfomgr;
	self->m_encrypt = NULL;
	self->m_decrypt = NULL;
	self->m_self = so;
	return self;
}

/* 释放Socketer对象，会自动调用关闭等善后操作 */
void Socketer::Release(Socketer *self) {
	if (!self)
		return;

	if (self->m_self) {
		socketer_release(self->m_self);
		self->m_self = NULL;
	}

	self->m_infomgr = NULL;
	self->m_encrypt = NULL;
	self->m_decrypt = NULL;

	cspin_lock(&s_infomgr.socket_lock);
	poolmgr_free_object(s_infomgr.socket_pool, self);
	cspin_unlock(&s_infomgr.socket_lock);
}

/* 设置关联的统计对象 */
void Socketer::SetDataInfoMgr(struct datainfomgr *infomgr) {
	assert(infomgr != NULL);
	m_infomgr = infomgr;
}

/* 设置接收数据字节的临界值，超过此值，则停止接收，若小于等于0，则视为不限制 */
void Socketer::SetRecvLimit(int size) {
	socketer_set_recv_limit(m_self, size);
}

/* 设置发送数据字节的临界值，若缓冲中数据长度大于此值，则断开此连接，若为0，则视为不限制 */
void Socketer::SetSendLimit(int size) {
	socketer_set_send_limit(m_self, size);
}

/* (对发送数据起作用)设置启用压缩，若要启用压缩，则此函数在创建socket对象后即刻调用 */
void Socketer::UseCompress() {
	socketer_use_compress(m_self);
}

/* (慎用)(对接收的数据起作用)启用解压缩，网络库会负责解压缩操作，仅供客户端使用 */
void Socketer::UseUncompress() {
	socketer_use_uncompress(m_self);
}

/*
 * 设置加密/解密函数， 以及特殊用途的参与加密/解密逻辑的数据。
 * 若加密/解密函数为NULL，则保持默认。
 */
void Socketer::SetEncryptDecryptFunction(void (*encryptfunc)(void *logicdata, char *buf, int len), void (*release_encrypt_logicdata)(void *), void *encrypt_logicdata, void (*decryptfunc)(void *logicdata, char *buf, int len), void (*release_decrypt_logicdata)(void *), void *decrypt_logicdata) {
	socketer_set_encrypt_function(m_self, encryptfunc, release_encrypt_logicdata, encrypt_logicdata);
	socketer_set_decrypt_function(m_self, decryptfunc, release_decrypt_logicdata, decrypt_logicdata);
}

static void encrypt_decrypt_as_key_do_func(void *logicdata, char *buf, int len) {
	struct encrypt_info *o = (struct encrypt_info *)logicdata;

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
		cspin_lock(&s_infomgr.encrypt_lock);
		m_encrypt = (struct encrypt_info *)poolmgr_alloc_object(s_infomgr.encrypt_pool);
		cspin_unlock(&s_infomgr.encrypt_lock);

		if (m_encrypt) {
			m_encrypt->maxidx = 0;
			m_encrypt->nowidx = 0;
			memset(m_encrypt->buf, 0, sizeof(m_encrypt->buf));
			socketer_set_encrypt_function(m_self, encrypt_decrypt_as_key_do_func, encrypt_info_release, m_encrypt);
		}
	}

	if (m_encrypt) {
		m_encrypt->maxidx = key_len > encrypt_info::enum_encrypt_len ? encrypt_info::enum_encrypt_len : key_len;
		memcpy(&m_encrypt->buf, key, m_encrypt->maxidx);
	}
}

/* 设置解密key */
void Socketer::SetDecryptKey(const char *key, int key_len) {
	if (!key || key_len < 0)
		return;

	if (!m_decrypt) {
		cspin_lock(&s_infomgr.encrypt_lock);
		m_decrypt = (struct encrypt_info *)poolmgr_alloc_object(s_infomgr.encrypt_pool);
		cspin_unlock(&s_infomgr.encrypt_lock);

		if (m_decrypt) {
			m_decrypt->maxidx = 0;
			m_decrypt->nowidx = 0;
			memset(m_decrypt->buf, 0, sizeof(m_decrypt->buf));
			socketer_set_decrypt_function(m_self, encrypt_decrypt_as_key_do_func, encrypt_info_release, m_decrypt);
		}
	}

	if (m_decrypt) {
		m_decrypt->maxidx = key_len > encrypt_info::enum_encrypt_len ? encrypt_info::enum_encrypt_len : key_len;
		memcpy(&m_decrypt->buf, key, m_decrypt->maxidx);
	}
}

/* (启用加密) */
void Socketer::UseEncrypt() {
	socketer_use_encrypt(m_self);
}

/* (启用解密) */
void Socketer::UseDecrypt() {
	socketer_use_decrypt(m_self);
}

/* 启用TGW接入 */
void Socketer::UseTGW() {
	socketer_use_tgw(m_self);
}

/* 连接指定的服务器 */
bool Socketer::Connect(const char *ip, short port) {
	return socketer_connect(m_self, ip, port);
}

/* 关闭用于连接的socket对象 */
void Socketer::Close() {
	socketer_close(m_self);
}

/* 测试socket套接字是否已关闭 */
bool Socketer::IsClose() {
	return socketer_is_close(m_self);
}

/* 获取此客户端ip地址 */
void Socketer::GetIP(char *ip, size_t len) {
	socketer_get_ip(m_self, ip, len);
}

/* 获取发送缓冲待发送字节数(若为0表示不存在待发送数据或数据已写入系统缓冲) */
int Socketer::GetSendBufferByteSize() {
	return socketer_get_send_buffer_byte_size(m_self);
}

/* 获取接收缓冲中待读取的字节数(若为0表示目前无数据可读) */
int Socketer::GetRecvBufferByteSize() {
	return socketer_get_recv_buffer_byte_size(m_self);
}

/* 对as3发送策略文件 */
bool Socketer::SendPolicyData() {
	//as3套接字策略文件
	char buf[512] = "<cross-domain-policy> <allow-access-from domain=\"*\" secure=\"false\" to-ports=\"*\"/> </cross-domain-policy> ";
	size_t datasize = strlen(buf);
	if (socketer_send_is_limit(m_self, datasize)) {
		Close();
		return false;
	}

	bool res = socketer_send_msg(m_self, buf, datasize + 1);
	if (res) {
		on_send_msg(m_infomgr, 0, datasize + 1);
	}
	return res;
}

/* 发送TGW信息头 */
bool Socketer::SendTGWInfo(const char *domain, int port) {
	char buf[1024] = {0};
	size_t datasize;
	snprintf(buf, sizeof(buf) - 1, "tgw_l7_forward\r\nHost: %s:%d\r\n\r\n", domain, port);
	buf[sizeof(buf) - 1] = '\0';
	datasize = strlen(buf);
	if (socketer_send_is_limit(m_self, datasize)) {
		Close();
		return false;
	}

	socketer_set_raw_datasize(m_self, datasize);
	bool res = socketer_send_msg(m_self, buf, datasize);
	if (res) {
		on_send_msg(m_infomgr, 0, datasize);
	}
	return res;
}

/*
 * 发送数据，仅仅是把数据压入包队列中，
 * adddata为附加到pMsg后面的数据，当然会自动修改pMsg的长度，addsize指定adddata的长度
 */
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

	if (pMsg->GetLength() < (int)sizeof(Msg))
		return false;

	if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN) {
		assert(false && "if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN)");
		log_error("	if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN)");
		return false;
	}

	if (socketer_send_is_limit(m_self, pMsg->GetLength() + addsize)) {
		Close();
		return false;
	}

	bool res = false;
	if (adddata && addsize != 0) {
		bool res1, res2;
		int onesend = pMsg->GetLength();
		pMsg->SetLength(onesend + addsize);
		res1 = socketer_send_msg(m_self, pMsg, onesend);

		/*
		 * 这里切记要修改回去。
		 * 例：对于同一个包遍历发送给一个列表，然后每次都附带不同尾巴。。。这种情景，那么必须如此恢复。
		 */
		pMsg->SetLength(onesend);
		res2 = socketer_send_msg(m_self, adddata, addsize);
		res = (res1 && res2);
	} else {
		res = socketer_send_msg(m_self, pMsg, pMsg->GetLength());
	}

	if (res) {
		on_send_msg(m_infomgr, 1, pMsg->GetLength() + addsize);
	}
	return res;
}

/* 接收数据 */
Msg *Socketer::GetMsg(char *buf, size_t bufsize) {
	Msg *pMsg = (Msg *)socketer_get_msg(m_self, buf, bufsize);
	if (pMsg) {
		if (pMsg->GetLength() < (int)sizeof(Msg)) {
			Close();
			return NULL;
		}

		on_recv_msg(m_infomgr, 1, pMsg->GetLength());
	}
	return pMsg;
}

/* 发送数据 */
bool Socketer::SendData(const void *data, size_t datasize) {
	if (!data)
		return false;

	if (socketer_send_is_limit(m_self, datasize)) {
		Close();
		return false;
	}

	bool res = socketer_send_data(m_self, (void *)data, datasize);
	if (res) {
		on_send_msg(m_infomgr, 0, datasize);
	}
	return res;
}

/* 接收数据 */
const void *Socketer::GetData(char *buf, size_t bufsize, int *datalen) {
	const void *data = socketer_get_data(m_self, buf, bufsize, datalen);
	if (data) {
		on_recv_msg(m_infomgr, 0, *datalen);
	}
	return data;
}

/* 触发真正的发送数据 */
void Socketer::CheckSend() {
	socketer_check_send(m_self);
}

/* 尝试投递接收操作 */
void Socketer::CheckRecv() {
	socketer_check_recv(m_self);
}



/*
 * 初始化网络
 * big_buf_size 指定大块的大小，big_buf_num 指定大块的数目，
 * small_buf_size 指定小块的大小，small_buf_num 指定小块的数目
 * listener_num 指定用于监听的对象的数目，socketer_num 指定用于连接的对象的数目
 * thread_num 指定网络线程数目，若设置为小于等于0，则会开启cpu个数的线程数目
 * infomgr 默认的网络数据统计管理器，一般为NULL
 */
bool net_init(size_t big_buf_size, size_t big_buf_num, size_t small_buf_size, size_t small_buf_num, 
		size_t listener_num, size_t socketer_num, int thread_num, struct datainfomgr *infomgr) {

	if (!infomgr_init(socketer_num, listener_num))
		return false;

	if (!net_module_init(big_buf_size, big_buf_num, small_buf_size, small_buf_num, 
				listener_num, socketer_num, thread_num)) {
		infomgr_release();
		return false;
	}

	bool need_release = false;
	if (!infomgr) {
		need_release = true;
		infomgr = DataInfoMgr_CreateObj();
	}

	if (!infomgr)
		return false;

	s_datainfomgr = infomgr;
	s_datainfo_need_release = need_release;

	return true;
}

/* 释放网络相关 */
void net_release() {
	infomgr_release();
	net_module_release();

	if (s_datainfo_need_release)
		DataInfoMgr_ReleaseObj(s_datainfomgr);

	s_datainfomgr = NULL;
	s_datainfo_need_release = false;
}

/* 执行相关操作，需要在主逻辑中调用此函数 */
void net_run() {
	net_module_run();
	DataInfoMgr_Run(s_datainfomgr);
}

/* 获取socket对象池，listen对象池，大块池，小块池的使用情况 */
const char *net_get_memory_info(char *buf, size_t buflen) {
	if (!buf || buflen < 8000)
		return NULL;

	size_t index = 0;

	snprintf(&buf[index], buflen - 1 - index, "%s", 
			"lxnet lib memory pool info:\n<+++++++++++++++++++++++++++++++++++++++++++++++++++++>");
	index = strlen(buf);

	cspin_lock(&s_infomgr.encrypt_lock);
	poolmgr_get_info(s_infomgr.encrypt_pool, &buf[index], buflen - 1 - index);
	cspin_unlock(&s_infomgr.encrypt_lock);

	index = strlen(buf);

	cspin_lock(&s_infomgr.socket_lock);
	poolmgr_get_info(s_infomgr.socket_pool, &buf[index], buflen - 1 - index);
	cspin_unlock(&s_infomgr.socket_lock);

	index = strlen(buf);

	cspin_lock(&s_infomgr.listen_lock);
	poolmgr_get_info(s_infomgr.listen_pool, &buf[index], buflen - 1 - index);
	cspin_unlock(&s_infomgr.listen_lock);

	index = strlen(buf);
	net_module_get_memory_info(&buf[index], buflen - 1 - index);

	index = strlen(buf);

	if (buf[index - 1] != '\n') {
		buf[index] = '\n';
		index++;
	}

	snprintf(&buf[index], buflen - 1 - index, "%s", 
			"<+++++++++++++++++++++++++++++++++++++++++++++++++++++>");

	buf[buflen - 1] = '\0';
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


/* 获取此进程所在的机器名 */
bool GetHostName(char *buf, size_t buflen) {
	return socketer_get_hostname(buf, buflen);
}

/* 根据域名获取ip地址 */
bool GetHostIPByName(const char *hostname, char *buf, size_t buflen, bool ipv6) {
	return socketer_get_host_ip_by_name(hostname, buf, buflen, ipv6);
}



/* 创建网络数据统计管理器 */
struct datainfomgr *DataInfoMgr_CreateObj() {
	struct datainfomgr *infomgr = (struct datainfomgr *)malloc(sizeof(struct datainfomgr));
	if (!infomgr)
		return NULL;

	memset(infomgr, 0, sizeof(*infomgr));

	time_t curtm = time(NULL);
	for (int i = 0; i < enum_netdata_end; ++i) {
		infomgr->data_table[i].tm_send_msg_num = curtm;
		infomgr->data_table[i].tm_recv_msg_num = curtm;
		infomgr->data_table[i].tm_send_bytes = curtm;
		infomgr->data_table[i].tm_recv_bytes = curtm;
	}

	return infomgr;
}

/* 释放指定网络数据统计管理器 */
void DataInfoMgr_ReleaseObj(struct datainfomgr *infomgr) {
	if (!infomgr)
		return;

	free(infomgr);
}

/* 执行网络数据统计相关操作 */
void DataInfoMgr_Run(struct datainfomgr *infomgr) {
	if (!infomgr)
		return;

	int64 currenttime = get_millisecond();
	if (currenttime - infomgr->last_time < 1000)
		return;

	infomgr->last_time = currenttime;

	time_t curtm = time(NULL);
	struct datainfo *max_info = &infomgr->data_table[enum_netdata_max];
	struct datainfo *now_info = &infomgr->data_table[enum_netdata_now];

	if (max_info->send_msg_num < now_info->send_msg_num) {
		max_info->send_msg_num = now_info->send_msg_num;
		max_info->tm_send_msg_num = curtm;
	}

	if (max_info->recv_msg_num < now_info->recv_msg_num) {
		max_info->recv_msg_num = now_info->recv_msg_num;
		max_info->tm_recv_msg_num = curtm;
	}

	if (max_info->send_bytes < now_info->send_bytes) {
		max_info->send_bytes = now_info->send_bytes;
		max_info->tm_send_bytes = curtm;
	}

	if (max_info->recv_bytes < now_info->recv_bytes) {
		max_info->recv_bytes = now_info->recv_bytes;
		max_info->tm_recv_bytes = curtm;
	}

	now_info->send_msg_num = 0;
	now_info->recv_msg_num = 0;
	now_info->send_bytes = 0;
	now_info->recv_bytes = 0;
}

//获取当前时间。格式为"2010-09-16 23:20:20"
static const char *get_current_time_str(time_t tval, char *buf, size_t buflen) {
	if (buflen < 32)
		return "null";

	struct tm tm_result;
	struct tm *now = safe_localtime(&tval, &tm_result);
	snprintf(buf, buflen - 1, "%d-%02d-%02d %02d:%02d:%02d", 
				now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, 
				now->tm_hour, now->tm_min, now->tm_sec);
	buf[buflen - 1] = '\0';
	return buf;
}

/* 获取网络数据统计信息 */
const char *GetNetDataAllInfo(char *buf, size_t buflen, struct datainfomgr *infomgr) {
	if (!buf || buflen < 4000)
		return NULL;

	if (!infomgr)
		infomgr = s_datainfomgr;

	if (!infomgr)
		return NULL;

	struct datainfo *totalinfo = &infomgr->data_table[enum_netdata_total];
	struct datainfo *max_info = &infomgr->data_table[enum_netdata_max];
	struct datainfo *now_info = &infomgr->data_table[enum_netdata_now];

	double num_unit = 1000 * 1000;
	double bytes_unit = 1024 * 1024;
	double total_send_msg_num = double(totalinfo->send_msg_num / num_unit);
	double total_send_bytes = double(totalinfo->send_bytes / bytes_unit);
	double total_recv_msg_num = double(totalinfo->recv_msg_num / num_unit);
	double total_recv_bytes = double(totalinfo->recv_bytes / bytes_unit);

	double max_send_msg_num = (double)max_info->send_msg_num;
	double max_send_bytes = (double)max_info->send_bytes / bytes_unit;
	double max_recv_msg_num = (double)max_info->recv_msg_num;
	double max_recv_bytes = (double)max_info->recv_bytes / bytes_unit;

	double now_send_msg_num = (double)now_info->send_msg_num;
	double now_send_bytes = (double)now_info->send_bytes / bytes_unit;
	double now_recv_msg_num = (double)now_info->recv_msg_num;
	double now_recv_bytes = (double)now_info->recv_bytes / bytes_unit;

	char buf_send_msg_num[128] = {0};
	char buf_send_bytes[128] = {0};
	char buf_recv_msg_num[128] = {0};
	char buf_recv_bytes[128] = {0};
	get_current_time_str(max_info->tm_send_msg_num, buf_send_msg_num, sizeof(buf_send_msg_num));
	get_current_time_str(max_info->tm_send_bytes, buf_send_bytes, sizeof(buf_send_bytes));
	get_current_time_str(max_info->tm_recv_msg_num, buf_recv_msg_num, sizeof(buf_recv_msg_num));
	get_current_time_str(max_info->tm_recv_bytes, buf_recv_bytes, sizeof(buf_recv_bytes));

	snprintf(buf, buflen - 1, 
			"total:\n"
				"\tsend msg num:%.6fM, send bytes:%.6fMB\n"
				"\trecv msg num:%.6fM, recv bytes:%.6fMB\n"
			"max:\n"
				"\tsend msg num:%.0f, time:%s\n\tsend bytes:%.6fMB, time:%s\n"
				"\trecv msg num:%.0f, time:%s\n\trecv bytes:%.6fMB, time:%s\n"
			"now:\n"
				"\tsend msg num:%.0f, send bytes:%.6fMB\n"
				"\trecv msg num:%.0f, recv bytes:%.6fMB\n", 
			total_send_msg_num, total_send_bytes, total_recv_msg_num, total_recv_bytes, 
			max_send_msg_num, buf_send_msg_num, max_send_bytes, buf_send_bytes, 
			max_recv_msg_num, buf_recv_msg_num, max_recv_bytes, buf_recv_bytes, 
			now_send_msg_num, now_send_bytes, now_recv_msg_num, now_recv_bytes);

	buf[buflen - 1] = '\0';
	return buf;
}

}

