LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := rot_ioptron.c
LOCAL_MODULE := ioptron

LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -Lobj/local/armeabi

include $(BUILD_STATIC_LIBRARY)
