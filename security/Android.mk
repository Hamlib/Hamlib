LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        aes.c \
        AESStringCrypt.c \
        hamlib_passwd.c \
        password.c \
        sha256.c.c


LOCAL_MODULE := libsecurity
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
