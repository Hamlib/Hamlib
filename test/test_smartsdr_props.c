/*
 *  Hamlib SmartSDR tracked property tests
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* Unit tests for the backend's model of the radio: reading a status line into */
/* the tracked properties, deciding whether a tracked value is still usable,   */
/* and building the command that writes one back. The rig is initialised but   */
/* never opened, so nothing here reaches a radio.                              */

#include "acutest.h"

#include <stdio.h>
#include <string.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <hamlib/rig_state.h>

#include "smartsdr_priv.h"
#include "smartsdr_props.h"


/* One full slice line as the radio sends it, trimmed to the properties the
 * backend tracks. */
#define SLICE_LINE \
    "S1234ABCD|slice 0 in_use=1 sample_rate=24000 RF_frequency=14.100000 " \
    "client_handle=0x76AF7C73 index_letter=A rit_on=1 rit_freq=-250 " \
    "xit_on=0 xit_freq=0 rxant=ANT2 mode=DIGU filter_lo=100 filter_hi=2800 " \
    "step=10 agc_mode=fast pan=0x40000000 txant=ANT1 lock=0 tx=1 active=1 " \
    "audio_level=73 audio_pan=51 audio_mute=1 anf=0 nr=1 nr_level=42 nb=0 " \
    "nb_level=50 wnb=0 apf=1 apf_level=30 squelch=1 squelch_level=20 " \
    "diversity=0 rfgain=24 tx_ant_list=ANT1,ANT2,XVTA"

/* A transmit line, which is a different object with its own keys. */
#define TRANSMIT_LINE \
    "S1234ABCD|transmit rfpower=25 tunepower=10 mic_level=50 " \
    "speech_processor_enable=1 speech_processor_level=35 vox_enable=1 " \
    "vox_level=40 vox_delay=20 speed=30 pitch=600 break_in=1 " \
    "break_in_delay=250 mon_gain_sb=45 mon_gain_cw=55 sb_monitor=1 tune=0"


/* An initialised, unopened rig and its private data. */
static RIG *props_rig(struct smartsdr_priv_data **out_priv)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR);

    if (!rig)
    {
        TEST_ASSERT(0 && "rig_init(SMARTSDR) failed");
    }

    *out_priv = (struct smartsdr_priv_data *)
                ((struct rig_state *)rig_data_pointer(rig,
                        RIG_PTRX_STATE))->priv;

    if (*out_priv == NULL)
    {
        rig_cleanup(rig);
        TEST_ASSERT(0 && "rig has no private data");
    }

    return rig;
}


/* Build the command for a property and require it to be exactly cmd. */
static void check_set_cmd(int idx, int slicenum, const char *value,
                          const char *expected)
{
    char got[160];
    int rc;

    memset(got, 0, sizeof(got));
    rc = smartsdr_prop_build_set_cmd(idx, slicenum, value, got, sizeof(got));

    TEST_CHECK(rc == 0);
    TEST_MSG("building the command for property %d returned %d", idx, rc);

    if (rc != 0)
    {
        return;
    }

    TEST_CHECK(strcmp(got, expected) == 0);
    TEST_MSG("property %d built \"%s\", expected \"%s\"", idx, got, expected);
}


/* ================================================================== */
/* smartsdr_status_find                                                */
/* ================================================================== */

/* Keys are matched whole. "pan=" appears inside "audio_pan=", and reading the
 * panadapter id out of the audio balance would bind the rig to a panadapter
 * that does not exist. */
void test_status_find_requires_a_key_boundary(void)
{
    static const char *line =
        "S1|slice 0 audio_pan=51 pan=0x40000000 audio_level=73";
    const char *v = smartsdr_status_find(line, "pan");

    TEST_CHECK(v != NULL);
    TEST_MSG("pan= was not found at all");

    if (v != NULL)
    {
        TEST_CHECK(strncmp(v, "0x40000000", 10) == 0);
        TEST_MSG("pan= resolved to \"%.12s\", expected 0x40000000", v);
    }

    v = smartsdr_status_find(line, "audio_pan");
    TEST_CHECK(v != NULL && strncmp(v, "51", 2) == 0);
    TEST_MSG("audio_pan= resolved wrongly");

    /* A key that only appears as the tail of another key is not present. */
    TEST_CHECK(smartsdr_status_find(line, "io_pan") == NULL);
    TEST_MSG("io_pan matched inside audio_pan");
}


