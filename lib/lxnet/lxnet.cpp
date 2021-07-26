
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
#include "crosslib.h"
#include "lxnet_datainfo.h"



struct infomgr {
	bool is_init;
	struct poolmgr *encrypt_pool;
	cspin encrypt_lock;

	struct poolmgr *proxy_pool;
	cspin proxy_lock;

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

	int max_idx;
	int now_idx;
	char buf[enum_encrypt_len];
};

struct proxy_info {
	enum {
		enum_proxy_end_char_len = 8,
		enum_proxy_buff_len = 128,
	};

	char proxy_end_char[enum_proxy_end_char_len];
	char proxy_buff[enum_proxy_buff_len];
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
	s_infomgr.proxy_pool = poolmgr_create(sizeof(struct proxy_info), 8, socketer_num, 1, 
																		"proxy info pool");
	s_infomgr.socket_pool = poolmgr_create(sizeof(lxnet::Socketer), 8, socketer_num, 1, 
																	"Socketer object pool");
	s_infomgr.listen_pool = poolmgr_create(sizeof(lxnet::Listener), 8, listener_num, 1, 
																	"Listen object pool");
	if (!s_infomgr.encrypt_pool || !s_infomgr.proxy_pool || 
			!s_infomgr.listen_pool || !s_infomgr.socket_pool) {
		poolmgr_release(s_infomgr.socket_pool);
		poolmgr_release(s_infomgr.listen_pool);

		poolmgr_release(s_infomgr.encrypt_pool);
		poolmgr_release(s_infomgr.proxy_pool);
		return false;
	}

	cspin_init(&s_infomgr.encrypt_lock);
	cspin_init(&s_infomgr.proxy_lock);
	cspin_init(&s_infomgr.socket_lock);
	cspin_init(&s_infomgr.listen_lock);
	s_infomgr.is_init = true;
	return true;
}

static void infomgr_release() {
	if (!s_infomgr.is_init)
		return;

	poolmgr_release(s_infomgr.socket_pool);
	poolmgr_release(s_infomgr.listen_pool);
	poolmgr_release(s_infomgr.encrypt_pool);
	poolmgr_release(s_infomgr.proxy_pool);
	cspin_destroy(&s_infomgr.socket_lock);
	cspin_destroy(&s_infomgr.listen_lock);
	cspin_destroy(&s_infomgr.encrypt_lock);
	cspin_destroy(&s_infomgr.proxy_lock);

	s_infomgr.is_init = false;
}

struct encrypt_info *encrypt_info_create() {
	struct encrypt_info *info = NULL;
	cspin_lock(&s_infomgr.encrypt_lock);
	info = (struct encrypt_info *)poolmgr_alloc_object(s_infomgr.encrypt_pool);
	cspin_unlock(&s_infomgr.encrypt_lock);

	if (info) {
		info->max_idx = 0;
		info->now_idx = 0;
		memset(info->buf, 0, sizeof(info->buf));
	}
	return info;
}

static void encrypt_info_release(void *info) {
	if (!s_infomgr.is_init)
		return;

	cspin_lock(&s_infomgr.encrypt_lock);
	poolmgr_free_object(s_infomgr.encrypt_pool, info);
	cspin_unlock(&s_infomgr.encrypt_lock);
}

struct proxy_info *proxy_info_create() {
	struct proxy_info *info = NULL;
	cspin_lock(&s_infomgr.proxy_lock);
	info = (struct proxy_info *)poolmgr_alloc_object(s_infomgr.proxy_pool);
	cspin_unlock(&s_infomgr.proxy_lock);

	if (info) {
		memset(info, 0, sizeof(*info));
	}
	return info;
}

