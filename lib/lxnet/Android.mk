LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := lxnet_static

LOCAL_MODULE_FILENAME := liblxnet

LOCAL_SRC_FILES := ./../../base/crosslib.c \
					./../../base/log.c \
					./../../base/pool.c \
					./../../3rd/quicklz/quicklz.c \
					./src/buf/net_buf.c \
					./src/buf/net_bufpool.c \
					./src/buf/net_compress.c \
					./src/buf/net_thread_buf.c \
					./src/event/linux_eventmgr.c \
					./src/event/net_module.c \
					./src/sock/_netlisten.c \
					./src/sock/_netsocket.c \
					./src/sock/net_common.c \
					./src/sock/net_pool.c \
					./src/threadpool/threadpool.c \
					./lxnet.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../base \
					$(LOCAL_PATH)/../../base/buf \
					$(LOCAL_PATH)/../../3rd/quicklz \
                    $(LOCAL_PATH)/src/buf \
					$(LOCAL_PATH)/src/event \
					$(LOCAL_PATH)/src/sock \
					$(LOCAL_PATH)/src/threadpool \

LOCAL_CFLAGS := -fPIC -DNDEBUG -O2
LOCAL_CPPFLAGS := -fPIC -DNDEBUG -O2

LOCAL_EXPORT_CFLAGS := -fPIC -DNDEBUG -O2
LOCAL_EXPORT_CPPFLAGS := -fPIC -DNDEBUG -O2

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH) \
						   $(LOCAL_PATH)/../../base

include $(BUILD_STATIC_LIBRARY)
