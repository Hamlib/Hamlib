/*
 *  Hamlib CI-V backend - main header
 *  Copyright (c) 2000-2016 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _ICOM_H
#define _ICOM_H 1

#include "hamlib/rig.h"
#include "cal.h"
#include "tones.h"
#include "idx_builtin.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#define BACKEND_VER "20230109"

#define ICOM_IS_ID31 rig_is_model(rig, RIG_MODEL_ID31)
#define ICOM_IS_ID51 rig_is_model(rig, RIG_MODEL_ID51)
#define ICOM_IS_ID4100 rig_is_model(rig, RIG_MODEL_ID4100)
#define ICOM_IS_ID5100 rig_is_model(rig, RIG_MODEL_ID5100)

#define ICOM_IS_SECONDARY_VFO(vfo) ((vfo) & (RIG_VFO_B | RIG_VFO_SUB | RIG_VFO_SUB_B | RIG_VFO_MAIN_B))
#define ICOM_GET_VFO_NUMBER(vfo) (ICOM_IS_SECONDARY_VFO(vfo) ? 0x01 : 0x00)

#define ICOM_MAX_SPECTRUM_FREQ_RANGES 20

/*
 * defines used by comp_cal_str in rig.c
 * STR_CAL_LENGTH is the length of the S Meter calibration table
 * STR_CAL_S0 is the value in dB of the lowest value (not even in table)
 * MULTIB_SUBCMD allows the dsp rigs ie pro models to use multibyte subcommands for all the extra
 * parameters and levels.
 */
#define STR_CAL_LENGTH 16
#define STR_CAL_S0 -54
#define MULTIB_SUBCMD

/*
 * minimal channel caps.
 * If your rig has better/lesser, don't modify this define but clone it,
 * so you don't change other rigs.
 */
#define IC_MIN_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
}

/*
 * common channel caps.
 * If your rig has better/lesser, don't modify this define but clone it,
 * so you don't change other rigs.
 */
#define IC_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .tx_freq = 1,   \
    .tx_mode = 1,   \
    .tx_width = 1,  \
    .rptr_offs = 1, \
}

/*
 * S-Meter data for uncalibrated rigs
 */
#define UNKNOWN_IC_STR_CAL { 2, {{ 0, -60}, { 255, 60}} }

struct ts_sc_list
{
    shortfreq_t ts; /* tuning step */
    unsigned char sc;   /* sub command */
};

/**
 * \brief Pipelined tuning state data structure.
 */
typedef struct rig_pltstate
{
    freq_t freq;
    freq_t next_freq;
    rmode_t mode;
    rmode_t next_mode;
    pbwidth_t width;
    pbwidth_t next_width;
    struct timeval timer_start;
    struct timeval timer_current;
    int usleep_time; /* dependent on radio module & serial data rate */
} pltstate_t;

/**
 * \brief Mappings between Hamlib and Icom AGC levels
 */
struct icom_agc_level
{
    enum agc_level_e
    level; /*!< Hamlib AGC level from agc_level_e enum, the last entry should have level -1 */
    unsigned char
    icom_level; /*!< Icom AGC level for C_CTL_FUNC (0x16), S_FUNC_AGC (0x12) command */
};

typedef enum
{
    CMD_PARAM_TYPE_NONE,
    CMD_PARAM_TYPE_LEVEL,
    CMD_PARAM_TYPE_PARM,
    CMD_PARAM_TYPE_TOKEN,
    CMD_PARAM_TYPE_FUNC,
} cmd_param_t;

/**
 * \brief Lookup table item for Icom levels & parms
 */
struct cmdparams
{
    union
    {
        setting_t s;    /*!< Level or parm */
        token_t t;      /*!< TOKEN_BACKEND */
    } id;
    cmd_param_t cmdparamtype;  /*!< CMD_PARAM_TYPE_LEVEL or CMD_PARAM_TYPE_PARM */
    int command;        /*!< CI-V command */
    int subcmd;         /*!< CI-V Subcommand */
    int submod;         /*!< Subcommand modifier */
    int sublen;         /*!< Number of bytes for subcommand extension */
    unsigned char subext[4]; /*!< Subcommand extension bytes */
    int dattyp;         /*!< Data type conversion */
    int datlen;         /*!< Number of data bytes in frame */
};

/**
 * \brief Icom-specific spectrum scope capabilities, if supported by the rig.
 */