/* A key must be followed by '=' to be a key at all. */
void test_status_find_requires_an_equals(void)
{
    static const char *line = "S1|slice 0 mode_list=USB,LSB mode=USB";

    TEST_CHECK(smartsdr_status_find(line, "mode_list") != NULL);

    {
        const char *v = smartsdr_status_find(line, "mode");

        TEST_CHECK(v != NULL);
        TEST_MSG("mode= was not found past mode_list=");

        if (v != NULL)
        {
            TEST_CHECK(strncmp(v, "USB", 3) == 0);
            TEST_MSG("mode= resolved to \"%.8s\"", v);
        }
    }

    TEST_CHECK(smartsdr_status_find(line, "nosuchkey") == NULL);
    TEST_CHECK(smartsdr_status_find(NULL, "mode") == NULL);
}


/* A key at the very start of the text is still a key. */
void test_status_find_matches_at_line_start(void)
{
    const char *v = smartsdr_status_find("tx=1 active=1", "tx");

    TEST_CHECK(v != NULL && v[0] == '1');
    TEST_MSG("tx= at the start of the line was not found");
}


/* ================================================================== */
/* smartsdr_props_absorb                                               */
/* ================================================================== */

void test_absorb_slice_status_line(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, SLICE_LINE);

    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].seen);
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].ival == 73);
    TEST_MSG("audio_level=%d expected 73",
             priv->props[SMARTSDR_P_AUDIO_LEVEL].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_SQUELCH_LEVEL].ival == 20);
    TEST_MSG("squelch_level=%d expected 20",
             priv->props[SMARTSDR_P_SQUELCH_LEVEL].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_NR_LEVEL].ival == 42);
    TEST_MSG("nr_level=%d expected 42",
             priv->props[SMARTSDR_P_NR_LEVEL].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_APF_LEVEL].ival == 30);
    TEST_MSG("apf_level=%d expected 30",
             priv->props[SMARTSDR_P_APF_LEVEL].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_PAN].ival == 51);
    TEST_MSG("audio_pan=%d expected 51",
             priv->props[SMARTSDR_P_AUDIO_PAN].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_RFGAIN].ival == 24);
    TEST_MSG("rfgain=%d expected 24", priv->props[SMARTSDR_P_RFGAIN].ival);

    /* On/off properties are the same tracked values, read as 0 or 1. */
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_MUTE].ival == 1);
    TEST_CHECK(priv->props[SMARTSDR_P_SQUELCH].ival == 1);
    TEST_CHECK(priv->props[SMARTSDR_P_NR].ival == 1);
    TEST_CHECK(priv->props[SMARTSDR_P_NB].ival == 0);
    TEST_CHECK(priv->props[SMARTSDR_P_APF].ival == 1);
    TEST_CHECK(priv->props[SMARTSDR_P_ANF].ival == 0);
    TEST_CHECK(priv->props[SMARTSDR_P_LOCK].ival == 0);
    TEST_CHECK(priv->props[SMARTSDR_P_RIT_ON].ival == 1);
    TEST_CHECK(priv->props[SMARTSDR_P_XIT_ON].ival == 0);
    TEST_CHECK(priv->props[SMARTSDR_P_TX].ival == 1);

    /* Frequencies and offsets keep their sign and their fraction. */
    TEST_CHECK(priv->props[SMARTSDR_P_RIT_FREQ].ival == -250);
    TEST_MSG("rit_freq=%d expected -250",
             priv->props[SMARTSDR_P_RIT_FREQ].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_RF_FREQUENCY].dval == 14.1);
    TEST_MSG("RF_frequency=%f expected 14.1",
             priv->props[SMARTSDR_P_RF_FREQUENCY].dval);

    TEST_CHECK(priv->props[SMARTSDR_P_FILTER_LO].ival == 100);
    TEST_CHECK(priv->props[SMARTSDR_P_FILTER_HI].ival == 2800);
    TEST_CHECK(priv->props[SMARTSDR_P_STEP].ival == 10);

    /* String properties keep the radio's own spelling. */
    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_MODE].sval, "DIGU") == 0);
    TEST_MSG("mode=\"%s\" expected DIGU", priv->props[SMARTSDR_P_MODE].sval);

    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_AGC_MODE].sval, "fast") == 0);
    TEST_MSG("agc_mode=\"%s\" expected fast",
             priv->props[SMARTSDR_P_AGC_MODE].sval);

    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_RXANT].sval, "ANT2") == 0);
    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_TXANT].sval, "ANT1") == 0);
    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_TX_ANT_LIST].sval,
                      "ANT1,ANT2,XVTA") == 0);
    TEST_MSG("tx_ant_list=\"%s\"", priv->props[SMARTSDR_P_TX_ANT_LIST].sval);

    rig_cleanup(rig);
}


