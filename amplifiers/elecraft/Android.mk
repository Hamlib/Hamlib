LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := kpa.c kpa1500.c
LOCAL_MODULE := elecraft

LOCAL_CFLAGS := -DIN_HAMLIB
LOCAL_C_INCLUDES := android include include/hamlib lib src
LOCAL_LDLIBS := -lhamlib

include $(BUILD_STATIC_LIBRARY)
