LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        ../android/ltdl.c \
	rig.c serial.c misc.c register.c event.c \
	cal.c conf.c tones.c rotator.c locator.c rot_reg.c \
	rot_conf.c rot_settings.c rot_ext.c iofunc.c iofunc.h ext.c \
	mem.c settings.c parallel.c usb_port.c debug.c \
	network.c cm108.c gpio.c microham.c amplifier.c amp_reg.c amp_conf.c \
	amp_settings.c amp_ext.c sleep.c sprintflst.c \
	cache.c snapshot_data.c fifo.c


LOCAL_MODULE := libhamlib
LOCAL_CFLAGS := -DIN_HAMLIB
LOCAL_C_INCLUDES := android include lib

# To find all the available libraries, run: find {rigs,rotators,amplifiers} -type f -name Android.mk -printf '%h\n' | awk -F/ '{print $NF}' | sort -u | tr '\n' ' '
LOCAL_STATIC_LIBRARIES := \
	misc \
	adat alinco amsat androidsensor anytone aor apex ars barrett celestron \
	cnctrk codan commradio dorji drake dummy easycomm elad elecraft ether6 \
	expert flexradio flir fodtrack gemini gomspace grbltrk guohetec gs232a harris \
	heathkit icmarine icom ioptron jrc kachina kenwood kit lowe m2 mds \
	meade motorola pcr prm80 prosistel racal radant rft rotorez rs saebrtrack \
	sartek satel simplecat skanti skywatcher spid tapr tentec ts7400 tuner \
	uniden winradio wj yaesu

LOCAL_LDLIBS := -llog -landroid

include $(BUILD_SHARED_LIBRARY)