void test_absorb_transmit_status_line(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_TRANSMIT, TRANSMIT_LINE);

    TEST_CHECK(priv->props[SMARTSDR_P_RFPOWER].ival == 25);
    TEST_MSG("rfpower=%d expected 25", priv->props[SMARTSDR_P_RFPOWER].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_MIC_LEVEL].ival == 50);
    TEST_MSG("mic_level=%d expected 50",
             priv->props[SMARTSDR_P_MIC_LEVEL].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_SP_LEVEL].ival == 35);
    TEST_CHECK(priv->props[SMARTSDR_P_SP_ENABLE].ival == 1);
    TEST_CHECK(priv->props[SMARTSDR_P_VOX_LEVEL].ival == 40);
    TEST_CHECK(priv->props[SMARTSDR_P_VOX_DELAY].ival == 20);
    TEST_CHECK(priv->props[SMARTSDR_P_VOX_ENABLE].ival == 1);
    TEST_CHECK(priv->props[SMARTSDR_P_SPEED].ival == 30);
    TEST_CHECK(priv->props[SMARTSDR_P_PITCH].ival == 600);
    TEST_CHECK(priv->props[SMARTSDR_P_BREAK_IN].ival == 1);
    TEST_CHECK(priv->props[SMARTSDR_P_BREAK_IN_DELAY].ival == 250);
    TEST_CHECK(priv->props[SMARTSDR_P_MON_GAIN_SB].ival == 45);
    TEST_CHECK(priv->props[SMARTSDR_P_MON_GAIN_CW].ival == 55);
    TEST_CHECK(priv->props[SMARTSDR_P_TUNE].ival == 0);

    /* The transmit monitor is reported as sb_monitor, which is the name that
     * has to be looked for; mon is only the name it is written under. */
    TEST_CHECK(priv->props[SMARTSDR_P_SB_MONITOR].seen);
    TEST_MSG("sb_monitor was not read from the transmit line");
    TEST_CHECK(priv->props[SMARTSDR_P_SB_MONITOR].ival == 1);

    rig_cleanup(rig);
}


/* A property belongs to one object. Reading a slice line as if it were the
 * transmit object would mix two radios' worth of state into one. */
void test_absorb_ignores_the_wrong_object(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_TRANSMIT, SLICE_LINE);

    TEST_CHECK(!priv->props[SMARTSDR_P_AUDIO_LEVEL].seen);
    TEST_MSG("a slice property was taken from a transmit line");
    TEST_CHECK(!priv->props[SMARTSDR_P_MODE].seen);
    TEST_CHECK(!priv->props[SMARTSDR_P_RXANT].seen);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, TRANSMIT_LINE);

    TEST_CHECK(!priv->props[SMARTSDR_P_RFPOWER].seen);
    TEST_MSG("a transmit property was taken from a slice line");
    TEST_CHECK(!priv->props[SMARTSDR_P_SPEED].seen);

    rig_cleanup(rig);
}


