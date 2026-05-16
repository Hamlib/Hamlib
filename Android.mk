TOP_PATH := $(call my-dir)

# Link hamlib/config.h to android/config.h, Windows doesn't like symbol link so use preprocessor instead.
$(shell printf "#include \"../../android/config.h\"\n" > $(TOP_PATH)/include/hamlib/config.h)

# Again, Windows shell isn't compatible with the automake rule. Leave it unknown for now.
HAMLIB_DATETIME_H := $(TOP_PATH)/src/hamlibdatetime.h
ifeq ($(wildcard $(HAMLIB_DATETIME_H)),)
$(shell printf "#define HAMLIBDATETIME \"unknown\"\n" > $(HAMLIB_DATETIME_H))
endif

include $(TOP_PATH)/src/Android.mk

include $(TOP_PATH)/lib/Android.mk

# To generate a full list of files, run: find {rigs,rotators,amplifiers} -name Android.mk |  sed 's|^|include $(TOP_PATH)/|'
include $(TOP_PATH)/rigs/adat/Android.mk
include $(TOP_PATH)/rigs/alinco/Android.mk
include $(TOP_PATH)/rigs/anytone/Android.mk
include $(TOP_PATH)/rigs/aor/Android.mk
include $(TOP_PATH)/rigs/barrett/Android.mk
include $(TOP_PATH)/rigs/codan/Android.mk
include $(TOP_PATH)/rigs/commradio/Android.mk
include $(TOP_PATH)/rigs/dorji/Android.mk
include $(TOP_PATH)/rigs/drake/Android.mk
include $(TOP_PATH)/rigs/dummy/Android.mk
include $(TOP_PATH)/rigs/elad/Android.mk
include $(TOP_PATH)/rigs/flexradio/Android.mk
include $(TOP_PATH)/rigs/gomspace/Android.mk
include $(TOP_PATH)/rigs/guohetec/Android.mk
include $(TOP_PATH)/rigs/harris/Android.mk
include $(TOP_PATH)/rigs/icmarine/Android.mk
include $(TOP_PATH)/rigs/icom/Android.mk
include $(TOP_PATH)/rigs/jrc/Android.mk
include $(TOP_PATH)/rigs/kachina/Android.mk
include $(TOP_PATH)/rigs/kenwood/Android.mk
include $(TOP_PATH)/rigs/kit/Android.mk
include $(TOP_PATH)/rigs/lowe/Android.mk
include $(TOP_PATH)/rigs/mds/Android.mk
include $(TOP_PATH)/rigs/motorola/Android.mk
include $(TOP_PATH)/rigs/pcr/Android.mk
include $(TOP_PATH)/rigs/prm80/Android.mk
include $(TOP_PATH)/rigs/racal/Android.mk
include $(TOP_PATH)/rigs/rft/Android.mk
include $(TOP_PATH)/rigs/rs/Android.mk
include $(TOP_PATH)/rigs/simplecat/Android.mk
include $(TOP_PATH)/rigs/skanti/Android.mk
include $(TOP_PATH)/rigs/tapr/Android.mk
include $(TOP_PATH)/rigs/tentec/Android.mk
include $(TOP_PATH)/rigs/tuner/Android.mk
include $(TOP_PATH)/rigs/uniden/Android.mk
include $(TOP_PATH)/rigs/winradio/Android.mk
include $(TOP_PATH)/rigs/wj/Android.mk
include $(TOP_PATH)/rigs/yaesu/Android.mk
include $(TOP_PATH)/rotators/amsat/Android.mk
include $(TOP_PATH)/rotators/androidsensor/Android.mk
include $(TOP_PATH)/rotators/apex/Android.mk
include $(TOP_PATH)/rotators/ars/Android.mk
include $(TOP_PATH)/rotators/celestron/Android.mk
include $(TOP_PATH)/rotators/cnctrk/Android.mk
include $(TOP_PATH)/rotators/easycomm/Android.mk
include $(TOP_PATH)/rotators/ether6/Android.mk
include $(TOP_PATH)/rotators/flir/Android.mk
include $(TOP_PATH)/rotators/fodtrack/Android.mk
include $(TOP_PATH)/rotators/grbltrk/Android.mk
include $(TOP_PATH)/rotators/gs232a/Android.mk
include $(TOP_PATH)/rotators/heathkit/Android.mk
#include $(TOP_PATH)/rotators/indi/Android.mk
include $(TOP_PATH)/rotators/ioptron/Android.mk
include $(TOP_PATH)/rotators/m2/Android.mk
include $(TOP_PATH)/rotators/meade/Android.mk
include $(TOP_PATH)/rotators/prosistel/Android.mk
include $(TOP_PATH)/rotators/radant/Android.mk
include $(TOP_PATH)/rotators/rotorez/Android.mk
include $(TOP_PATH)/rotators/saebrtrack/Android.mk
include $(TOP_PATH)/rotators/sartek/Android.mk
include $(TOP_PATH)/rotators/satel/Android.mk
include $(TOP_PATH)/rotators/skywatcher/Android.mk
include $(TOP_PATH)/rotators/spid/Android.mk
include $(TOP_PATH)/rotators/ts7400/Android.mk
include $(TOP_PATH)/amplifiers/elecraft/Android.mk
include $(TOP_PATH)/amplifiers/expert/Android.mk
include $(TOP_PATH)/amplifiers/gemini/Android.mk

# include $(TOP_PATH)/tests/Android.mk
