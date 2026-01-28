/*
 * Hamlib Yaesu backend - FTX-1 Common Function and Level Overrides
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file provides the main dispatcher for set/get func/level operations,
 * routing to the appropriate helper functions in group-specific files.
 *
 * Dispatched Functions (RIG_FUNC_*):
 *   ANF  -> ftx1_filter.c - Auto Notch Filter
 *   MN   -> ftx1_filter.c - Manual Notch
 *   APF  -> ftx1_filter.c - Audio Peak Filter
 *   NB   -> ftx1_noise.c - Noise Blanker (read-only on FTX-1)
 *   NR   -> ftx1_noise.c - Noise Reduction (read-only on FTX-1)
 *   VOX  -> ftx1_tx.c - VOX on/off
 *   MON  -> ftx1_tx.c - Monitor on/off
 *   TUNER -> ftx1_tx.c - Antenna tuner
 *   LOCK -> ftx1_info.c - Lock
 *   TONE -> ftx1_ctcss.c - CTCSS encode
 *   TSQL -> ftx1_ctcss.c - Tone squelch
 *   SBKIN/FBKIN -> ftx1_cw.c - Break-in modes
 *
 * Dispatched Levels (RIG_LEVEL_*):
 *   AF      -> ftx1_audio.c - AF Gain
 *   RF      -> ftx1_audio.c - RF Gain
 *   SQL     -> ftx1_tx.c - Squelch
 *   MICGAIN -> ftx1_audio.c - Mic Gain
 *   RFPOWER -> ftx1_audio.c - TX Power
 *   VOXGAIN -> ftx1_audio.c - VOX Gain
 *   VOXDELAY -> ftx1_audio.c - VOX Delay
 *   AGC     -> ftx1_audio.c - AGC
 *   KEYSPD  -> ftx1_cw.c - Keyer Speed
 *   BKINDL  -> ftx1_cw.c - Break-in Delay
 *   NOTCHF  -> ftx1_filter.c - Notch Frequency
 *   APF     -> ftx1_filter.c - APF level
 *   NB      -> ftx1_noise.c - NB level (read-only)
 *   NR      -> ftx1_noise.c - NR level (read-only)
 *   PREAMP  -> ftx1_preamp.c - Preamp
 *   ATT     -> ftx1_preamp.c - Attenuator
 *   STRENGTH/RAWSTR -> ftx1_audio.c - S-meter
 *   SWR/ALC/COMP -> ftx1_audio.c - TX meters
 *
 * Unhandled functions/levels fall through to newcat_set/get_func/level.
 */

#include <stdlib.h>
#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/* Extern helpers from ftx1_filter.c */
extern int ftx1_set_anf_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_anf_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_mn_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_mn_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_apf_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_apf_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_notchf_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_notchf_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_apf_level_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_apf_level_helper(RIG *rig, vfo_t vfo, value_t *val);

/* Extern helpers from ftx1_noise.c */
extern int ftx1_set_nb_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_nb_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_nr_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_nr_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_nb_level_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_nb_level_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_nr_level_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_nr_level_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_na_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_na_helper(RIG *rig, vfo_t vfo, int *status);

/* Extern helpers from ftx1_preamp.c */
extern int ftx1_set_preamp_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_preamp_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_att_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_att_helper(RIG *rig, vfo_t vfo, value_t *val);