/* A property the line does not mention keeps whatever it had. */
void test_absorb_leaves_unmentioned_properties_alone(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, SLICE_LINE);
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].ival == 73);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE,
                          "S1234ABCD|slice 0 squelch_level=5");

    TEST_CHECK(priv->props[SMARTSDR_P_SQUELCH_LEVEL].ival == 5);
    TEST_MSG("the short update did not apply");
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].ival == 73);
    TEST_MSG("audio_level became %d after an unrelated update",
             priv->props[SMARTSDR_P_AUDIO_LEVEL].ival);
    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_MODE].sval, "DIGU") == 0);

    rig_cleanup(rig);
}


/* A string value ends at the space before the next key, not at the end of the
 * line. */
void test_absorb_string_stops_at_the_next_key(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE,
                          "S1|slice 0 mode=USB rxant=ANT1 lock=0");

    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_MODE].sval, "USB") == 0);
    TEST_MSG("mode=\"%s\", expected USB", priv->props[SMARTSDR_P_MODE].sval);
    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_RXANT].sval, "ANT1") == 0);
    TEST_MSG("rxant=\"%s\", expected ANT1", priv->props[SMARTSDR_P_RXANT].sval);

    rig_cleanup(rig);
}


/* A value longer than the store is cut to fit rather than written past it. */
void test_absorb_truncates_an_overlong_string(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);
    char line[256];
    size_t room = sizeof(priv->props[SMARTSDR_P_TX_ANT_LIST].sval);

    snprintf(line, sizeof(line),
             "S1|slice 0 tx_ant_list=%s",
             "ANT1,ANT2,XVTA,XVTB,ANT3,ANT4,ANT5,ANT6,ANT7,ANT8,ANT9");
    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, line);

    TEST_CHECK(strlen(priv->props[SMARTSDR_P_TX_ANT_LIST].sval) == room - 1);
    TEST_MSG("stored %zu characters in a %zu-byte field",
             strlen(priv->props[SMARTSDR_P_TX_ANT_LIST].sval), room);

    TEST_CHECK(strncmp(priv->props[SMARTSDR_P_TX_ANT_LIST].sval,
                       "ANT1,ANT2,XVTA", 14) == 0);
    TEST_MSG("stored \"%s\"", priv->props[SMARTSDR_P_TX_ANT_LIST].sval);

    rig_cleanup(rig);
}


/* Reading a line stamps every property it carried, which is what the staleness
 * check works from. */
void test_absorb_marks_seen_and_stamps(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);
    int64_t before = smartsdr_now_ms();

    TEST_CHECK(!priv->props[SMARTSDR_P_AUDIO_LEVEL].seen);
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].stamp_ms == 0);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, SLICE_LINE);

    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].seen);
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].stamp_ms >= before);
    TEST_MSG("stamp %lld is before the line arrived at %lld",
             (long long)priv->props[SMARTSDR_P_AUDIO_LEVEL].stamp_ms,
             (long long)before);

    rig_cleanup(rig);
}


void test_absorb_guards_null(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    smartsdr_props_absorb(NULL, SMARTSDR_OBJ_SLICE, SLICE_LINE);
    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, NULL);

    TEST_CHECK(!priv->props[SMARTSDR_P_AUDIO_LEVEL].seen);
    TEST_MSG("a null line was absorbed anyway");

    rig_cleanup(rig);
}


/* ================================================================== */
/* smartsdr_prop_valid                                                 */
/* ================================================================== */

/* Nothing is readable until the radio has reported it: this backend never
 * queries a value, so an unseen property has no value to give. */
void test_prop_valid_needs_the_radio_to_have_reported(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    TEST_CHECK(!smartsdr_prop_valid(priv, SMARTSDR_P_AUDIO_LEVEL));
    TEST_MSG("an unreported property was usable");

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, SLICE_LINE);

    TEST_CHECK(smartsdr_prop_valid(priv, SMARTSDR_P_AUDIO_LEVEL));
    TEST_MSG("a freshly reported property was not usable");

    rig_cleanup(rig);
}


/* With an expiry configured, a value the radio reported long enough ago is
 * treated as unknown rather than reported as current. */
