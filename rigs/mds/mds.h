#include <hamlib/rig.h>

#define MDS_DATA_LEN 256
#define MDS_RET_LEN 256

#define MDS_VFOS (RIG_VFO_A)

#define MDS_ALL_MODES (RIG_MODE_USB)

#define MDS_LEVELS (RIG_LEVEL_NONE)


struct mds_priv_data {
    char cmd_str[MDS_DATA_LEN];       /* command string buffer */
    char ret_data[MDS_RET_LEN];       /* returned data--max value, most are less */
};

extern const struct rig_caps mds_4710_caps;
extern const struct rig_caps mds_9710_caps;

int mds_init(RIG *rig);
int mds_open(RIG *rig);
int mds_cleanup(RIG *rig);
int mds_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int mds_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int mds_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int mds_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
const char *mds_get_info(RIG *rig);
