LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := dummy_common.c dummy.c rot_dummy.c rot_pstrotator.c netrigctl.c netrotctl.c flrig.c trxmanager.c amp_dummy.c netampctl.c tci1x.c aclog.c sdrsharp.c quisk.c gqrx.c
LOCAL_MODULE := dummy

LOCAL_CFLAGS := -DIN_HAMLIB
LOCAL_C_INCLUDES := android include include/hamlib lib src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