void test_prop_valid_honours_status_timeout(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, SLICE_LINE);
    priv->status_timeout_ms = 1000;

    TEST_CHECK(smartsdr_prop_valid(priv, SMARTSDR_P_AUDIO_LEVEL));
    TEST_MSG("a value reported just now was already stale");

    /* Age the value past the expiry without waiting for it. */
    priv->props[SMARTSDR_P_AUDIO_LEVEL].stamp_ms = smartsdr_now_ms() - 1001;
    TEST_CHECK(!smartsdr_prop_valid(priv, SMARTSDR_P_AUDIO_LEVEL));
    TEST_MSG("a value older than the expiry was still usable");

    /* Exactly at the expiry is not yet past it. */
    priv->props[SMARTSDR_P_AUDIO_LEVEL].stamp_ms = smartsdr_now_ms() - 1000;
    TEST_CHECK(smartsdr_prop_valid(priv, SMARTSDR_P_AUDIO_LEVEL));
    TEST_MSG("a value exactly at the expiry was discarded");

    /* Other properties expire on their own stamps, not on this one's. */
    TEST_CHECK(smartsdr_prop_valid(priv, SMARTSDR_P_MODE));
    TEST_MSG("an unrelated property expired with audio_level");

    /* A fresh report brings it back. */
    priv->props[SMARTSDR_P_AUDIO_LEVEL].stamp_ms = smartsdr_now_ms() - 5000;
    TEST_CHECK(!smartsdr_prop_valid(priv, SMARTSDR_P_AUDIO_LEVEL));
    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE,
                          "S1234ABCD|slice 0 audio_level=12");
    TEST_CHECK(smartsdr_prop_valid(priv, SMARTSDR_P_AUDIO_LEVEL));
    TEST_MSG("a re-reported value was still treated as stale");
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].ival == 12);

    rig_cleanup(rig);
}


/* The radio pushes its changes, so with no expiry configured a value stays
 * usable however long ago it arrived. */
void test_prop_valid_without_a_timeout_never_expires(void)
{
    struct smartsdr_priv_data *priv;
    RIG *rig = props_rig(&priv);

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, SLICE_LINE);

    TEST_CHECK(priv->status_timeout_ms == 0);
    TEST_MSG("the default expiry is %d, expected 0", priv->status_timeout_ms);

    priv->props[SMARTSDR_P_AUDIO_LEVEL].stamp_ms = smartsdr_now_ms() - 3600000;
    TEST_CHECK(smartsdr_prop_valid(priv, SMARTSDR_P_AUDIO_LEVEL));
    TEST_MSG("an hour-old value expired with no expiry configured");

    rig_cleanup(rig);
}


/* ================================================================== */
/* smartsdr_prop_build_set_cmd                                         */
/* ================================================================== */

/* A slice property is written to the slice this rig drives. */
void test_set_cmd_slice_form(void)
{
    check_set_cmd(SMARTSDR_P_AUDIO_LEVEL, 0, "50",
                  "slice set 0 audio_level=50");
    check_set_cmd(SMARTSDR_P_SQUELCH_LEVEL, 0, "20",
                  "slice set 0 squelch_level=20");
    check_set_cmd(SMARTSDR_P_AGC_MODE, 0, "fast",
                  "slice set 0 agc_mode=fast");
    check_set_cmd(SMARTSDR_P_RXANT, 0, "ANT2", "slice set 0 rxant=ANT2");
}


/* The slice number comes from the rig, so slice C writes to slice 2. */
void test_set_cmd_uses_the_rigs_slice(void)
{
    check_set_cmd(SMARTSDR_P_AUDIO_LEVEL, 2, "50",
                  "slice set 2 audio_level=50");
    check_set_cmd(SMARTSDR_P_AUDIO_LEVEL, 7, "50",
                  "slice set 7 audio_level=50");
}


/* A transmit property is written to the transmit object, which has no slice. */
void test_set_cmd_transmit_form(void)
{
    check_set_cmd(SMARTSDR_P_RFPOWER, 3, "25", "transmit set rfpower=25");
    check_set_cmd(SMARTSDR_P_VOX_LEVEL, 3, "40", "transmit set vox_level=40");
    check_set_cmd(SMARTSDR_P_SP_LEVEL, 3, "35",
                  "transmit set speech_processor_level=35");
}