struct icom_spectrum_scope_caps
{
    int spectrum_line_length; /*!< Number of bytes in a complete spectrum scope line */
    int single_frame_data_length; /*!< Number of bytes of specrtum data in a single CI-V frame when the data split to multiple frames */
    int data_level_min; /*!<  */
    int data_level_max;
    double signal_strength_min;
    double signal_strength_max;
};

/**
 * \brief Icom spectrum scope edge frequencies, if supported by the rig.
 *
 * Last entry should have zeros in all fields.
 */
struct icom_spectrum_edge_frequency_range
{
    int range_id; /*!< ID of the range, as specified in the Icom CI-V manuals. First range ID is 1. */
    freq_t low_freq; /*!< The low edge frequency if the range in Hz */
    freq_t high_freq; /*!< The high edge frequency if the range in Hz */
};

/**
 * \brief Cached Icom spectrum scope data.
 *
 * This data is used to store data for current line of spectrum data as it is being received from the rig.
 * Caching the data is necessary for handling spectrum scope data split in multiple CI-V frames.
 */
struct icom_spectrum_scope_cache
{
    int id; /*!< Numeric ID of the spectrum scope data stream identifying the VFO/receiver. First ID is zero. Icom rigs with multiple scopes have IDs: 0 = Main, 1 = Sub. */
    int spectrum_metadata_valid; /*!< Boolean value to track validity of the cached data for spectrum scope. */
    enum rig_spectrum_mode_e spectrum_mode; /*!< The spectrum mode of the current spectrum scope line being received. */
    freq_t spectrum_center_freq; /*!< The center frequency of the current spectrum scope line being received */
    freq_t spectrum_span_freq; /*!< The frequency span of the current spectrum scope line being received */
    freq_t spectrum_low_edge_freq; /*!< The low edge frequency of the current spectrum scope line being received */
    freq_t spectrum_high_edge_freq; /*!< The high edge frequency of the current spectrum scope line being received */
    size_t spectrum_data_length;     /*!< Number of bytes of 8-bit spectrum data in the data buffer. The amount of data may vary if the rig has multiple spectrum scopes, depending on the scope. */
    unsigned char *spectrum_data; /*!< Dynamically allocated buffer for raw spectrum data */
};

struct icom_priv_caps
{
    unsigned char re_civ_addr;  /*!< The remote equipment's default CI-V address */
    int civ_731_mode; /*!< Off: freqs on 10 digits, On: freqs on 8 digits plus passband setting */
    // According to the CI-V+ manual the IC-781, IC-R9000, and IC-R7000 can select pas$
    // The other rigs listed apparently cannot and may need the civ_731_mode=1 which are
    // 1-706
    // 2-706MKII
    // 3-706MKIIG
    // 4-707
    // 5-718
    // 6-746
    // 7-746PRO
    // 8-756
    // 9-756PRO
    // 10-756PROII
    // 11-820H
    // 12-821H
    // 13-910H
    // 14-R10
    // 15-R8500
    // 16-703
    // 17-7800

    int no_xchg; /*!< Off: use VFO XCHG to set other VFO, On: use set VFO to set other VFO */
    const struct ts_sc_list *ts_sc_list;
    // the 4 elements above are mandatory
    // everything below here is optional in the backends
    int settle_time; /*!< Receiver settle time, in ms */
    int (*r2i_mode)(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                    unsigned char *md, signed char *pd); /*!< backend specific code
                               to convert bandwidth and
                               mode to cmd tokens */
    void (*i2r_mode)(RIG *rig, unsigned char md, int pd,
                     rmode_t *mode, pbwidth_t *width);    /*!< backend specific code
                               to convert response
                               tokens to bandwidth and
                               mode */
    int antack_len;             /*!< Length of 0x12 cmd may be 3 or 4 bytes as of 2020-01-22 e.g. 7851 */
    int ant_count;              /*!< Number of antennas */
    int serial_full_duplex;     /*!< Whether RXD&TXD are not tied together */
    int offs_len;               /*!< Number of bytes in offset frequency field. 0 defaults to 3 */
    int serial_USB_echo_check;  /*!< Flag to test USB echo state */
    int agc_levels_present;     /*!< Flag to indicate that agc_levels array is populated */
    struct icom_agc_level agc_levels[HAMLIB_MAX_AGC_LEVELS + 1]; /*!< Icom rig-specific AGC levels, the last entry should have level -1 */
    struct icom_spectrum_scope_caps spectrum_scope_caps; /*!< Icom spectrum scope capabilities, if supported by the rig. Main/Sub scopes in Icom rigs have the same caps. */
    struct icom_spectrum_edge_frequency_range spectrum_edge_frequency_ranges[ICOM_MAX_SPECTRUM_FREQ_RANGES]; /*!< Icom spectrum scope edge frequencies, if supported by the rig. Last entry should have zeros in all fields. */
    struct cmdparams *extcmds;  /*!< Pointer to extended operations array */
};