/* Extern helpers from ftx1_audio.c */
extern int ftx1_set_af_gain(RIG *rig, vfo_t vfo, float val);
extern int ftx1_get_af_gain(RIG *rig, vfo_t vfo, float *val);
extern int ftx1_set_rf_gain(RIG *rig, vfo_t vfo, float val);
extern int ftx1_get_rf_gain(RIG *rig, vfo_t vfo, float *val);
extern int ftx1_set_mic_gain(RIG *rig, float val);
extern int ftx1_get_mic_gain(RIG *rig, float *val);
extern int ftx1_set_power(RIG *rig, float val);
extern int ftx1_get_power(RIG *rig, float *val);
extern int ftx1_set_vox_gain(RIG *rig, float val);
extern int ftx1_get_vox_gain(RIG *rig, float *val);
extern int ftx1_set_vox_delay(RIG *rig, int ms);
extern int ftx1_get_vox_delay(RIG *rig, int *ms);
extern int ftx1_set_agc(RIG *rig, vfo_t vfo, int val);
extern int ftx1_get_agc(RIG *rig, vfo_t vfo, int *val);
extern int ftx1_get_smeter(RIG *rig, vfo_t vfo, int *val);
extern int ftx1_get_meter(RIG *rig, int meter_type, int *val);
extern int ftx1_set_monitor_level(RIG *rig, vfo_t vfo, float val);
extern int ftx1_get_monitor_level(RIG *rig, vfo_t vfo, float *val);
extern int ftx1_set_amc_output(RIG *rig, float val);
extern int ftx1_get_amc_output(RIG *rig, float *val);
extern int ftx1_set_width(RIG *rig, vfo_t vfo, int width_code);
extern int ftx1_get_width(RIG *rig, vfo_t vfo, int *width_code);
extern int ftx1_set_meter_switch(RIG *rig, vfo_t vfo, int meter_type);
extern int ftx1_get_meter_switch(RIG *rig, vfo_t vfo, int *meter_type);

