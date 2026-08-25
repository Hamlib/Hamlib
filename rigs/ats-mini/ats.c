/*
 * Hamlib Mini backend
 * Copyright (c) 2026 by Hamlib Team & DG3BP
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hamlib/config.h"
#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "register.h"
#include "serial.h"

#include "ats.h"

static double p_ats_parse_scaled_value(const char *token) {
    char *endptr = NULL;
    double val = strtod(token, &endptr);
 
    if (endptr && *endptr != '\0') {
        if (*endptr == 'k' || *endptr == 'K') {
            val *= 1000.0;
        } else if (*endptr == 'M') {
            val *= 1000000.0;
        }
    }
 
    return val;
}
 
static int p_ats_write(RIG *rig, const unsigned char *cmd) {
    int res_uart = write_block(RIGPORT(rig), cmd, strlen((const char *)cmd));
    if (res_uart != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: write_block failed: %d\n", __func__, res_uart);
        return RIG_EIO;
    }
    usleep(MINI_UART_CMD_DLY * 1000);
    return res_uart;
}

static int p_ats_read_line(RIG *rig, unsigned char *buff, unsigned int buff_size) {
    unsigned int idx = 0, to = 0;
    char c;

    while (1) {
        int nb_uart = read_block(RIGPORT(rig), (unsigned char *) &c, 1);

        if (to > MINI_UART_TO_CNT) {
            return RIG_ETIMEOUT;
        }
        if (nb_uart <= 0) {
            to++;
            continue;
        }

        if (idx >= (buff_size - 1)) {
            rig_debug(RIG_DEBUG_ERR, "%s: line too long without newline, aborting\n", __func__);
            return RIG_EPROTO;
        }

        buff[idx] = c;
        idx++;
        if (c == '\n') {
            buff[idx] = '\0';
            return RIG_OK;
        }
    }
}

#define MINI_UART_MAX_LINE_RETRIES 4

static int p_ats_update_state(RIG *rig) {
    unsigned char buff[MINI_UART_BUFF_SIZE];
    int res = RIG_OK;
    int attempt;
    unsigned int idx = 0;
    struct ats_mini_mon_data *tmp_mon_data = NULL;
    int have_valid_line = 0;
    int resynced = 0;

    res = p_ats_write(rig, (const unsigned char *)"t");
    if (res != RIG_OK) {
        return res;
    }

    for (attempt = 0; attempt < MINI_UART_MAX_LINE_RETRIES; attempt++) {
        res = p_ats_read_line(rig, buff, MINI_UART_BUFF_SIZE);

        if (res == RIG_ETIMEOUT && !resynced) {
            rig_debug(RIG_DEBUG_WARN, "%s: no monitor data, resyncing toggle\n", __func__);
            resynced = 1;
            res = p_ats_write(rig, (const unsigned char *)"t");
            if (res != RIG_OK) {
                goto cleanup;
            }
            attempt--;
            continue;
        }

        if (res != RIG_OK) {
            break;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s, monitor '%s' (attempt %d)\n", __func__, (char *)buff, attempt + 1);

        tmp_mon_data = (struct ats_mini_mon_data *) calloc(1, sizeof(struct ats_mini_mon_data));
        if (!tmp_mon_data) {
            res = RIG_ENOMEM;
            goto cleanup;
        }

        idx = 0;
        char *token = strtok((char *) buff, ",");
        while (token) {
            token = strtok(0, ",");
            idx++;

            if (!token) {
                break;
            }

            switch (idx) {
                case 1: {
                    tmp_mon_data->freq_khz_raw = strtod(token, NULL);
                    break;
                }
                case 2: {
                    tmp_mon_data->bfo_hz = strtod(token, NULL);
                    break;
                }
                case 4: {
                    strncpy(tmp_mon_data->bandname, token, (MINI_BANDNAME_LEN - 1));
                    tmp_mon_data->bandname[MINI_BANDNAME_LEN - 1] = '\0';
                    break;
                }
                case 5: {
                    if (strcmp(token, "AM") == 0) {
                        tmp_mon_data->mode = RIG_MODE_AM;
                    } else if (strcmp(token, "USB") == 0) {
                        tmp_mon_data->mode = RIG_MODE_USB;
                    } else if (strcmp(token, "LSB") == 0) {
                        tmp_mon_data->mode = RIG_MODE_LSB;
                    } else if (strcmp(token, "FM") == 0) {
                        tmp_mon_data->mode = RIG_MODE_FM;
                    } else {
                        tmp_mon_data->mode = RIG_MODE_NONE;
                    }
                    break;
                }
                case 6: {
                    tmp_mon_data->step = Hz(p_ats_parse_scaled_value(token));
                    break;
                }
                case 7: {
                    tmp_mon_data->bandwidth = Hz(p_ats_parse_scaled_value(token));
                    break;
                }
                case 8: {
                    tmp_mon_data->attenuation = (int) strtol(token, NULL, 10);
                    break;
                }
                case 9: {
                    tmp_mon_data->volume = (int) strtol(token, NULL, 10);
                    break;
                }
                case 10: {
                    tmp_mon_data->RSSI = (int) strtol(token, NULL, 10);
                    break;
                }
                case 11: {
                    tmp_mon_data->SNR = (int) strtol(token, NULL, 10);
                    break;
                }
                case 14: {
                    tmp_mon_data->seq_no = (int) strtol(token, NULL, 10);
                    break;
                }
                default:
                    break;
            }
            if (idx > MINI_UART_MAX_TOKEN) {
                rig_debug(RIG_DEBUG_ERR, "%s: parse failure\n", __func__);
                free(tmp_mon_data);
                tmp_mon_data = NULL;
                res = RIG_EPROTO;
                goto cleanup;
            }
        }

        if (idx >= 14) {
            have_valid_line = 1;
            res = RIG_OK;
            break;
        }

        rig_debug(RIG_DEBUG_WARN, "%s: short/malformed line (only %u fields), reading next line (attempt %d/%d)\n",
                  __func__, idx, attempt + 1, MINI_UART_MAX_LINE_RETRIES);
        free(tmp_mon_data);
        tmp_mon_data = NULL;
        res = RIG_EPROTO;
    }

cleanup:
    {
        int off_res = p_ats_write(rig, (const unsigned char *)"t");
        if (off_res != RIG_OK && res == RIG_OK) {
            res = off_res;
        }
    }

    if (!have_valid_line || !tmp_mon_data) {
        if (tmp_mon_data) {
            free(tmp_mon_data);
        }
        rig_debug(RIG_DEBUG_ERR, "%s: giving up after %d attempt(s), res=%d\n", __func__, attempt + 1, res);
        return (res != RIG_OK) ? res : RIG_EPROTO;
    }

    if (tmp_mon_data->mode == RIG_MODE_FM) {
        tmp_mon_data->freq = Hz(tmp_mon_data->freq_khz_raw * 10000);
    } else {
        tmp_mon_data->freq = Hz(tmp_mon_data->freq_khz_raw * 1000) + (freq_t) tmp_mon_data->bfo_hz;
    }

    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;

    if (tmp_mon_data->seq_no != mon_data->seq_no) {
        mon_data->freq = tmp_mon_data->freq;
        mon_data->freq_khz_raw = tmp_mon_data->freq_khz_raw;
        mon_data->bfo_hz = tmp_mon_data->bfo_hz;
        strncpy(mon_data->bandname, tmp_mon_data->bandname, (MINI_BANDNAME_LEN - 1));
        mon_data->bandname[MINI_BANDNAME_LEN - 1] = '\0';
        mon_data->mode = tmp_mon_data->mode;
        mon_data->step = tmp_mon_data->step;
        mon_data->bandwidth = tmp_mon_data->bandwidth;
        mon_data->attenuation = tmp_mon_data->attenuation;
        mon_data->volume = tmp_mon_data->volume;
        mon_data->RSSI = tmp_mon_data->RSSI;
        mon_data->SNR = tmp_mon_data->SNR;
        mon_data->seq_no = tmp_mon_data->seq_no;
    }

    free(tmp_mon_data);
    return RIG_OK;
}

static int p_ats_set_band(RIG *rig, mini_band_id_t band_id) {
    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;

    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }

    char start_bandname[MINI_BANDNAME_LEN];
    strncpy(start_bandname, mon_data->bandname, (MINI_BANDNAME_LEN - 1));

    rig_debug(RIG_DEBUG_WARN, "%s: start %s, requested %s\n", __func__, start_bandname, mini_band_ids[band_id]);
    while (1) {
        if (strcmp(mon_data->bandname, mini_band_ids[band_id]) == 0) {
            break;
        }

        res = p_ats_write(rig, (const unsigned char *)"B");
        if (res != RIG_OK) {
            return res;
        }

        res = p_ats_update_state(rig);
        if (res != RIG_OK) {
            return res;
        }

        if (strcmp(mon_data->bandname, start_bandname) == 0) {
            rig_debug(RIG_DEBUG_ERR, "%s: CANNOT switch band\n", __func__);
            return RIG_EINVAL;
        }
    };

    return RIG_OK;
}

static int p_ats_set_mode(RIG *rig, rmode_t mode_id) {
    if ((mode_id != RIG_MODE_AM) && (mode_id != RIG_MODE_USB) && (mode_id != RIG_MODE_LSB)
            && ( mode_id != RIG_MODE_FM)) {
        rig_debug(RIG_DEBUG_ERR, "%s: UNSUPPORTED mode\n", __func__);
        return RIG_EINVAL;
    }

    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;

    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }

    rmode_t start_mode_id = mon_data->mode;
    rig_debug(RIG_DEBUG_WARN, "%s: start %" PRIu64 ", requested %" PRIu64 "\n", __func__, start_mode_id, mode_id);
    while (1) {
        if (mon_data->mode == mode_id) {
            break;
        }

        res = p_ats_write(rig, (const unsigned char *)"M");
        if (res != RIG_OK) {
            return res;
        }

        res = p_ats_update_state(rig);
        if (res != RIG_OK) {
            return res;
        }

        if (mon_data->mode == start_mode_id) {
            rig_debug(RIG_DEBUG_ERR, "%s: CANNOT switch mode\n", __func__);
            return RIG_EINVAL;
        }
    };

    return RIG_OK;
}

#define MINI_BW_MATCH_TOLERANCE_HZ 50

static int p_ats_set_bandwidth(RIG *rig, pbwidth_t width) {
    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;

    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }

    freq_t start_bw = mon_data->bandwidth;
    rig_debug(RIG_DEBUG_WARN, "%s: start %.0f Hz, requested %.0f Hz\n",
              __func__, (double) start_bw, (double) width);

    while (1) {
        freq_t diff = mon_data->bandwidth - (freq_t) width;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < MINI_BW_MATCH_TOLERANCE_HZ) {
            break;
        }

        res = p_ats_write(rig, (const unsigned char *)"W");
        if (res != RIG_OK) {
            return res;
        }

        res = p_ats_update_state(rig);
        if (res != RIG_OK) {
            return res;
        }

        freq_t start_diff = mon_data->bandwidth - start_bw;
        if (start_diff < 0) {
            start_diff = -start_diff;
        }
        if (start_diff < MINI_BW_MATCH_TOLERANCE_HZ) {
            rig_debug(RIG_DEBUG_ERR, "%s: CANNOT reach requested bandwidth %.0f Hz "
                      "(cycled back to start %.0f Hz)\n",
                      __func__, (double) width, (double) start_bw);
            return -RIG_EINVAL;
        }
    };

    return RIG_OK;
}

static int p_ats_set_volume(RIG *rig, int target_vol) {
    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;
 
    if (target_vol < MINI_VOLUME_MIN) {
        target_vol = MINI_VOLUME_MIN;
    }
    if (target_vol > MINI_VOLUME_MAX) {
        target_vol = MINI_VOLUME_MAX;
    }
 
    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }
 
    rig_debug(RIG_DEBUG_WARN, "%s: start %d, requested %d\n", __func__, mon_data->volume, target_vol);
 
    int guard = (MINI_VOLUME_MAX - MINI_VOLUME_MIN) + 2;
 
    while (mon_data->volume != target_vol && guard-- > 0) {
        const unsigned char *cmd = (mon_data->volume < target_vol)
                                    ? (const unsigned char *) "V"
                                    : (const unsigned char *) "v";
 
        res = p_ats_write(rig, cmd);
        if (res != RIG_OK) {
            return res;
        }
 
        res = p_ats_update_state(rig);
        if (res != RIG_OK) {
            return res;
        }
    }
 
    if (mon_data->volume != target_vol) {
        rig_debug(RIG_DEBUG_ERR, "%s: CANNOT reach requested volume %d (stuck at %d)\n",
                  __func__, target_vol, mon_data->volume);
        return -RIG_EINVAL;
    }
 
    return RIG_OK;
}

#define MINI_STEP_MATCH_TOLERANCE_HZ 1
 
static int p_ats_set_step(RIG *rig, shortfreq_t ts) {
    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;
 
    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }
 
    freq_t start_step = mon_data->step;
    rig_debug(RIG_DEBUG_WARN, "%s: start %.0f Hz, requested %.0f Hz\n",
              __func__, (double) start_step, (double) ts);
 
    while (1) {
        freq_t diff = mon_data->step - (freq_t) ts;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < MINI_STEP_MATCH_TOLERANCE_HZ) {
            break;
        }
 
        res = p_ats_write(rig, (const unsigned char *) "S");
        if (res != RIG_OK) {
            return res;
        }
 
        res = p_ats_update_state(rig);
        if (res != RIG_OK) {
            return res;
        }
 
        freq_t start_diff = mon_data->step - start_step;
        if (start_diff < 0) {
            start_diff = -start_diff;
        }
        if (start_diff < MINI_STEP_MATCH_TOLERANCE_HZ) {
            rig_debug(RIG_DEBUG_ERR, "%s: CANNOT reach requested step %.0f Hz "
                      "(cycled back to start %.0f Hz) - not available in current mode?\n",
                      __func__, (double) ts, (double) start_step);
            return -RIG_EINVAL;
        }
    }
 
    return RIG_OK;
}

static int ats_mini_init(RIG *rig) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s version %s\n", __func__, rig->caps->version);

    struct ats_mini_mon_data *priv = (struct ats_mini_mon_data *)
                                     calloc(1, sizeof(struct ats_mini_mon_data));
    if (!priv) {
        return -RIG_ENOMEM;
    }

    memset(priv->bandname, 0, MINI_BANDNAME_LEN);
    priv->mode = RIG_MODE_NONE;
    STATE(rig)->priv = priv;

    return RIG_OK;
}

static int ats_mini_cleanup(RIG *rig) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    if (STATE(rig)->priv) {
        free(STATE(rig)->priv);
        STATE(rig)->priv = NULL;
    }

    return RIG_OK;
}

static int ats_mini_open(RIG *rig) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    if (!rig || !STATE(rig)) {
        rig_debug(RIG_DEBUG_ERR, "%s: NULL rig or state\n", __func__);
        return RIG_EARG;
    }

    // rig_debug(RIG_DEBUG_VERBOSE, "%s, flush UART\n", __func__);
    // rig_flush(RIGPORT(rig));

    int res = RIG_OK, nb_uart = -1;
    char c;

    usleep(1000 * 1000); // wait a bit to see if we receive any characters
    nb_uart = read_block(RIGPORT(rig), (unsigned char *) &c, 1);
    rig_debug(RIG_DEBUG_VERBOSE, "%s, read %d\n", __func__, nb_uart);
    if (nb_uart > 0) {
        rig_debug(RIG_DEBUG_WARN, "%s, already in monitor mode\n", __func__);
        while (1) {
            nb_uart = read_block(RIGPORT(rig), (unsigned char *) &c, 1);
            if (nb_uart <= 0) {
                continue;
            }
            if (c == '\n') {
                break;
            }
        }

        rig_debug(RIG_DEBUG_WARN, "%s, stop monitor mode\n", __func__);
        res = p_ats_write(rig, (const unsigned char *) "t");
        if (res != RIG_OK) {
            return res;
        }
    }

    res = p_ats_set_band(rig, MINI_BAND_ALL);
    if (res != RIG_OK) {
        return res;
    }

    return RIG_OK;
}

static int ats_mini_close(RIG *rig) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    return RIG_OK;
}

static const char *ats_mini_info(RIG *rig) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    static char info[128] = "";

    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return info;
    }

    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;

    snprintf(info, sizeof(info), "%s: freq %.3f kHz, mode ",
             STATE(rig)->model_name, (mon_data->freq / 1e3));
    switch (mon_data->mode) {
        case RIG_MODE_AM: {
            strcat(info, "AM");
            break;
        }
        case RIG_MODE_USB: {
            strcat(info, "USB");
            break;
        }
        case RIG_MODE_LSB: {
            strcat(info, "LSB");
            break;
        }
        case RIG_MODE_FM: {
            strcat(info, "FM");
            break;
        }
        default: {
            strcat(info, "???");
            break;
        }
    }
    return info;
}

static int ats_mini_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }

    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;

    int in_fm_range = (freq >= MHz(64) && freq <= MHz(108));
    int in_hf_range = (freq >= kHz(150) && freq <= MHz(30));

    if (!in_fm_range && !in_hf_range) {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: refusing to set %.0f Hz - outside any supported "
                  "receiver range (150 kHz-30 MHz or 64-108 MHz)\n",
                  __func__, (double) freq);
        return -RIG_EINVAL;
    }

    int current_is_fm = (mon_data->mode == RIG_MODE_FM);

    if (in_fm_range != current_is_fm) {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: refusing to set %.0f Hz - would require a band change "
                  "(current band '%s', mode %d) which this backend does not "
                  "perform automatically; change the band on the radio first\n",
                  __func__, (double) freq, mon_data->bandname, (int) mon_data->mode);
        return -RIG_EINVAL;
    }

    char cmd[20] = "";
    sprintf(cmd, "F%d\r\n", (int) freq);
    res = p_ats_write(rig, (const unsigned char *)cmd);
    if (res != RIG_OK) {
        return res;
    }

    return RIG_OK;
}

static int ats_mini_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }

    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;
    *freq = mon_data->freq;

    return RIG_OK;
}

static int ats_mini_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    int res = p_ats_set_mode(rig, mode);
    if (res != RIG_OK) {
        return res;
    }

    if (width > 0) {
        res = p_ats_set_bandwidth(rig, width);
        if (res != RIG_OK) {
            return res;
        }
    }

    return RIG_OK;
}

static int ats_mini_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }

    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;
    *mode = mon_data->mode;
    *width = (pbwidth_t) mon_data->bandwidth;

    return RIG_OK;
}

static int ats_mini_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);
 
    if (level != RIG_LEVEL_AF) {
        return -RIG_EINVAL;
    }
 
    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }
 
    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;
    val->f = (float) (mon_data->volume - MINI_VOLUME_MIN) /
             (float) (MINI_VOLUME_MAX - MINI_VOLUME_MIN);
 
    return RIG_OK;
}
 
static int ats_mini_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);
 
    if (level != RIG_LEVEL_AF) {
        return -RIG_EINVAL;
    }
 
    int target_vol = MINI_VOLUME_MIN +
                      (int) (val.f * (MINI_VOLUME_MAX - MINI_VOLUME_MIN) + 0.5f);
 
    return p_ats_set_volume(rig, target_vol);
}

static int ats_mini_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);
 
    return p_ats_set_step(rig, ts);
}
 
static int ats_mini_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);
 
    int res = p_ats_update_state(rig);
    if (res != RIG_OK) {
        return res;
    }
 
    struct ats_mini_mon_data *mon_data = STATE(rig)->priv;
    *ts = (shortfreq_t) mon_data->step;
 
    return RIG_OK;
}

struct rig_caps ats_mini_caps = {
    RIG_MODEL(RIG_MODEL_ATS_MINI),
    .model_name = "ATS Mini",
    .mfg_name = "AMNVOLT",
    .version = "20260701.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_ALPHA,
    .rig_type = RIG_TYPE_RECEIVER,
    .ptt_type = RIG_PTT_NONE,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 115200,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 0,
    .retry = 0,

    .has_get_func = RIG_FUNC_NONE,
    .has_set_func = RIG_FUNC_NONE,
    .has_get_level = RIG_LEVEL_AF,
    .has_set_level = RIG_LEVEL_AF,
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .level_gran = {},
    .parm_gran = {},
    .ctcss_list = NULL,
    .dcs_list = NULL,
    .preamp =
    {
        RIG_DBLST_END,
    },
    .attenuator =
    {
        RIG_DBLST_END,
    },
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
    .targetable_vfo = 0,
    .transceive = RIG_TRN_OFF,
    .bank_qty = 0,
    .chan_desc_sz = 0,
    .chan_list =
    {
        RIG_CHAN_END,
    },

    .rx_range_list1 =
    {
        {
            .startf = kHz(150),
            .endf = MHz(30),
            .modes = RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_AM,
            .low_power = -1,
            .high_power = -1,
            .vfo = RIG_VFO_A,
            .ant = RIG_ANT_1,
            .label = "ATS Mini V4 RX Range 1"
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 =
    {
        {
            .startf = MHz(64),
            .endf = MHz(108),
            .modes = RIG_MODE_FM,
            .low_power = -1,
            .high_power = -1,
            .vfo = RIG_VFO_A,
            .ant = RIG_ANT_1,
            .label = "ATS Mini V4 RX Range 2"
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =
    {
        RIG_FRNG_END,
    },
    .tuning_steps =
    {
        {RIG_MODE_AM, Hz(1000)},
        {RIG_MODE_AM, Hz(5000)},
        {RIG_MODE_AM, Hz(9000)},
        {RIG_MODE_AM | RIG_MODE_FM, Hz(10000)},
        {RIG_MODE_AM | RIG_MODE_FM, Hz(50000)},
        {RIG_MODE_AM | RIG_MODE_FM, Hz(100000)},
        {RIG_MODE_AM | RIG_MODE_FM, Hz(1000000)},
        {RIG_MODE_FM, Hz(200000)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(10)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(25)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(50)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(100)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(500)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(1000)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(5000)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(9000)},
        {RIG_MODE_USB | RIG_MODE_LSB, Hz(10000)},
        RIG_TS_END,
    },
    .filters =
    {
        {RIG_MODE_USB, Hz(500)},
        {RIG_MODE_USB, Hz(1000)},
        {RIG_MODE_USB, Hz(1200)},
        {RIG_MODE_USB, Hz(2200)},
        {RIG_MODE_USB, Hz(3000)},
        {RIG_MODE_USB, Hz(4000)},
        {RIG_MODE_LSB, Hz(500)},
        {RIG_MODE_LSB, Hz(1000)},
        {RIG_MODE_LSB, Hz(1200)},
        {RIG_MODE_LSB, Hz(2200)},
        {RIG_MODE_LSB, Hz(3000)},
        {RIG_MODE_LSB, Hz(4000)},
        {RIG_MODE_AM, Hz(1000)},
        {RIG_MODE_AM, Hz(1800)},
        {RIG_MODE_AM, Hz(2500)},
        {RIG_MODE_AM, Hz(3000)},
        {RIG_MODE_AM, Hz(4000)},
        {RIG_MODE_AM, Hz(6000)},
        {RIG_MODE_FM, Hz(40000)},
        {RIG_MODE_FM, Hz(60000)},
        {RIG_MODE_FM, Hz(84000)},
        {RIG_MODE_FM, Hz(110000)},
        RIG_FLT_END,
    },

    .rig_init = ats_mini_init,
    .rig_cleanup = ats_mini_cleanup,
    .rig_open = ats_mini_open,
    .rig_close = ats_mini_close,

    .get_info = ats_mini_info,
    .get_freq = ats_mini_get_freq,
    .set_freq = ats_mini_set_freq,
    .get_level = ats_mini_get_level,
    .set_level = ats_mini_set_level,
    .set_ts = ats_mini_set_ts,
    .get_ts = ats_mini_get_ts, 
    .get_mode = ats_mini_get_mode,
    .set_mode = ats_mini_set_mode,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS,
};

DECLARE_INITRIG_BACKEND(atsmini) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s: backend start\n", __func__);

    rig_register(&ats_mini_caps);

    return RIG_OK;
}
