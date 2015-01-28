#ifndef _H_MSG_BASE_H_
#define _H_MSG_BASE_H_
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "platform_config.h"

#pragma pack(push,1)
struct MsgHeader {
	int msglength;
};

struct Msg {
	MsgHeader header;
	short msgtype;
	Msg() {
		SetLength(sizeof(header) + sizeof(msgtype));
	}

	int GetLength() { return header.msglength; }
	void SetLength(int len) { header.msglength = len; }
	short GetType() { return msgtype; }
	void SetType(short type) { msgtype = type; }
};

struct MessagePack:public Msg {
	enum  {
		e_thismessage_max_size = 1024*128	//此消息最大长度
	};

	char m_buf[e_thismessage_max_size];
	size_t m_index;		//位置
	int m_maxindex;		//最大索引值	主要是用于读时

	MessagePack() {
		header.msglength = sizeof(Msg);
		memset(m_buf, 0, sizeof(m_buf));
		m_index = 0;
		m_maxindex = 0;
	}

	//切记在从包中取出时。调用此函数，重置缓冲索引
	void Begin() {
		m_index = 0;
		m_maxindex = GetLength() - (int)sizeof(Msg);
	}

	//设置当前位置索引
	void SetIndex(size_t idx) {
		if (idx >= e_thismessage_max_size)
			idx = e_thismessage_max_size - 1;
		if (idx < 0)
			idx = 0;
		m_index = idx;
		m_maxindex = GetLength() - (int)sizeof(Msg);
	}

	//重置包大小以及索引
	void ResetMsgLength() {
		Begin();
		header.msglength = sizeof(Msg);
		m_maxindex = 0;
	}

	//用于覆盖数据。不做包长度的累加
	void PutDataNotAddLength(size_t index, void *data, size_t size) {
		if (!data) {
			assert(false && "data is NULL !!");
			return;
		}

		if ((index + size) > e_thismessage_max_size) {
			assert(false && "is overflow!!! error!");
			return;
		}

		memcpy(&m_buf[index], data, size);
	}

	bool CanPush(size_t size) {
		if ((m_index + size) > e_thismessage_max_size)
			return false;
		return true;
	}

	bool PushBlock(void *data, size_t size) {
		if (!data)
			return false;
		if ((m_index + size) > e_thismessage_max_size) {
			assert(false && "error!");
			return false;
		}

		memcpy(&m_buf[m_index], data, size);
		m_index += size;
		header.msglength += size;
		return true;
	}

	bool PushLString(const char *str, size_t strsize, int16 maxpush = SHRT_MAX - 3) {
		assert(strsize < SHRT_MAX - 3);
		int16 size = (int16)strsize;
		if (size > maxpush)
			size = maxpush;
		assert(size >= 0);
		if (size < 0)
			return false;
		PushInt16(size);
		if (0 == size)
			return true;
		return PushBlock((void *)str, size);
	}

	bool PushString(const char *str, int16 maxpush = SHRT_MAX - 3) {
		size_t strsize = strlen(str);
		assert(strsize < SHRT_MAX - 3);
		int16 size = (int16)strsize;
		if (size > maxpush)
			size = maxpush;
		assert(size >= 0);
		if (size < 0)
			return false;
		PushInt16(size);
		if (0 == size)
			return true;
		return PushBlock((void *)str, size);
	}

	bool PushLBigString(const char *str, size_t strsize, int32 maxpush = INT_MAX - 3) {
		assert(strsize < INT_MAX - 3);
		int32 size = (int32)strsize;
		if (size > maxpush)
			size = maxpush;
		assert(size >= 0);
		if (size < 0)
			return false;
		PushInt32(size);
		if (0 == size)
			return true;
		return PushBlock((void *)str, size);
	}	

	bool PushBigString(const char *str, int32 maxpush = INT_MAX - 3) {
		size_t strsize = strlen(str);
		assert(strsize < INT_MAX - 3);
		int32 size = (int32)strsize;
		if (size > maxpush)
			size = maxpush;
		assert(size >= 0);
		if (size < 0)
			return false;
		PushInt32(size);
		if (0 == size)
			return true;
		return PushBlock((void *)str, size);
	}

	void PushInt64(int64 data) {
		if ((m_index + sizeof(data)) > e_thismessage_max_size) {
			assert(false && "error!");
			return;
		}

		memcpy(&m_buf[m_index], &data, sizeof(data));
		m_index += sizeof(data);
		header.msglength += sizeof(data);
	}

	void PushInt32(int32 data) {
		if ((m_index + sizeof(data)) > e_thismessage_max_size) {
			assert(false && "error!");
			return;
		}

		memcpy(&m_buf[m_index], &data, sizeof(data));
		m_index += sizeof(data);
		header.msglength += sizeof(data);
	}

	void PushInt16(int16 data) {
		if ((m_index + sizeof(data)) > e_thismessage_max_size) {
			assert(false && "error!");
			return;
		}

		memcpy (&m_buf[m_index], &data, sizeof(data));
		m_index += sizeof(data);
		header.msglength += sizeof(data);
	}