/* Extern helpers from ftx1_filter.c */
extern int ftx1_set_filter_number(RIG *rig, vfo_t vfo, int filter);
extern int ftx1_get_filter_number(RIG *rig, vfo_t vfo, int *filter);
extern int ftx1_set_contour(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_contour(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_contour_freq(RIG *rig, vfo_t vfo, int freq);
extern int ftx1_get_contour_freq(RIG *rig, vfo_t vfo, int *freq);

/* Extern helpers from ftx1_tx.c */
extern int ftx1_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
extern int ftx1_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
extern int ftx1_set_vox(RIG *rig, int status);
extern int ftx1_get_vox(RIG *rig, int *status);
extern int ftx1_set_monitor(RIG *rig, int status);
extern int ftx1_get_monitor(RIG *rig, int *status);
extern int ftx1_set_tuner(RIG *rig, int mode);
extern int ftx1_get_tuner(RIG *rig, int *mode);
extern int ftx1_set_squelch(RIG *rig, vfo_t vfo, float val);
extern int ftx1_get_squelch(RIG *rig, vfo_t vfo, float *val);
extern int ftx1_set_powerstat(RIG *rig, powerstat_t status);
extern int ftx1_get_powerstat(RIG *rig, powerstat_t *status);

/* Extern helpers from ftx1_cw.c */
extern int ftx1_set_keyer_speed(RIG *rig, int wpm);
extern int ftx1_get_keyer_speed(RIG *rig, int *wpm);
extern int ftx1_set_cw_pitch(RIG *rig, int val);
extern int ftx1_get_cw_pitch(RIG *rig, int *val);
extern int ftx1_set_keyer(RIG *rig, int enable);
extern int ftx1_get_keyer(RIG *rig, int *enable);
extern int ftx1_set_cw_delay(RIG *rig, int ms);
extern int ftx1_get_cw_delay(RIG *rig, int *ms);
extern int ftx1_send_morse(RIG *rig, vfo_t vfo, const char *msg);
extern int ftx1_stop_morse(RIG *rig, vfo_t vfo);
extern int ftx1_wait_morse(RIG *rig, vfo_t vfo);

/* Extern helpers from ftx1_tx.c */
extern int ftx1_set_breakin(RIG *rig, int mode);
extern int ftx1_get_breakin(RIG *rig, int *mode);
extern int ftx1_set_processor(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_processor(RIG *rig, vfo_t vfo, int *status);

/* Extern helpers from ftx1_vfo.c */
extern int ftx1_set_dual_receive(RIG *rig, int dual);
extern int ftx1_get_dual_receive(RIG *rig, int *dual);

/* Extern helpers from ftx1_ctcss.c */
extern int ftx1_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
extern int ftx1_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
extern int ftx1_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
extern int ftx1_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
extern int ftx1_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
extern int ftx1_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code);
extern int ftx1_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
extern int ftx1_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code);
extern int ftx1_set_ctcss_mode(RIG *rig, tone_t mode);
extern int ftx1_get_ctcss_mode(RIG *rig, tone_t *mode);

/* Extern helpers from ftx1_clarifier.c */
extern int ftx1_set_rx_clar(RIG *rig, vfo_t vfo, shortfreq_t offset);
extern int ftx1_get_rx_clar(RIG *rig, vfo_t vfo, shortfreq_t *offset);
extern int ftx1_set_tx_clar(RIG *rig, vfo_t vfo, shortfreq_t offset);
extern int ftx1_get_tx_clar(RIG *rig, vfo_t vfo, shortfreq_t *offset);

/* Extern helpers from ftx1_info.c */
extern int ftx1_set_trn(RIG *rig, int trn);
extern int ftx1_get_trn(RIG *rig, int *trn);
extern int ftx1_get_info(RIG *rig, char *info, size_t info_len);
extern int ftx1_set_lock(RIG *rig, int lock);
extern int ftx1_get_lock(RIG *rig, int *lock);
extern int ftx1_set_if_shift(RIG *rig, vfo_t vfo, int on, int shift_hz);
extern int ftx1_get_if_shift(RIG *rig, vfo_t vfo, int *on, int *shift_hz);

/* Main override for set_func */
int ftx1_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: func=%s status=%d\n", __func__,
              rig_strfunc(func), status);

    switch (func) {
        case RIG_FUNC_ANF:
            return ftx1_set_anf_helper(rig, vfo, status);
        case RIG_FUNC_MN:
            return ftx1_set_mn_helper(rig, vfo, status);
        case RIG_FUNC_APF:
            return ftx1_set_apf_helper(rig, vfo, status);
        case RIG_FUNC_NB:
            return ftx1_set_nb_helper(rig, vfo, status);
        case RIG_FUNC_NR:
            return ftx1_set_nr_helper(rig, vfo, status);
        case RIG_FUNC_VOX:
            return ftx1_set_vox(rig, status);
        case RIG_FUNC_MON:
            return ftx1_set_monitor(rig, status);
        case RIG_FUNC_TUNER:
            return ftx1_set_tuner(rig, status ? 1 : 0);
        case RIG_FUNC_LOCK:
            return ftx1_set_lock(rig, status);
        case RIG_FUNC_COMP:
            return ftx1_set_processor(rig, vfo, status);
        case RIG_FUNC_SBKIN:
            return ftx1_set_breakin(rig, status ? 1 : 0);
        case RIG_FUNC_FBKIN:
            return ftx1_set_breakin(rig, status ? 2 : 0);
        case RIG_FUNC_TONE:
            return ftx1_set_ctcss_mode(rig, status ? FTX1_CTCSS_MODE_ENC : FTX1_CTCSS_MODE_OFF);
        case RIG_FUNC_TSQL:
            return ftx1_set_ctcss_mode(rig, status ? FTX1_CTCSS_MODE_TSQ : FTX1_CTCSS_MODE_OFF);
        case RIG_FUNC_RIT:
            /* FTX-1: Setting RX CLAR to 0 disables it; to enable, use set_rit with offset */
            if (!status) return ftx1_set_rx_clar(rig, vfo, 0);
            return RIG_OK;  /* Enable is no-op; must use set_rit with offset value */
        case RIG_FUNC_XIT:
            /* FTX-1: Setting TX CLAR to 0 disables it; to enable, use set_xit with offset */
            if (!status) return ftx1_set_tx_clar(rig, vfo, 0);
            return RIG_OK;  /* Enable is no-op; must use set_xit with offset value */
        case RIG_FUNC_DUAL_WATCH:
            /* FTX-1: FR command controls dual/single receive mode */
            return ftx1_set_dual_receive(rig, status);
        /* Note: NA command (narrow) available via ftx1_set_na_helper but no RIG_FUNC_NAR in Hamlib */
        /* Note: Contour (CO command) not exposed as RIG_FUNC_CONTOUR doesn't exist in Hamlib */
        default:
            return newcat_set_func(rig, vfo, func, status);
    }
}

