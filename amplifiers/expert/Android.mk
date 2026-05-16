LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := expert.c
LOCAL_MODULE := expert

LOCAL_CFLAGS := -DIN_HAMLIB
LOCAL_C_INCLUDES := android include include/hamlib lib src
LOCAL_LDLIBS := -lhamlib

include $(BUILD_STATIC_LIBRARY)