	void PushInt8(int8 data) {
		if ((m_index + sizeof(data)) > e_thismessage_max_size) {
			assert(false && "error!");
			return;
		}

		memcpy(&m_buf[m_index], &data, sizeof(data));
		m_index += sizeof(data);
		header.msglength += sizeof(data);
	}

	void PushBoolean(bool value) {
		PushInt8((int8)value);
	}

	void PushFloat(float data) {
		if ((m_index + sizeof(data)) > e_thismessage_max_size) {
			assert(false && "error!");
			return;
		}

		memcpy(&m_buf[m_index], &data, sizeof(data));
		m_index += sizeof(data);
		header.msglength += sizeof(data);
	}

	void PushDouble(double data) {
		if ((m_index + sizeof(data)) > e_thismessage_max_size) {
			assert(false && "error!");
			return;
		}

		memcpy(&m_buf[m_index], &data, sizeof(data));
		m_index += sizeof(data);
		header.msglength += sizeof(data);
	}

	bool GetBlock(void *data, size_t size) {
		if (size == 0)
			return false;
		if (int(m_index + size) > m_maxindex) {
			assert(false && "error!");
			return false;
		}

		memcpy(data, &m_buf[m_index], size);
		m_index += size;
		return true;
	}

	const char *GetLString(size_t *datalen) {
		*datalen = 0;
		int16 size = GetInt16();
		assert(size >= 0);
		if (size < 0)
			return NULL;

		if (0 == size)
			return "";

		if (int(m_index + size) > m_maxindex) {
			assert(false && "error!");
			return NULL;
		}

		const char *data = &m_buf[m_index];
		*datalen = (size_t)size;
		m_index += size;
		return data;
	}

	bool GetString(char *buf, size_t buflen) {
		assert(buflen >= 1);
		if (buflen < 1)
			return false;
		buf[buflen-1] = '\0';
		int16 size = GetInt16();
		assert(size >= 0);
		if (size < 0) {
			buf[0] = '\0';
			return false;
		}

		if (0 == size) {
			buf[0] = '\0';
			return true;
		}

		buf[(size_t)(buflen > (size_t)size ? size : (buflen-1))] = '\0';
		return GetBlock(buf, (size_t)(buflen > (size_t)size ? size : (buflen-1)));
	}
	
	const char *GetLBigString(size_t *datalen) {
		*datalen = 0;
		int32 size = GetInt32();
		assert(size >= 0);
		if (size < 0)
			return NULL;

		if (0 == size)
			return "";

		if (int(m_index + size) > m_maxindex) {
			assert(false && "error!");
			return NULL;
		}

		const char *data = &m_buf[m_index];
		*datalen = size;
		m_index += size;
		return data;
	}

	bool GetBigString(char *buf, size_t buflen) {
		assert(buflen >= 1);
		if (buflen < 1)
			return false;
		buf[buflen-1] = '\0';
		int32 size = GetInt32();
		assert(size >= 0);
		if (size < 0) {
			buf[0] = '\0';
			return false;
		}

		if (0 == size) {
			buf[0] = '\0';
			return true;
		}

		buf[(size_t)(buflen > (size_t)size ? size : (buflen-1))] = '\0';
		return GetBlock(buf, (size_t)(buflen > (size_t)size ? size : (buflen-1)));
	}

	int64 GetInt64() {
		if(m_index + sizeof( int64 ) > (size_t)m_maxindex) {
			assert(false && "error!");
			return (int64)0;
		}

		int64 temp;
		memcpy(&temp, &m_buf[m_index], sizeof(temp));
		m_index += sizeof(temp);
		return temp;
	}

	int32 GetInt32() {
		int32 temp;
		if (m_index + sizeof(temp) > (size_t)m_maxindex) {
			assert(false && "error!");
			return (int32)0;
		}

		memcpy(&temp, &m_buf[m_index], sizeof(temp));
		m_index += sizeof(temp);
		return temp;
	}

	int16 GetInt16() {
		int16 temp;
		if (m_index + sizeof(temp) > (size_t)m_maxindex) {
			assert(false && "error!");
			return (int16)0;
		}

		memcpy(&temp, &m_buf[m_index], sizeof(temp));
		m_index += sizeof(temp);
		return temp;
	}

	int8 GetInt8() {
		int8 temp;
		if (m_index + sizeof(temp) > (size_t)m_maxindex) {
			assert(false && "error!");
			return (short)0;
		}

		temp = *((int8*)&m_buf[m_index]);
		m_index += sizeof(temp);
		return temp;
	}

	bool GetBoolean() {
		return (bool)GetInt8();
	}

	float GetFloat() {
		if (m_index + sizeof(float) > (size_t)m_maxindex) {
			assert(false && "error!");
			return (float)0.0;
		}

		float temp;
		memcpy(&temp, &m_buf[m_index], sizeof(temp));
		m_index += sizeof(temp);
		return temp;
	}

	double GetDouble() {
		if (m_index + sizeof(double) > (size_t)m_maxindex) {
			assert(false && "error!");
			return (double)0.0;
		}

		double temp;
		memcpy(&temp, &m_buf[m_index], sizeof(temp));
		m_index += sizeof(temp);
		return temp;
	}
};

#pragma pack(pop)

#endif