/* Main override for get_func */
int ftx1_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int ret, mode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: func=%s\n", __func__, rig_strfunc(func));

    switch (func) {
        case RIG_FUNC_ANF:
            return ftx1_get_anf_helper(rig, vfo, status);
        case RIG_FUNC_MN:
            return ftx1_get_mn_helper(rig, vfo, status);
        case RIG_FUNC_APF:
            return ftx1_get_apf_helper(rig, vfo, status);
        case RIG_FUNC_NB:
            return ftx1_get_nb_helper(rig, vfo, status);
        case RIG_FUNC_NR:
            return ftx1_get_nr_helper(rig, vfo, status);
        case RIG_FUNC_VOX:
            return ftx1_get_vox(rig, status);
        case RIG_FUNC_MON:
            return ftx1_get_monitor(rig, status);
        case RIG_FUNC_TUNER:
            ret = ftx1_get_tuner(rig, &mode);
            if (ret == RIG_OK) *status = (mode > 0) ? 1 : 0;
            return ret;
        case RIG_FUNC_LOCK:
            return ftx1_get_lock(rig, status);
        case RIG_FUNC_COMP:
            return ftx1_get_processor(rig, vfo, status);
        case RIG_FUNC_SBKIN:
            ret = ftx1_get_breakin(rig, &mode);
            if (ret == RIG_OK) *status = (mode == 1) ? 1 : 0;
            return ret;
        case RIG_FUNC_FBKIN:
            ret = ftx1_get_breakin(rig, &mode);
            if (ret == RIG_OK) *status = (mode == 2) ? 1 : 0;
            return ret;
        case RIG_FUNC_TONE:
            {
                tone_t ctcss_mode;
                ret = ftx1_get_ctcss_mode(rig, &ctcss_mode);
                if (ret == RIG_OK) *status = (ctcss_mode == FTX1_CTCSS_MODE_ENC) ? 1 : 0;
                return ret;
            }
        case RIG_FUNC_TSQL:
            {
                tone_t ctcss_mode;
                ret = ftx1_get_ctcss_mode(rig, &ctcss_mode);
                if (ret == RIG_OK) *status = (ctcss_mode == FTX1_CTCSS_MODE_TSQ) ? 1 : 0;
                return ret;
            }
        case RIG_FUNC_RIT:
            {
                shortfreq_t rx_clar_offset;
                ret = ftx1_get_rx_clar(rig, vfo, &rx_clar_offset);
                if (ret == RIG_OK) *status = (rx_clar_offset != 0) ? 1 : 0;
                return ret;
            }
        case RIG_FUNC_XIT:
            {
                shortfreq_t tx_clar_offset;
                ret = ftx1_get_tx_clar(rig, vfo, &tx_clar_offset);
                if (ret == RIG_OK) *status = (tx_clar_offset != 0) ? 1 : 0;
                return ret;
            }
        case RIG_FUNC_DUAL_WATCH:
            /* FTX-1: FR command controls dual/single receive mode */
            return ftx1_get_dual_receive(rig, status);
        /* Note: NA command (narrow) available via ftx1_get_na_helper but no RIG_FUNC_NAR in Hamlib */
        /* Note: Contour (CO command) not exposed as RIG_FUNC_CONTOUR doesn't exist in Hamlib */
        default:
            return newcat_get_func(rig, vfo, func, status);
    }
}

