
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := librtsp
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Real Time Streaming Protocol library
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -DRTSP_API_EXPORTS -fvisibility=hidden -std=gnu99
LOCAL_SRC_FILES := \
	src/rtsp.c \
	src/rtsp_client.c \
	src/rtsp_client_session.c \
	src/rtsp_server.c \
	src/rtsp_server_request.c \
	src/rtsp_server_session.c
LOCAL_LIBRARIES := \
	libfutils \
	libpomp \
	libulog
ifeq ("$(TARGET_OS)","windows")
  LOCAL_CFLAGS += -D_WIN32_WINNT=0x0600
  LOCAL_LDLIBS += -lws2_32
endif
include $(BUILD_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := rtsp-server-test
LOCAL_CATEGORY_PATH := multimedia
LOCAL_DESCRIPTION := Real Time Streaming Protocol library server test program
LOCAL_SRC_FILES := \
	tests/rtsp_server_test.c
LOCAL_LIBRARIES := \
	libfutils \
	libpomp \
	librtsp \
	libsdp \
	libulog
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := rtsp-client-test
LOCAL_CATEGORY_PATH := multimedia
LOCAL_DESCRIPTION := Real Time Streaming Protocol library client test program
LOCAL_SRC_FILES := \
	tests/rtsp_client_test.c
LOCAL_LIBRARIES := \
	libpomp \
	librtsp \
	libsdp \
	libulog
include $(BUILD_EXECUTABLE)
