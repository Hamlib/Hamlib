LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := adat.c adt_200a.c
LOCAL_MODULE := adat

LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := $(LOCAL_SHARED_LIBRARIES) -Lobj/local/armeabi

include $(BUILD_STATIC_LIBRARY)