struct icom_priv_data
{
    unsigned char re_civ_addr;  /*!< The remote equipment's CI-V address */
    int civ_731_mode; /*!< Off: freqs on 10 digits, On: freqs on 8 digits */
    int no_xchg; /*!< Off: use VFO XCHG to set other VFO, On: use set VFO to set other VFO */
    int no_1a_03_cmd; /*!< Rig doesn't tell IF widths */
    int split_on; /*!< Record split state */
    pltstate_t *pltstate; /*!< Only on optoscan */
    int serial_USB_echo_off; /*!< USB is not set to echo */
    /* we track vfos internally for use with different functions like split */
    /* this allows queries using CURR_VFO and Main/Sub to behave */
    vfo_t rx_vfo;
    vfo_t tx_vfo;
    freq_t curr_freq; /*!< Our current freq depending on which vfo is selected */
    freq_t main_freq; /*!< Track last setting of main -- not being used yet */
    freq_t sub_freq;  /*!< Track last setting of sub -- not being used yet */
    freq_t maina_freq;
    freq_t mainb_freq;
    freq_t suba_freq;
    freq_t subb_freq;
    freq_t vfoa_freq; /*!< Track last setting of vfoa -- used to return last freq when ptt is asserted */
    freq_t vfob_freq; /*!< Track last setting of vfob -- used to return last freq when ptt is asserted */
    int x25cmdfails; /*!< This will get set if the 0x25 command fails so we try just once */
    int x26cmdfails; /*!< This will get set if the 0x26 command fails so we try just once */
    int x1cx03cmdfails; /*!< This will get set if the 0x1c 0x03 command fails so we try just once */
    int poweron; /*!< To prevent powering on more than once */
    unsigned char filter; /*!< Current filter selected */
    unsigned char datamode; /*!< Current datamode */
    int spectrum_scope_count; /*!< Number of spectrum scopes, calculated from caps */
    struct icom_spectrum_scope_cache spectrum_scope_cache[HAMLIB_MAX_SPECTRUM_SCOPES]; /*!< Cached Icom spectrum scope data used during reception of the data. The array index must match the scope ID. */
    freq_t other_freq; /*!< Our other freq depending on which vfo is selected */
    int vfo_flag; // used to skip vfo check when frequencies are equal
};

extern const struct ts_sc_list r8500_ts_sc_list[];
extern const struct ts_sc_list r8600_ts_sc_list[];
extern const struct ts_sc_list ic737_ts_sc_list[];
extern const struct ts_sc_list r75_ts_sc_list[];
extern const struct ts_sc_list r7100_ts_sc_list[];
extern const struct ts_sc_list r9000_ts_sc_list[];
extern const struct ts_sc_list r9500_ts_sc_list[];
extern const struct ts_sc_list ic756_ts_sc_list[];
extern const struct ts_sc_list ic756pro_ts_sc_list[];
extern const struct ts_sc_list ic705_ts_sc_list[];
extern const struct ts_sc_list ic706_ts_sc_list[];
extern const struct ts_sc_list ic7000_ts_sc_list[];
extern const struct ts_sc_list ic7100_ts_sc_list[];
extern const struct ts_sc_list ic7200_ts_sc_list[];
extern const struct ts_sc_list ic7300_ts_sc_list[];
extern const struct ts_sc_list ic9700_ts_sc_list[];
extern const struct ts_sc_list ic910_ts_sc_list[];
extern const struct ts_sc_list ic718_ts_sc_list[];
extern const struct ts_sc_list x108g_ts_sc_list[];

extern const pbwidth_t
rtty_fil[];  /* rtty filter passband width; available on 746pro and 756pro rigs */

pbwidth_t icom_get_dsp_flt(RIG *rig, rmode_t mode);

