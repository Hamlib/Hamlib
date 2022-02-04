LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := ft100.c ft747.c ft817.c ft847.c ft890.c ft900.c ft920.c \
		ft1000mp.c ft857.c ft897.c ft990.c frg8800.c \
		ft757gx.c ft736.c frg100.c frg9600.c ft1000d.c \
		vr5000.c ft767gx.c ft840.c ft980.c vx1700.c \
		newcat.c ft450.c ft950.c ft2000.c ft9000.c ft5000.c \
		ft1200.c ft991.c ft600.c ft3000.c ftdx101.c ftdx101mp.c \
	       	ft891.c ftdx10.c \
		yaesu.c

LOCAL_MODULE := yaesu

LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