/* RIG_LEVEL_MICGAIN reaches the microphone level, which the radio reports as
 * mic_level but only accepts as miclevel. */
void test_set_cmd_micgain_writes_miclevel(void)
{
    check_set_cmd(SMARTSDR_P_MIC_LEVEL, 0, "50", "transmit set miclevel=50");
}


/* RIG_FUNC_MON reaches the transmit monitor, which the radio reports as
 * sb_monitor but only accepts as mon. Writing the reported name is refused by
 * the radio, which is why the two names are held separately. */
void test_set_cmd_monitor_writes_mon(void)
{
    check_set_cmd(SMARTSDR_P_SB_MONITOR, 0, "1", "transmit set mon=1");
    check_set_cmd(SMARTSDR_P_SB_MONITOR, 0, "0", "transmit set mon=0");
}


/* RIG_LEVEL_KEYSPD reaches the keyer speed. It is reported on the transmit
 * object but written positionally to the CW command. */
void test_set_cmd_keyspd_is_positional_cw(void)
{
    check_set_cmd(SMARTSDR_P_SPEED, 0, "30", "cw wpm 30");
    check_set_cmd(SMARTSDR_P_SPEED, 5, "5", "cw wpm 5");
}


/* RIG_LEVEL_CWPITCH, likewise. */
void test_set_cmd_cwpitch_is_positional_cw(void)
{
    check_set_cmd(SMARTSDR_P_PITCH, 0, "600", "cw pitch 600");
}


/* RIG_LEVEL_BKIN_DLYMS, likewise. */
void test_set_cmd_bkin_dlyms_is_positional_cw(void)
{
    check_set_cmd(SMARTSDR_P_BREAK_IN_DELAY, 0, "250",
                  "cw break_in_delay 250");
}


/* RIG_FUNC_FBKIN, likewise: an on/off setting on the same command. */
void test_set_cmd_fbkin_is_positional_cw(void)
{
    check_set_cmd(SMARTSDR_P_BREAK_IN, 0, "1", "cw break_in 1");
    check_set_cmd(SMARTSDR_P_BREAK_IN, 0, "0", "cw break_in 0");
}


/* None of the CW settings may fall back to the transmit object's key=value
 * form, which the radio accepts and then ignores. */
void test_set_cmd_cw_settings_are_never_transmit_set(void)
{
    static const int cw_props[] =
    {
        SMARTSDR_P_SPEED, SMARTSDR_P_PITCH,
        SMARTSDR_P_BREAK_IN_DELAY, SMARTSDR_P_BREAK_IN
    };
    size_t i;

    for (i = 0; i < sizeof(cw_props) / sizeof(cw_props[0]); i++)
    {
        char got[160];

        TEST_CHECK(smartsdr_prop_build_set_cmd(cw_props[i], 0, "1", got,
                                               sizeof(got)) == 0);
        TEST_CHECK(strncmp(got, "cw ", 3) == 0);
        TEST_MSG("property %d built \"%s\", which is not a cw command",
                 cw_props[i], got);
        TEST_CHECK(strstr(got, "transmit set") == NULL);
        TEST_MSG("property %d built \"%s\"", cw_props[i], got);
    }
}


void test_set_cmd_rejects_bad_arguments(void)
{
    char got[160];

    TEST_CHECK(smartsdr_prop_build_set_cmd(-1, 0, "1", got, sizeof(got)) == -1);
    TEST_CHECK(smartsdr_prop_build_set_cmd(SMARTSDR_PROP_COUNT, 0, "1", got,
                                           sizeof(got)) == -1);
    TEST_CHECK(smartsdr_prop_build_set_cmd(SMARTSDR_P_RFPOWER, 0, NULL, got,
                                           sizeof(got)) == -1);
    TEST_CHECK(smartsdr_prop_build_set_cmd(SMARTSDR_P_RFPOWER, 0, "1", NULL,
                                           sizeof(got)) == -1);
    TEST_CHECK(smartsdr_prop_build_set_cmd(SMARTSDR_P_RFPOWER, 0, "1", got,
                                           0) == -1);
}