/* Main override for set_level */
int ftx1_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: level=%s\n", __func__, rig_strlevel(level));

    switch (level) {
        case RIG_LEVEL_AF:
            return ftx1_set_af_gain(rig, vfo, val.f);
        case RIG_LEVEL_RF:
            return ftx1_set_rf_gain(rig, vfo, val.f);
        case RIG_LEVEL_SQL:
            return ftx1_set_squelch(rig, vfo, val.f);
        case RIG_LEVEL_MICGAIN:
            return ftx1_set_mic_gain(rig, val.f);
        case RIG_LEVEL_RFPOWER:
            return ftx1_set_power(rig, val.f);
        case RIG_LEVEL_VOXGAIN:
            return ftx1_set_vox_gain(rig, val.f);
        case RIG_LEVEL_VOXDELAY:
            /* VOXDELAY is val.i in tenths of seconds, FTX-1 uses 00-30 (same units) */
            return ftx1_set_vox_delay(rig, val.i);
        case RIG_LEVEL_AGC:
            return ftx1_set_agc(rig, vfo, val.i);
        case RIG_LEVEL_KEYSPD:
            return ftx1_set_keyer_speed(rig, val.i);
        case RIG_LEVEL_BKINDL:
            return ftx1_set_cw_delay(rig, val.i);
        case RIG_LEVEL_CWPITCH:
            /* KP command: 00-75 maps to 300-1050 Hz (10Hz steps) */
            {
                int pitch_val = (val.i - 300) / 10;
                if (pitch_val < 0) pitch_val = 0;
                if (pitch_val > 75) pitch_val = 75;
                return ftx1_set_cw_pitch(rig, pitch_val);
            }
        case RIG_LEVEL_NOTCHF:
            return ftx1_set_notchf_helper(rig, vfo, val);
        case RIG_LEVEL_APF:
            return ftx1_set_apf_level_helper(rig, vfo, val);
        case RIG_LEVEL_NB:
            return ftx1_set_nb_level_helper(rig, vfo, val);
        case RIG_LEVEL_NR:
            return ftx1_set_nr_level_helper(rig, vfo, val);
        case RIG_LEVEL_PREAMP:
            return ftx1_set_preamp_helper(rig, vfo, val);
        case RIG_LEVEL_ATT:
            return ftx1_set_att_helper(rig, vfo, val);
        case RIG_LEVEL_IF:
            /* FTX-1 IS command: val.i is shift in Hz, turn on if non-zero */
            return ftx1_set_if_shift(rig, vfo, (val.i != 0) ? 1 : 0, val.i);
        case RIG_LEVEL_MONITOR_GAIN:
            return ftx1_set_monitor_level(rig, vfo, val.f);
        case RIG_LEVEL_METER:
            return ftx1_set_meter_switch(rig, vfo, val.i);
        /* Note: PBT_IN/PBT_OUT not supported - FTX-1 has width (SH) and IF shift (IS), not true passband tuning */
        case RIG_LEVEL_ANTIVOX:
            return ftx1_set_amc_output(rig, val.f);
        case RIG_LEVEL_BAND_SELECT:
            return ftx1_set_band_select(rig, vfo, val.i);
        default:
            return newcat_set_level(rig, vfo, level, val);
    }
}

