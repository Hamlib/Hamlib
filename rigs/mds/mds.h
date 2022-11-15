#define MDS_DATA_LEN 256
#define MDS_RET_LEN 256
struct mds_priv_data {
    char cmd_str[MDS_DATA_LEN];       /* command string buffer */
    char ret_data[MDS_RET_LEN];       /* returned data--max value, most are less */
};

extern const struct rig_caps barrett_caps;

