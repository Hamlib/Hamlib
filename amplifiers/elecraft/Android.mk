LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := elecraft.c kpa.c kpa1500.c
LOCAL_MODULE := elecraft

LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -lhamlib

include $(BUILD_STATIC_LIBRARY)
