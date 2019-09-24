
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_MSG_BASE_H_
#define _H_MSG_BASE_H_
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "platform_config.h"

#pragma pack(push, 1)

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
		message_max_length = 128 * 1024,

		//消息体最大长度
		message_data_max_length = message_max_length - sizeof(Msg),

		//字符串最大长度(用无符号16位记录长度)
		string_max_length = USHRT_MAX,

		//大字符串最大长度(用无符号32位记录长度)
		big_string_max_length = message_data_max_length - 4,
	};

	char m_buf[message_data_max_length];
	size_t m_index;			//当前索引
	int m_maxindex;			//最大索引值	主要是用于读时
	int m_error_num;		//出错次数
	bool m_enable_assert;	//是否开启assert

	MessagePack() {
		header.length = sizeof(Msg);
		memset(m_buf, 0, sizeof(m_buf));
		m_index = 0;
		m_maxindex = 0;
		m_error_num = 0;
		m_enable_assert = true;
	}

	bool HasError() {
		return m_error_num != 0;
	}

	int GetErrorNum() {
		return m_error_num;
	}

	void SetIndex(size_t idx) {
		if (idx >= message_data_max_length)
			idx = message_data_max_length - 1;

		if ((int)idx < 0)
			idx = 0;

		m_index = idx;
		m_maxindex = GetLength() - (int)sizeof(Msg);
	}

	int GetIndex() {
		return m_index;
	}

	//切记在从包中取出时。调用此函数，重置缓冲索引
	void Begin(bool enable_assert = true) {
		m_index = 0;
		m_maxindex = GetLength() - (int)sizeof(Msg);

		m_error_num = 0;
		m_enable_assert = enable_assert;
	}

	void Reset(bool enable_assert = true) {
		m_index = 0;
		m_maxindex = 0;
		header.length = sizeof(Msg);

		m_error_num = 0;
		m_enable_assert = enable_assert;
	}

	bool CanPush(size_t size) {
		if (m_index + size <= message_data_max_length)
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
	void PushBoolean(bool data) {
		PushBlock(&data, sizeof(data));
	}

	void PushInt8(int8 data) {
		PushBlock(&data, sizeof(data));
	}

	void PushUInt8(uint8 data) {
		PushBlock(&data, sizeof(data));
	}

	void PushInt16(int16 data) {
		PushBlock(&data, sizeof(data));
	}

	void PushUInt16(uint16 data) {
		PushBlock(&data, sizeof(data));
	}

	void PushInt32(int32 data) {
		PushBlock(&data, sizeof(data));
	}

	void PushUInt32(uint32 data) {
		PushBlock(&data, sizeof(data));
	}

	void PushInt64(int64 data) {
		PushBlock(&data, sizeof(data));
	}

	void PushUInt64(uint64 data) {
		PushBlock(&data, sizeof(data));
	}

	void PushFloat(float data) {
		PushBlock(&data, sizeof(data));
	}

	void PushDouble(double data) {
		PushBlock(&data, sizeof(data));
	}

	bool PushBlock(const void *data, size_t size) {
		if (data) {
			if (CanPush(size)) {
				__write_data(data, size);
				return true;
			}
		}

		__on_error();
		return false;
	}

	bool PushLBlock(const void *data, size_t size) {
		if (data) {
			uint32 temp_size = (uint32)size;
			if (CanPush(sizeof(temp_size) + size)) {
				__write_data(&temp_size, sizeof(temp_size));
				__write_data(data, size);
				return true;
			}
		}

		__on_error();
		return false;
	}

	bool PushLString(const char *str, size_t str_size, size_t max_push = string_max_length) {
		if (!str || str_size > string_max_length || max_push > string_max_length) {
			__on_error();
			return false;
		}

		if (str_size > max_push)
			str_size = max_push;

		uint16 temp_size = (uint16)str_size;
		if (CanPush(sizeof(temp_size) + str_size)) {
			__write_data(&temp_size, sizeof(temp_size));
			__write_data(str, str_size);
			return true;
		}

		__on_error();
		return false;
	}

	bool PushString(const char *str, size_t max_push = string_max_length) {
		if (!str) {
			__on_error();
			return false;
		}

		size_t str_size = strlen(str);
		return PushLString(str, str_size, max_push);
	}

	bool PushLBigString(const char *str, size_t str_size, size_t max_push = big_string_max_length) {
		if (!str || str_size > big_string_max_length || max_push > big_string_max_length) {
			__on_error();
			return false;
		}

		if (str_size > max_push)
			str_size = max_push;

		uint32 temp_size = (uint32)str_size;
		if (CanPush(sizeof(temp_size) + str_size)) {
			__write_data(&temp_size, sizeof(temp_size));
			__write_data(str, str_size);
			return true;
		}

		__on_error();
		return false;
	}

	bool PushBigString(const char *str, size_t max_push = big_string_max_length) {
		if (!str) {
			__on_error();
			return false;
		}

		size_t str_size = strlen(str);
		return PushLBigString(str, str_size, max_push);
	}

	//用于覆盖数据。不做包长度的累加
	bool PutDataNotAddLength(size_t index, const void *data, size_t size) {
		if (!data) {
			__on_error();
			return false;
		}

		if ((index + size) > message_data_max_length) {
			__on_error();
			return false;
		}

		memcpy(&m_buf[index], data, size);
		return true;
	}



	/*
	 * ================================================================================
	 * reader interface.
	 * ================================================================================
	 */
	bool GetBoolean() {
		bool temp = false;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	int8 GetInt8() {
		int8 temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	uint8 GetUInt8() {
		uint8 temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	int16 GetInt16() {
		int16 temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	uint16 GetUInt16() {
		uint16 temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	int32 GetInt32() {
		int32 temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	uint32 GetUInt32() {
		uint32 temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	int64 GetInt64() {
		int64 temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	uint64 GetUInt64() {
		uint64 temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	float GetFloat() {
		float temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	double GetDouble() {
		double temp = 0;
		GetBlock(&temp, sizeof(temp));
		return temp;
	}

	bool GetBlock(void *buf, size_t size) {
		if (!buf || 0 == size) {
			__on_error();
			return false;
		}

		if (CanGet(size)) {
			__read_data(buf, size);
			return true;
		}

		__on_error();
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

		__on_error();
		return NULL;
	}

	const char *GetLBlock(size_t *datalen) {
		*datalen = 0;
		uint32 size = 0;
		if (CanGet(sizeof(size))) {
			__read_data(&size, sizeof(size));
			if (0 == size)
				return "";

			if (CanGet((size_t)size)) {
				*datalen = (size_t)size;
				return __read_data_ref((size_t)size);
			}
		}

		__on_error();
		return NULL;
	}

	const char *GetLString(size_t *datalen) {
		*datalen = 0;
		uint16 size = 0;
		if (CanGet(sizeof(size))) {
			__read_data(&size, sizeof(size));
			if (size > string_max_length) {
				__on_error();
				return NULL;
			}

			if (0 == size)
				return "";

			if (CanGet((size_t)size)) {
				*datalen = (size_t)size;
				return __read_data_ref((size_t)size);
			}
		}

		__on_error();
		return NULL;
	}

	bool GetString(char *buf, size_t buflen) {
		if (buflen < 1) {
			__on_error();
			return false;
		}

		buf[buflen - 1] = '\0';
		uint16 size = 0;
		if (CanGet(sizeof(size))) {
			__read_data(&size, sizeof(size));
			if (size > string_max_length) {
				buf[0] = '\0';
				__on_error();
				return false;
			}

			if (0 == size) {
				buf[0] = '\0';
				return true;
			}

			buf[(size_t)(buflen > (size_t)size ? size : (buflen - 1))] = '\0';
			return GetBlock(buf, (size_t)(buflen > (size_t)size ? size : (buflen - 1)));
		}

		__on_error();
		return false;
	}

	const char *GetLBigString(size_t *datalen) {
		*datalen = 0;
		uint32 size = 0;
		if (CanGet(sizeof(size))) {
			__read_data(&size, sizeof(size));
			if (size > big_string_max_length) {
				__on_error();
				return NULL;
			}

			if (0 == size)
				return "";

			if (CanGet((size_t)size)) {
				*datalen = (size_t)size;
				return __read_data_ref((size_t)size);
			}
		}

		__on_error();
		return NULL;
	}

	bool GetBigString(char *buf, size_t buflen) {
		if (buflen < 1) {
			__on_error();
			return false;
		}

		buf[buflen - 1] = '\0';
		uint32 size = 0;
		if (CanGet(sizeof(size))) {
			__read_data(&size, sizeof(size));
			if (size > big_string_max_length) {
				buf[0] = '\0';
				__on_error();
				return false;
			}

			if (0 == size) {
				buf[0] = '\0';
				return true;
			}

			buf[(size_t)(buflen > (size_t)size ? size : (buflen - 1))] = '\0';
			return GetBlock(buf, (size_t)(buflen > (size_t)size ? size : (buflen - 1)));
		}

		__on_error();
		return false;
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

	inline void __on_error() {
		++m_error_num;

		if (m_enable_assert) {
			assert(false && "error!");
		}
	}

};

#pragma pack(pop)

#endif

