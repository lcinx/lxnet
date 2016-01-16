#ifndef _H_MSG_BASE_H_
#define _H_MSG_BASE_H_
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "platform_config.h"

#pragma pack(push,1)

struct MsgHeader {
	int32 length;
};

struct Msg {
	MsgHeader header;
	int16 msgtype;

	Msg() {
		SetLength(sizeof(header) + sizeof(msgtype));
	}

	void SetLength(int32 length) { header.length = length; }
	int32 GetLength() { return header.length; }
	void SetType(int16 type) { msgtype = type; }
	int16 GetType() { return msgtype; }
};

struct MessagePack:public Msg {
	enum {
		//消息最大长度
		e_thismessage_max_size = 1024 * 128 - sizeof(Msg)
	};

	char m_buf[e_thismessage_max_size];
	size_t m_index;		//当前索引
	int m_maxindex;		//最大索引值	主要是用于读时

	MessagePack() {
		header.length = sizeof(Msg);
		memset(m_buf, 0, sizeof(m_buf));
		m_index = 0;
		m_maxindex = 0;
	}

	void SetIndex(size_t idx) {
		if (idx >= e_thismessage_max_size)
			idx = e_thismessage_max_size - 1;

		if ((int)idx < 0)
			idx = 0;

		m_index = idx;
		m_maxindex = GetLength() - (int)sizeof(Msg);
	}

	int GetIndex() {
		return m_index;
	}

	//切记在从包中取出时。调用此函数，重置缓冲索引
	void Begin() {
		m_index = 0;
		m_maxindex = GetLength() - (int)sizeof(Msg);
	}

	void Reset() {
		m_index = 0;
		m_maxindex = 0;
		header.length = sizeof(Msg);
	}

	bool CanPush(size_t size) {
		if (m_index + size <= e_thismessage_max_size)
			return true;
		return false;
	}

	bool CanGet(size_t size) {
		if ((int)(m_index + size) <= m_maxindex)
			return true;
		return false;
	}



	/*
	 * ================================================================================
	 * writer interface.
	 * ================================================================================
	 */
	void PushBoolean(bool value) {
		PushInt8((int8)value);
	}

	void PushInt8(int8 data) {
		if (CanPush(sizeof(data))) {
			__write_data(&data, sizeof(data));
		} else {
			assert(false && "error!");
		}
	}

	void PushInt16(int16 data) {
		if (CanPush(sizeof(data))) {
			__write_data(&data, sizeof(data));
		} else {
			assert(false && "error!");
		}
	}

	void PushInt32(int32 data) {
		if (CanPush(sizeof(data))) {
			__write_data(&data, sizeof(data));
		} else {
			assert(false && "error!");
		}
	}

	void PushInt64(int64 data) {
		if (CanPush(sizeof(data))) {
			__write_data(&data, sizeof(data));
		} else {
			assert(false && "error!");
		}
	}

	void PushFloat(float data) {
		if (CanPush(sizeof(data))) {
			__write_data(&data, sizeof(data));
		} else {
			assert(false && "error!");
		}
	}

	void PushDouble(double data) {
		if (CanPush(sizeof(data))) {
			__write_data(&data, sizeof(data));
		} else {
			assert(false && "error!");
		}
	}

	bool PushBlock(const void *data, size_t size) {
		if (!data)
			return false;

		if (CanPush(size)) {
			__write_data(data, size);
			return true;
		}

		assert(false && "error!");
		return false;
	}

	bool PushLBlock(const void *data, size_t size) {
		if (!data)
			return false;

		if (CanPush(sizeof(int32) + size)) {
			PushInt32(size);
			__write_data(data, size);
			return true;
		}

		assert(false && "error!");
		return false;
	}

	bool PushLString(const char *str, size_t str_size, int16 max_push = SHRT_MAX - 3) {
		assert(str);
		assert(str_size < SHRT_MAX - 3);
		int16 size = (int16)str_size;
		if (size > max_push)
			size = max_push;

		assert(size >= 0);
		if (!str || size < 0)
			return false;

		if (CanPush(sizeof(int16) + (size_t)size)) {
			PushInt16(size);
			__write_data(str, (size_t)size);
			return true;
		}

		assert(false && "error!");
		return false;
	}

	bool PushString(const char *str, int16 max_push = SHRT_MAX - 3) {
		if (!str)
			return false;

		size_t str_size = strlen(str);
		return PushLString(str, str_size, max_push);
	}

	bool PushLBigString(const char *str, size_t str_size, int32 max_push = INT_MAX - 3) {
		assert(str);
		assert(str_size < INT_MAX - 3);
		int32 size = (int32)str_size;
		if (size > max_push)
			size = max_push;

		assert(size >= 0);
		if (!str || size < 0)
			return false;

		if (CanPush(sizeof(int32) + (size_t)size)) {
			PushInt32(size);
			__write_data(str, (size_t)size);
			return true;
		}

		assert(false && "error!");
		return false;
	}

