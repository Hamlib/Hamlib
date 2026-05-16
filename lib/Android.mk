LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := misc
LOCAL_SRC_FILES = cJSON.c cJSON.h asyncpipe.c asyncpipe.h precise_time.c

LOCAL_CFLAGS := -DIN_HAMLIB
LOCAL_C_INCLUDES := android lib include src
LOCAL_LDLIBS := -llog -landroid

include $(BUILD_STATIC_LIBRARY)
