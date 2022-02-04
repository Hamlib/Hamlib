LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := ts850.c ts870s.c ts570.c ts450s.c ts950.c ts50s.c \
		ts790.c ts2000.c k2.c k3.c ts930.c \
		ts680.c ts690.c ts140.c ts480.c trc80.c ts590.c \
		ts440.c ts940.c ts711.c ts811.c r5000.c \
		thd7.c thf7.c thg71.c tmd700.c tmv7.c thf6a.c thd72.c tmd710.c \
		kenwood.c th.c ic10.c elecraft.c transfox.c flex6xxx.c ts990s.c \
		xg3.c thd74.c flex.c pihpsdr.c ts890s.c
LOCAL_MODULE := kenwood

LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