int icom_init(RIG *rig);
int icom_rig_open(RIG *rig);
int icom_rig_close(RIG *rig);
int icom_cleanup(RIG *rig);
int icom_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int icom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int icom_get_rit_new(RIG *rig, vfo_t vfo, shortfreq_t *ts);
int icom_set_rit_new(RIG *rig, vfo_t vfo, shortfreq_t ts);
int icom_set_xit_new(RIG *rig, vfo_t vfo, shortfreq_t ts);
int icom_set_mode_with_data(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int icom_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int icom_get_mode_with_data(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width);
int icom_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
#if 0 // see icom_get_vfo in icom.c
int icom_get_vfo(RIG *rig, vfo_t *vfo);
#else
#define icom_get_vfo NULL
#endif
int icom_set_vfo(RIG *rig, vfo_t vfo);
int icom_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
int icom_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift);
int icom_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs);
int icom_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs);
int icom_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int icom_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int icom_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                        pbwidth_t tx_width);
int icom_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                        pbwidth_t *tx_width);
int icom_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t tx_freq,
                             rmode_t tx_mode, pbwidth_t tx_width);
int icom_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *tx_freq,
                             rmode_t *tx_mode, pbwidth_t *tx_width);
int icom_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
int icom_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
int icom_mem_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
int icom_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
int icom_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
int icom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int icom_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int icom_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int icom_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int icom_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
int icom_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
int icom_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
int icom_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
int icom_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code);
int icom_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
int icom_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code);
int icom_set_bank(RIG *rig, vfo_t vfo, int bank);
int icom_set_mem(RIG *rig, vfo_t vfo, int ch);
int icom_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int icom_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
int icom_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int icom_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int icom_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val);
int icom_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val);
int icom_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int icom_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int icom_set_ext_func(RIG *rig, vfo_t vfo, token_t token, int status);
int icom_get_ext_func(RIG *rig, vfo_t vfo, token_t token, int *status);
int icom_set_parm(RIG *rig, setting_t parm, value_t val);
int icom_get_parm(RIG *rig, setting_t parm, value_t *val);
int icom_set_ext_parm(RIG *rig, token_t token, value_t val);
int icom_get_ext_parm(RIG *rig, token_t token, value_t *val);
int icom_set_ext_cmd(RIG *rig, vfo_t vfo, token_t token, value_t val);
int icom_get_ext_cmd(RIG *rig, vfo_t vfo, token_t token, value_t *val);
int icom_set_conf(RIG *rig, token_t token, const char *val);
int icom_get_conf(RIG *rig, token_t token, char *val);
int icom_set_powerstat(RIG *rig, powerstat_t status);
int icom_get_powerstat(RIG *rig, powerstat_t *status);
int icom_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);
int icom_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                 ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);
int icom_decode_event(RIG *rig);
int icom_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq,
                  rmode_t mode);
int icom_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq,
                  rmode_t mode);
int icom_send_morse(RIG *rig, vfo_t vfo, const char *msg);
int icom_stop_morse(RIG *rig, vfo_t vfo);
int icom_send_voice_mem(RIG *rig, vfo_t vfo, int bank);
/* Exposed routines */
int icom_get_split_vfos(RIG *rig, vfo_t *rx_vfo, vfo_t *tx_vfo);
int icom_set_raw(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                 unsigned char *subcmdbuf, int val_bytes, int val);
int icom_get_raw_buf(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                     unsigned char *subcmdbuf, int *reslen, unsigned char *res);
int icom_get_raw(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                 unsigned char *subcmdbuf, int *val);
int icom_set_level_raw(RIG *rig, setting_t level, int cmd, int subcmd,
                       int subcmdbuflen, unsigned char *subcmdbuf, int val_bytes, value_t val);
int icom_get_level_raw(RIG *rig, setting_t level, int cmd, int subcmd,
                       int subcmdbuflen, unsigned char *subcmdbuf, value_t *val);
int icom_set_custom_parm(RIG *rig, int parmbuflen, unsigned char *parmbuf,
                         int val_bytes, int value);
int icom_get_custom_parm(RIG *rig, int parmbuflen, unsigned char *parmbuf,
                         int *value);
int icom_set_custom_parm_time(RIG *rig, int parmbuflen, unsigned char *parmbuf,
                              int seconds);
int icom_get_custom_parm_time(RIG *rig, int parmbuflen, unsigned char *parmbuf,
                              int *seconds);
