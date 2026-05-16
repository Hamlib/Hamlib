LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := ft100.c ft747.c ft817.c ft847.c ft890.c ft900.c \
	ft920.c ft1000mp.c ft857.c ft897.c ft990.c ft990v12.c frg8800.c \
	ft757gx.c ft600.c ft736.c frg100.c frg9600.c ft1000d.c vr5000.c \
	ft767gx.c ft840.c ft980.c vx1700.c ft710.c newcat.c ft450.c ft950.c \
	ft991.c ft2000.c ft9000.c ft5000.c ft1200.c ft891.c ftdx101.c \
	ftdx101mp.c ft3000.c ftdx10.c ftx1/ftx1.c ftx1/ftx1_vfo.c ftx1/ftx1_freq.c \
	ftx1/ftx1_mode.c ftx1/ftx1_filter.c ftx1/ftx1_noise.c ftx1/ftx1_func.c \
	ftx1/ftx1_preamp.c ftx1/ftx1_audio.c ftx1/ftx1_tx.c ftx1/ftx1_mem.c \
	ftx1/ftx1_cw.c ftx1/ftx1_ctcss.c ftx1/ftx1_scan.c ftx1/ftx1_info.c \
	ftx1/ftx1_ext.c ftx1/ftx1_clarifier.c ftx1/ftx1_menu.c \
	yaesu.c

LOCAL_MODULE := yaesu

LOCAL_CFLAGS := -DIN_HAMLIB
LOCAL_C_INCLUDES := android include include/hamlib lib src
LOCAL_LDLIBS := -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