	bool PushBigString(const char *str, int32 max_push = INT_MAX - 3) {
		if (!str)
			return false;

		size_t str_size = strlen(str);
		return PushLBigString(str, str_size, max_push);
	}

	//用于覆盖数据。不做包长度的累加
	void PutDataNotAddLength(size_t index, const void *data, size_t size) {
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



	/*
	 * ================================================================================
	 * reader interface.
	 * ================================================================================
	 */
	bool GetBoolean() {
		return (bool)GetInt8();
	}

	int8 GetInt8() {
		int8 temp = 0;
		if (CanGet(sizeof(temp))) {
			__read_data(&temp, sizeof(temp));
		} else {
			assert(false && "error!");
		}
		return temp;
	}

	int16 GetInt16() {
		int16 temp = 0;
		if (CanGet(sizeof(temp))) {
			__read_data(&temp, sizeof(temp));
		} else {
			assert(false && "error!");
		}
		return temp;
	}

	int32 GetInt32() {
		int32 temp = 0;
		if (CanGet(sizeof(temp))) {
			__read_data(&temp, sizeof(temp));
		} else {
			assert(false && "error!");
		}
		return temp;
	}

	int64 GetInt64() {
		int64 temp = 0;
		if (CanGet(sizeof(temp))) {
			__read_data(&temp, sizeof(temp));
		} else {
			assert(false && "error!");
		}
		return temp;
	}

	float GetFloat() {
		float temp = 0;
		if (CanGet(sizeof(temp))) {
			__read_data(&temp, sizeof(temp));
		} else {
			assert(false && "error!");
		}
		return temp;
	}

	double GetDouble() {
		double temp = 0;
		if (CanGet(sizeof(temp))) {
			__read_data(&temp, sizeof(temp));
		} else {
			assert(false && "error!");
		}
		return temp;
	}

	bool GetBlock(void *buf, size_t size) {
		if (!buf || 0 == size)
			return false;

		if (CanGet(size)) {
			__read_data(buf, size);
			return true;
		}

		assert(false && "error!");
		return false;
	}

	const char *GetBlockRef(size_t size, size_t *datalen) {
		*datalen = 0;
		size_t get_size = m_maxindex - m_index;
		size = get_size > size ? size : get_size;

		if (0 == size)
			return NULL;

		if (CanGet(size)) {
			*datalen = size;
			return __read_data_ref(size);
		}

		assert(false && "error!");
		return NULL;
	}

	const char *GetLBlock(size_t *datalen) {
		*datalen = 0;
		int32 size = GetInt32();
		assert(size >= 0);
		if (size < 0)
			return NULL;

		if (0 == size)
			return "";

		if (CanGet((size_t)size)) {
			*datalen = (size_t)size;
			return __read_data_ref((size_t)size);
		}

		assert(false && "error!");
		return NULL;
	}

	const char *GetLString(size_t *datalen) {
		*datalen = 0;
		int16 size = GetInt16();
		assert(size >= 0);
		if (size < 0)
			return NULL;

		if (0 == size)
			return "";

		if (CanGet((size_t)size)) {
			*datalen = (size_t)size;
			return __read_data_ref((size_t)size);
		}

		assert(false && "error!");
		return NULL;
	}

	bool GetString(char *buf, size_t buflen) {
		assert(buflen >= 1);
		if (buflen < 1)
			return false;

		buf[buflen - 1] = '\0';
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

		buf[(size_t)(buflen > (size_t)size ? size : (buflen - 1))] = '\0';
		return GetBlock(buf, (size_t)(buflen > (size_t)size ? size : (buflen - 1)));
	}

	const char *GetLBigString(size_t *datalen) {
		*datalen = 0;
		int32 size = GetInt32();
		assert(size >= 0);
		if (size < 0)
			return NULL;

		if (0 == size)
			return "";

		if (CanGet((size_t)size)) {
			*datalen = (size_t)size;
			return __read_data_ref((size_t)size);
		}

		assert(false && "error!");
		return NULL;
	}

	bool GetBigString(char *buf, size_t buflen) {
		assert(buflen >= 1);
		if (buflen < 1)
			return false;

		buf[buflen - 1] = '\0';
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

		buf[(size_t)(buflen > (size_t)size ? size : (buflen - 1))] = '\0';
		return GetBlock(buf, (size_t)(buflen > (size_t)size ? size : (buflen - 1)));
	}



private:

	inline void __write_data(const void *data, size_t size) {
		memcpy(&m_buf[m_index], data, size);
		m_index += size;
		header.length += size;
	}

	inline void __read_data(void *buf, size_t size) {
		memcpy(buf, &m_buf[m_index], size);
		m_index += size;
	}

	inline const char *__read_data_ref(size_t size) {
		const char *data = &m_buf[m_index];
		m_index += size;
		return data;
	}

};

#pragma pack(pop)

#endif

