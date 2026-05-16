# Android makefile for Simple CAT rig

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := simplecat
LOCAL_SRC_FILES = simplecat.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../..

include $(BUILD_STATIC_LIBRARY)
