
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := librtsp
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Real Time Streaming Protocol library
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -DRTSP_API_EXPORTS -fvisibility=hidden -std=gnu99 -D_GNU_SOURCE
LOCAL_CXXFLAGS := -DRTSP_API_EXPORTS -fvisibility=hidden -std=c++11 -D_GNU_SOURCE
LOCAL_SRC_FILES := \
	src/rtsp.c \
	src/rtsp_auth.c \
	src/rtsp_base64.c \
	src/rtsp_client.c \
	src/rtsp_client_session.c \
	src/rtsp_server.c \
	src/rtsp_server_request.c \
	src/rtsp_server_session.c \
	src/rtsp_url.c \
	src/rtsp_url.cpp
LOCAL_LIBRARIES := \
	libcrypto \
	libfutils \
	libpomp \
	libtransport-packet \
	libtransport-socket \
	libtransport-tls \
	libulog
ifeq ("$(TARGET_OS)","windows")
  LOCAL_CFLAGS += -D_WIN32_WINNT=0x0600
  LOCAL_LDLIBS += -lws2_32
endif
include $(BUILD_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := librtsp-internal
LOCAL_DESCRIPTION := Real Time Streaming Protocol library internal headers
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src/internal/

include $(BUILD_CUSTOM)


include $(CLEAR_VARS)
LOCAL_MODULE := rtsp-server-test
LOCAL_CATEGORY_PATH := multimedia
LOCAL_DESCRIPTION := Real Time Streaming Protocol library server test program
LOCAL_SRC_FILES := \
	tools/rtsp_server_test.c
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
	tools/rtsp_client_test.c
LOCAL_LIBRARIES := \
	libpomp \
	librtsp \
	libsdp \
	libulog
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := rtsp-client-ingest-test
LOCAL_CATEGORY_PATH := multimedia
LOCAL_DESCRIPTION := Real Time Streaming Protocol library client ingest test program
LOCAL_SRC_FILES := \
	tools/rtsp_client_ingest_test.c
LOCAL_LIBRARIES := \
	libpomp \
	librtsp \
	libsdp \
	libulog
include $(BUILD_EXECUTABLE)


ifdef TARGET_TEST

include $(CLEAR_VARS)

LOCAL_MODULE := tst-librtsp
LOCAL_CFLAGS += -DTARGET_TEST -D_GNU_SOURCE
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src
LOCAL_EXPORT_CXXFLAGS := -std=c++11
LOCAL_SRC_FILES := \
	tests/rtsp_test_auth.c \
	tests/rtsp_test_base64.c \
	tests/rtsp_test_url.c \
	tests/rtsp_test_url.cpp \
	tests/rtsp_test.c
LOCAL_LIBRARIES := \
	libcunit \
	libfutils \
	librtsp

include $(BUILD_EXECUTABLE)

endif
