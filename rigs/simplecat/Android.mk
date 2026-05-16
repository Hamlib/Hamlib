# Android makefile for Simple CAT rig

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := simplecat
LOCAL_SRC_FILES = simplecat.c

LOCAL_CFLAGS := -DIN_HAMLIB
LOCAL_C_INCLUDES := android include include/hamlib lib src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