/* Main override for get_level */
int ftx1_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int ret, ival;
    float fval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: level=%s\n", __func__, rig_strlevel(level));

    switch (level) {
        case RIG_LEVEL_AF:
            ret = ftx1_get_af_gain(rig, vfo, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_RF:
            ret = ftx1_get_rf_gain(rig, vfo, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_SQL:
            ret = ftx1_get_squelch(rig, vfo, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_MICGAIN:
            ret = ftx1_get_mic_gain(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_RFPOWER:
            ret = ftx1_get_power(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_VOXGAIN:
            ret = ftx1_get_vox_gain(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_VOXDELAY:
            /* FTX-1 returns 00-30 (tenths of seconds), same as Hamlib VOXDELAY */
            ret = ftx1_get_vox_delay(rig, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_AGC:
            ret = ftx1_get_agc(rig, vfo, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_KEYSPD:
            ret = ftx1_get_keyer_speed(rig, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_BKINDL:
            ret = ftx1_get_cw_delay(rig, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_CWPITCH:
            /* KP command: 00-75 maps to 300-1050 Hz (10Hz steps) */
            ret = ftx1_get_cw_pitch(rig, &ival);
            if (ret == RIG_OK) val->i = 300 + (ival * 10);
            return ret;
        case RIG_LEVEL_NOTCHF:
            return ftx1_get_notchf_helper(rig, vfo, val);
        case RIG_LEVEL_APF:
            return ftx1_get_apf_level_helper(rig, vfo, val);
        case RIG_LEVEL_NB:
            return ftx1_get_nb_level_helper(rig, vfo, val);
        case RIG_LEVEL_NR:
            return ftx1_get_nr_level_helper(rig, vfo, val);
        case RIG_LEVEL_PREAMP:
            return ftx1_get_preamp_helper(rig, vfo, val);
        case RIG_LEVEL_ATT:
            return ftx1_get_att_helper(rig, vfo, val);
        case RIG_LEVEL_STRENGTH:
        case RIG_LEVEL_RAWSTR:
            ret = ftx1_get_smeter(rig, vfo, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_SWR:
            ret = ftx1_get_meter(rig, 4, &ival);  /* 4=SWR */
            if (ret == RIG_OK) val->f = (float)ival / 100.0f;
            return ret;
        case RIG_LEVEL_ALC:
            ret = ftx1_get_meter(rig, 2, &ival);  /* 2=ALC */
            if (ret == RIG_OK) val->f = (float)ival / 100.0f;
            return ret;
        case RIG_LEVEL_COMP:
            ret = ftx1_get_meter(rig, 1, &ival);  /* 1=COMP */
            if (ret == RIG_OK) val->f = (float)ival / 100.0f;
            return ret;
        case RIG_LEVEL_IF:
            {
                int on, shift_hz;
                ret = ftx1_get_if_shift(rig, vfo, &on, &shift_hz);
                if (ret == RIG_OK) val->i = shift_hz;
                return ret;
            }
        case RIG_LEVEL_MONITOR_GAIN:
            ret = ftx1_get_monitor_level(rig, vfo, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_METER:
            ret = ftx1_get_meter_switch(rig, vfo, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        /* Note: PBT_IN/PBT_OUT not supported - see set_level comment */
        case RIG_LEVEL_ANTIVOX:
            ret = ftx1_get_amc_output(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_BAND_SELECT:
            ret = ftx1_get_band_select(rig, vfo, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        default:
            return newcat_get_level(rig, vfo, level, val);
    }
}

/* PTT wrapper */
int ftx1_set_ptt_func(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    return ftx1_set_ptt(rig, vfo, ptt);
}

int ftx1_get_ptt_func(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    return ftx1_get_ptt(rig, vfo, ptt);
}

/* Power stat wrapper */
int ftx1_set_powerstat_func(RIG *rig, powerstat_t status)
{
    return ftx1_set_powerstat(rig, status);
}

int ftx1_get_powerstat_func(RIG *rig, powerstat_t *status)
{
    return ftx1_get_powerstat(rig, status);
}

/* CTCSS tone wrappers */
int ftx1_set_ctcss_tone_func(RIG *rig, vfo_t vfo, tone_t tone)
{
    return ftx1_set_ctcss_tone(rig, vfo, tone);
}

int ftx1_get_ctcss_tone_func(RIG *rig, vfo_t vfo, tone_t *tone)
{
    return ftx1_get_ctcss_tone(rig, vfo, tone);
}

int ftx1_set_ctcss_sql_func(RIG *rig, vfo_t vfo, tone_t tone)
{
    return ftx1_set_ctcss_sql(rig, vfo, tone);
}

int ftx1_get_ctcss_sql_func(RIG *rig, vfo_t vfo, tone_t *tone)
{
    return ftx1_get_ctcss_sql(rig, vfo, tone);
}

/* DCS code wrappers */
int ftx1_set_dcs_code_func(RIG *rig, vfo_t vfo, tone_t code)
{
    return ftx1_set_dcs_code(rig, vfo, code);
}

int ftx1_get_dcs_code_func(RIG *rig, vfo_t vfo, tone_t *code)
{
    return ftx1_get_dcs_code(rig, vfo, code);
}

int ftx1_set_dcs_sql_func(RIG *rig, vfo_t vfo, tone_t code)
{
    return ftx1_set_dcs_sql(rig, vfo, code);
}

int ftx1_get_dcs_sql_func(RIG *rig, vfo_t vfo, tone_t *code)
{
    return ftx1_get_dcs_sql(rig, vfo, code);
}

/* Morse/CW wrappers */
int ftx1_send_morse_func(RIG *rig, vfo_t vfo, const char *msg)
{
    return ftx1_send_morse(rig, vfo, msg);
}

int ftx1_stop_morse_func(RIG *rig, vfo_t vfo)
{
    return ftx1_stop_morse(rig, vfo);
}

int ftx1_wait_morse_func(RIG *rig, vfo_t vfo)
{
    return ftx1_wait_morse(rig, vfo);
}

/* Transceive (AI) mode wrapper */
int ftx1_set_trn_func(RIG *rig, int trn)
{
    return ftx1_set_trn(rig, trn);
}

int ftx1_get_trn_func(RIG *rig, int *trn)
{
    return ftx1_get_trn(rig, trn);
}
