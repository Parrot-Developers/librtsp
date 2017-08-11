
LOCAL_PATH := $(call my-dir)

##################
#  RTSP library  #
##################

include $(CLEAR_VARS)
LOCAL_MODULE := librtsp
LOCAL_DESCRIPTION := Real Time Streaming Protocol library
LOCAL_CATEGORY_PATH := libs
LOCAL_SRC_FILES := \
    src/rtsp.c \
    src/rtsp_server.c \
    src/rtsp_client.c \
    src/rtsp_log.c
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_LIBRARIES := libpomp libfutils
LOCAL_CONDITIONAL_LIBRARIES := OPTIONAL:libulog

include $(BUILD_LIBRARY)

############################
#  Server test executable  #
############################

include $(CLEAR_VARS)
LOCAL_MODULE := rtsp_server_test
LOCAL_DESCRIPTION := Real Time Streaming Protocol library server test program
LOCAL_CATEGORY_PATH := multimedia
LOCAL_SRC_FILES := test/rtsp_server_test.c
LOCAL_LIBRARIES := librtsp libpomp libulog
include $(BUILD_EXECUTABLE)

############################
#  Client test executable  #
############################

include $(CLEAR_VARS)
LOCAL_MODULE := rtsp_client_test
LOCAL_DESCRIPTION := Real Time Streaming Protocol library client test program
LOCAL_CATEGORY_PATH := multimedia
LOCAL_SRC_FILES := test/rtsp_client_test.c
LOCAL_LIBRARIES := librtsp libsdp libpomp libulog
include $(BUILD_EXECUTABLE)
