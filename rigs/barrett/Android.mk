LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := barrett.c 950.c 4050.c 4100.c
LOCAL_MODULE := barrett

LOCAL_CFLAGS := -DIN_HAMLIB
LOCAL_C_INCLUDES := android include include/hamlib lib src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