int icom_get_freq_range(RIG *rig);
int icom_is_async_frame(RIG *rig, size_t frame_length, const unsigned char *frame);
int icom_process_async_frame(RIG *rig, size_t frame_length, const unsigned char *frame);
int icom_read_frame_direct(RIG *rig, size_t buffer_length, const unsigned char *buffer);

extern const struct confparams icom_cfg_params[];
extern const struct confparams icom_ext_levels[];
extern const struct confparams icom_ext_funcs[];
extern const struct confparams icom_ext_parms[];
extern const struct cmdparams icom_ext_cmds[];

extern const struct rig_caps ic703_caps;
extern const struct rig_caps ic705_caps;
extern const struct rig_caps ic706_caps;
extern const struct rig_caps ic706mkii_caps;
extern const struct rig_caps ic706mkiig_caps;
extern const struct rig_caps ic707_caps;
extern const struct rig_caps ic718_caps;
extern const struct rig_caps ic725_caps;
extern const struct rig_caps ic726_caps;
extern const struct rig_caps ic728_caps;
extern const struct rig_caps ic729_caps;
extern const struct rig_caps ic735_caps;
extern const struct rig_caps ic736_caps;
extern const struct rig_caps ic737_caps;
extern const struct rig_caps ic738_caps;
extern const struct rig_caps ic746_caps;
extern const struct rig_caps ic7410_caps;
extern const struct rig_caps ic746pro_caps;
extern const struct rig_caps ic756_caps;
extern const struct rig_caps ic756pro_caps;
extern const struct rig_caps ic756pro2_caps;
extern const struct rig_caps ic756pro3_caps;
extern const struct rig_caps ic751_caps;
extern const struct rig_caps ic7600_caps;
extern const struct rig_caps ic7610_caps;
extern const struct rig_caps ic761_caps;
extern const struct rig_caps ic765_caps;
extern const struct rig_caps ic7700_caps;
extern const struct rig_caps ic775_caps;
extern const struct rig_caps ic78_caps;
extern const struct rig_caps ic7800_caps;
extern const struct rig_caps ic785x_caps;
extern const struct rig_caps ic7000_caps;
extern const struct rig_caps ic7100_caps;
extern const struct rig_caps ic7200_caps;
extern const struct rig_caps ic7300_caps;
extern const struct rig_caps ic781_caps;
extern const struct rig_caps ic820h_caps;
extern const struct rig_caps ic821h_caps;
extern const struct rig_caps ic910_caps;
extern const struct rig_caps ic9100_caps;
extern const struct rig_caps ic970_caps;
extern struct rig_caps ic9700_caps;
extern const struct rig_caps icrx7_caps;
extern const struct rig_caps icr10_caps;
extern const struct rig_caps icr20_caps;
extern const struct rig_caps icr6_caps;
extern const struct rig_caps icr71_caps;
extern const struct rig_caps icr72_caps;
extern const struct rig_caps icr75_caps;
extern const struct rig_caps icr7000_caps;
extern const struct rig_caps icr7100_caps;
extern const struct rig_caps icr8500_caps;
extern const struct rig_caps icr9000_caps;
extern const struct rig_caps icr9500_caps;
extern const struct rig_caps ic271_caps;
extern const struct rig_caps ic275_caps;
extern const struct rig_caps ic375_caps;
extern const struct rig_caps ic471_caps;
extern const struct rig_caps ic475_caps;
extern const struct rig_caps ic575_caps;
extern const struct rig_caps ic1275_caps;
extern const struct rig_caps icf8101_caps;

extern const struct rig_caps omnivip_caps;
extern const struct rig_caps delta2_caps;

extern const struct rig_caps os456_caps;
extern const struct rig_caps os535_caps;

extern const struct rig_caps ic92d_caps;
extern const struct rig_caps id1_caps;
extern const struct rig_caps id31_caps;
extern const struct rig_caps id51_caps;
extern const struct rig_caps id4100_caps;
extern const struct rig_caps id5100_caps;
extern const struct rig_caps ic2730_caps;

extern const struct rig_caps perseus_caps;

extern const struct rig_caps x108g_caps;
extern const struct rig_caps x6100_caps;
extern const struct rig_caps g90_caps;
extern const struct rig_caps x5105_caps;

extern const struct rig_caps icr8600_caps;
extern const struct rig_caps icr30_caps;

#endif /* _ICOM_H */