/* A command that does not fit would go out meaning something other than what
 * was asked for, so it is refused instead of truncated. */
void test_set_cmd_rejects_a_command_that_does_not_fit(void)
{
    char got[16];

    TEST_CHECK(smartsdr_prop_build_set_cmd(SMARTSDR_P_SP_LEVEL, 0, "35", got,
                                           sizeof(got)) == -1);
    TEST_MSG("a truncated command was accepted: \"%s\"", got);
    TEST_CHECK(got[0] == '\0');
    TEST_MSG("the refused command left \"%s\" behind", got);

    /* One byte short of the whole command is still short. */
    {
        char exact[sizeof("transmit set rfpower=25")];
        char tight[sizeof("transmit set rfpower=25") - 1];

        TEST_CHECK(smartsdr_prop_build_set_cmd(SMARTSDR_P_RFPOWER, 0, "25",
                                               exact, sizeof(exact)) == 0);
        TEST_CHECK(strcmp(exact, "transmit set rfpower=25") == 0);

        TEST_CHECK(smartsdr_prop_build_set_cmd(SMARTSDR_P_RFPOWER, 0, "25",
                                               tight, sizeof(tight)) == -1);
        TEST_MSG("a one-byte-short buffer took the whole command");
    }
}


/* ================================================================== */
/* Test list                                                           */
/* ================================================================== */

TEST_LIST =
{
    { "status_find_requires_a_key_boundary",  test_status_find_requires_a_key_boundary },
    { "status_find_requires_an_equals",       test_status_find_requires_an_equals },
    { "status_find_matches_at_line_start",    test_status_find_matches_at_line_start },
    { "absorb_slice_status_line",             test_absorb_slice_status_line },
    { "absorb_transmit_status_line",          test_absorb_transmit_status_line },
    { "absorb_ignores_the_wrong_object",      test_absorb_ignores_the_wrong_object },
    { "absorb_leaves_unmentioned_properties_alone", test_absorb_leaves_unmentioned_properties_alone },
    { "absorb_string_stops_at_the_next_key",  test_absorb_string_stops_at_the_next_key },
    { "absorb_truncates_an_overlong_string",  test_absorb_truncates_an_overlong_string },
    { "absorb_marks_seen_and_stamps",         test_absorb_marks_seen_and_stamps },
    { "absorb_guards_null",                   test_absorb_guards_null },
    { "prop_valid_needs_the_radio_to_have_reported", test_prop_valid_needs_the_radio_to_have_reported },
    { "prop_valid_honours_status_timeout",    test_prop_valid_honours_status_timeout },
    { "prop_valid_without_a_timeout_never_expires", test_prop_valid_without_a_timeout_never_expires },
    { "set_cmd_slice_form",                   test_set_cmd_slice_form },
    { "set_cmd_uses_the_rigs_slice",          test_set_cmd_uses_the_rigs_slice },
    { "set_cmd_transmit_form",                test_set_cmd_transmit_form },
    { "set_cmd_micgain_writes_miclevel",      test_set_cmd_micgain_writes_miclevel },
    { "set_cmd_monitor_writes_mon",           test_set_cmd_monitor_writes_mon },
    { "set_cmd_keyspd_is_positional_cw",      test_set_cmd_keyspd_is_positional_cw },
    { "set_cmd_cwpitch_is_positional_cw",     test_set_cmd_cwpitch_is_positional_cw },
    { "set_cmd_bkin_dlyms_is_positional_cw",  test_set_cmd_bkin_dlyms_is_positional_cw },
    { "set_cmd_fbkin_is_positional_cw",       test_set_cmd_fbkin_is_positional_cw },
    { "set_cmd_cw_settings_are_never_transmit_set", test_set_cmd_cw_settings_are_never_transmit_set },
    { "set_cmd_rejects_bad_arguments",        test_set_cmd_rejects_bad_arguments },
    { "set_cmd_rejects_a_command_that_does_not_fit", test_set_cmd_rejects_a_command_that_does_not_fit },
    { NULL, NULL }
};
