LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := ar8200.c ar8000.c ar5000.c ar3000.c ar7030.c ar3030.c \
	     ar2700.c ar8600.c ar7030p.c ar7030p_utils.c sr2200.c aor.c
LOCAL_MODULE := aor

LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
