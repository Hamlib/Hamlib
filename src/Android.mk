LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        ../android/ltdl.c \
        rig.c \
        serial.c \
        misc.c \
        register.c \
        event.c \
        cal.c \
        conf.c \
        tones.c \
        rotator.c \
        locator.c \
        rot_reg.c \
        rot_conf.c \
        iofunc.c \
        ext.c \
        mem.c \
        settings.c \
        parallel.c \
        debug.c \
        network.c \
        sleep.c \
        gpio.c \
        microham.c \
        rot_ext.c \
        cm108.c \
        sprintflst.c


LOCAL_MODULE := libhamlib
LOCAL_CFLAGS := 
LOCAL_C_INCLUDES := android include
LOCAL_STATIC_LIBRARIES := adat alinco amsat aor ars barrett celestron cnctrk \
        dorji drake dummy easycomm elad ether6 flexradio fodtrack \
        gs232a heathkit icmarine icom ioptron jrc kachina kenwood kit \
        lowe m2 meade pcr prm80 prosistel racal rft \
        rotorez rs sartek satel skanti spid tapr tentec ts7400 tuner \
        uniden wj yaesu radant androidsensor

LOCAL_LDLIBS := -llog -landroid

include $(BUILD_SHARED_LIBRARY)
