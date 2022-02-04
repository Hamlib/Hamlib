LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := ic706.c icr8500.c ic735.c ic775.c ic756.c  \
		ic275.c ic475.c ic1275.c ic820h.c ic821h.c \
		icr7000.c ic910.c ic9100.c ic970.c ic725.c ic737.c ic718.c \
		os535.c os456.c omni.c delta2.c ic92d.c \
		ic736.c ic738.c ic7410.c ic746.c ic703.c ic726.c ic271.c \
		ic765.c ic781.c ic471.c icr9000.c icr9500.c \
		icr10.c icr20.c icr6.c icr71.c icr72.c icr75.c icrx7.c \
		id1.c id5100.c ic2730.c \
		ic707.c ic728.c ic751.c ic761.c \
		ic78.c ic7800.c ic7000.c ic7100.c ic7200.c ic7600.c ic7700.c \
		icom.c frame.c optoscan.c x108g.c perseus.c id4100.c id51.c \
		id31.c icr8600.c ic7300.c ic7610.c icr30.c ic785x.c
LOCAL_MODULE := icom

LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include src
LOCAL_LDLIBS := -lhamlib -Lobj/local/$(TARGET_ARCH_ABI)

include $(BUILD_STATIC_LIBRARY)
