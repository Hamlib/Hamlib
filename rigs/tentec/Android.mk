LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := rx320.c rx340.c rx350.c rx331.c \
		pegasus.c argonaut.c orion.c jupiter.c omnivii.c paragon.c \
		tentec.c tentec2.c tt550.c
LOCAL_MODULE := tentec

LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