static void proxy_info_release(struct proxy_info *info) {
	if (!s_infomgr.is_init)
		return;

	cspin_lock(&s_infomgr.proxy_lock);
	poolmgr_free_object(s_infomgr.proxy_pool, info);
	cspin_unlock(&s_infomgr.proxy_lock);
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
	self->m_proxy = NULL;
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
	self->m_proxy = NULL;
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

	if (self->m_proxy) {
		proxy_info_release(self->m_proxy);
		self->m_proxy = NULL;
	}

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
void Socketer::SetEncryptDecryptFunction(void (*encryptfunc)(void *logicdata, char *buf, int len), 
									void (*release_encrypt_logicdata)(void *), void *encrypt_logicdata, 
									void (*decryptfunc)(void *logicdata, char *buf, int len), 
									void (*release_decrypt_logicdata)(void *), void *decrypt_logicdata) {

	socketer_set_encrypt_function(m_self, encryptfunc, release_encrypt_logicdata, encrypt_logicdata);
	socketer_set_decrypt_function(m_self, decryptfunc, release_decrypt_logicdata, decrypt_logicdata);
}

static void encrypt_decrypt_as_key_do_func(void *logicdata, char *buf, int len) {
	struct encrypt_info *o = (struct encrypt_info *)logicdata;
	for (int i = 0; i < len; ++i) {
		if (o->now_idx >= o->max_idx)
			o->now_idx = 0;

		buf[i] ^= o->buf[o->now_idx];
		++o->now_idx;
	}
}

/* 设置加密key */
void Socketer::SetEncryptKey(const char *key, int key_len) {
	if (!key || key_len < 0)
		return;

	if (!m_encrypt) {
		m_encrypt = encrypt_info_create();
		if (m_encrypt) {
			socketer_set_encrypt_function(m_self, 
					encrypt_decrypt_as_key_do_func, encrypt_info_release, m_encrypt);
		}
	}

	if (m_encrypt) {
		m_encrypt->max_idx = 
			key_len > encrypt_info::enum_encrypt_len ? encrypt_info::enum_encrypt_len : key_len;
		memcpy(&m_encrypt->buf, key, m_encrypt->max_idx);
	}
}

/* 设置解密key */
void Socketer::SetDecryptKey(const char *key, int key_len) {
	if (!key || key_len < 0)
		return;

	if (!m_decrypt) {
		m_decrypt = encrypt_info_create();
		if (m_decrypt) {
			socketer_set_decrypt_function(m_self, 
					encrypt_decrypt_as_key_do_func, encrypt_info_release, m_decrypt);
		}
	}

	if (m_decrypt) {
		m_decrypt->max_idx = 
			key_len > encrypt_info::enum_encrypt_len ? encrypt_info::enum_encrypt_len : key_len;
		memcpy(&m_decrypt->buf, key, m_decrypt->max_idx);
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

/* 设置代理接入参数 */
void Socketer::SetProxyParam(const char *proxy_end_char, int proxy_end_char_len) {
	if (!proxy_end_char || proxy_end_char_len <= 0)
		return;

	if (m_proxy || proxy_end_char_len > proxy_info::enum_proxy_end_char_len)
		return;

	m_proxy = proxy_info_create();
	if (!m_proxy)
		return;

	memcpy(&m_proxy->proxy_end_char, proxy_end_char, proxy_end_char_len);

	socketer_set_proxy_param(m_self, 
			m_proxy->proxy_end_char, (size_t)proxy_end_char_len, 
			m_proxy->proxy_buff, sizeof(m_proxy->proxy_buff) - 1);
}

/* 获取代理数据 */
const char *Socketer::GetProxyData() {
	if (!m_proxy)
		return NULL;

	return m_proxy->proxy_buff;
}

/* 启用/禁用代理接入 */
void Socketer::UseProxy(bool flag) {
	socketer_use_proxy(m_self, flag);
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

/* 设置发送指定字节的原始数据(发送时指定字节的数据不执行压缩、加密操作) */
void Socketer::SetSendRawDataSize(int size) {
	socketer_set_raw_datasize(m_self, size);
}

/*
 * 发送数据，仅仅是把数据压入包队列中，
 * adddata为附加到msg后面的数据，当然会自动修改msg的长度，addsize指定adddata的长度
 */
bool Socketer::SendMsg(Msg *msg, void *adddata, size_t addsize) {
	if (!msg)
		return false;

	if (adddata && addsize == 0) {
		assert(false);
		return false;
	}

	if (!adddata && addsize != 0) {
		assert(false);
		return false;
	}

	if (msg->GetLength() < (int)sizeof(Msg))
		return false;

	if (msg->GetLength() + (int)addsize > MessagePack::message_max_length) {
		assert(false && "msg->GetLength() + addsize > MessagePack::message_max_length");
		return false;
	}

	if (socketer_send_is_limit(m_self, msg->GetLength() + addsize)) {
		Close();
		return false;
	}

	bool res = false;
	if (adddata && addsize != 0) {
		bool res1, res2;
		int onesend = msg->GetLength();
		msg->SetLength(onesend + addsize);
		res1 = socketer_send_msg(m_self, msg, onesend);

		/*
		 * 这里切记要修改回去。
		 * 例：对于同一个包遍历发送给一个列表，然后每次都附带不同尾巴。。。这种情景，那么必须如此恢复。
		 */
		msg->SetLength(onesend);
		res2 = socketer_send_msg(m_self, adddata, addsize);
		res = (res1 && res2);
	} else {
		res = socketer_send_msg(m_self, msg, msg->GetLength());
	}

	if (res) {
		on_send_msg(m_infomgr, 1, msg->GetLength() + addsize);
	}
	return res;
}

/* 接收数据 */
Msg *Socketer::GetMsg(char *buf, size_t bufsize) {
	Msg *msg = (Msg *)socketer_get_msg(m_self, buf, bufsize);
	if (msg) {
		if (msg->GetLength() < (int)sizeof(Msg)) {
			Close();
			return NULL;
		}

		on_recv_msg(m_infomgr, 1, msg->GetLength());
	}
	return msg;
}

/* 发送数据 */
bool Socketer::SendData(const void *data, size_t datasize) {
	if (!data)
		return false;

	if (socketer_send_is_limit(m_self, datasize)) {
		Close();
		return false;
	}

	bool res = socketer_send_data(m_self, (void *)data, (int)datasize);
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

/* 查指定数据的结尾字节数(用于行分隔等) */
int Socketer::FindDataEndSize(const char *data, size_t datalen) {
	return socketer_find_data_end_size(m_self, data, (int)datalen);
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
size_t net_get_memory_info(struct poolmgr_info *array, size_t num) {
	if (!array || num < 8)
		return 0;

	size_t index = 0;

	cspin_lock(&s_infomgr.encrypt_lock);
	poolmgr_get_info(s_infomgr.encrypt_pool, &array[index]);
	cspin_unlock(&s_infomgr.encrypt_lock);
	++index;

	cspin_lock(&s_infomgr.socket_lock);
	poolmgr_get_info(s_infomgr.socket_pool, &array[index]);
	cspin_unlock(&s_infomgr.socket_lock);
	++index;

	cspin_lock(&s_infomgr.listen_lock);
	poolmgr_get_info(s_infomgr.listen_pool, &array[index]);
	cspin_unlock(&s_infomgr.listen_lock);
	++index;

	return index + net_module_get_memory_info(&array[index], num - index);
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

	time_t now = time(NULL);
	for (int i = 0; i < enum_netdata_end; ++i) {
		infomgr->data_table[i].send_msg_num_time = now;
		infomgr->data_table[i].recv_msg_num_time = now;
		infomgr->data_table[i].send_bytes_time = now;
		infomgr->data_table[i].recv_bytes_time = now;
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

	struct datainfo *max_info = &infomgr->data_table[enum_netdata_max];
	struct datainfo *now_info = &infomgr->data_table[enum_netdata_now];

	if (max_info->send_msg_num < now_info->send_msg_num) {
		max_info->send_msg_num = now_info->send_msg_num;
		max_info->send_msg_num_time = time(NULL);
	}

	if (max_info->recv_msg_num < now_info->recv_msg_num) {
		max_info->recv_msg_num = now_info->recv_msg_num;
		max_info->recv_msg_num_time = time(NULL);
	}

	if (max_info->send_bytes < now_info->send_bytes) {
		max_info->send_bytes = now_info->send_bytes;
		max_info->send_bytes_time = time(NULL);
	}

	if (max_info->recv_bytes < now_info->recv_bytes) {
		max_info->recv_bytes = now_info->recv_bytes;
		max_info->recv_bytes_time = time(NULL);
	}

	now_info->send_msg_num = 0;
	now_info->recv_msg_num = 0;
	now_info->send_bytes = 0;
	now_info->recv_bytes = 0;
}

/* 获取网络数据统计对象 */
struct datainfomgr *GetDataInfoMgr() {
	if (s_datainfo_need_release)
		return s_datainfomgr;

	return NULL;
}

}

