LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := apex.c apex.h sharedloop.c
LOCAL_MODULE := apex

LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
