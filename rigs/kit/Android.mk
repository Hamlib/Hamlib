LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := elektor304.c drt1.c dwt.c usrp.c elektor507.c \
		dds60.c miniVNA.c si570avrusb.c funcube.c fifisdr.c hiqsdr.c \
    pcrotor.c kit.c rs_hfiq.c
LOCAL_MODULE := kit

LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
