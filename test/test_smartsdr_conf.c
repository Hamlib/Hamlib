/*
 *  Hamlib SmartSDR configuration token tests
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

/* Unit tests for the settings a SmartSDR rig accepts. Every case works on a   */
/* rig that has been initialised but never opened, so nothing here reaches a   */
/* radio: a configuration token is read into the private data and read back    */
/* out of it, and that is the whole of what is under test.                     */

#include "acutest.h"

#include <stdlib.h>
#include <string.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <hamlib/port.h>


/* The model whose slice is chosen by the slice= setting rather than by the
 * model number, so every token is reachable on one rig. */
#define TEST_MODEL RIG_MODEL_SMARTSDR

/* Every setting this backend declares. */
static const char *const smartsdr_tokens[] =
{
    "nat_traversal",
    "auto_reconnect",
    "split_slice",
    "status_timeout",
    "slice",
    "spectrum",
    "slice_missing",
    "tx_audio_source",
    "liveness_timeout",
    "vita_port",
};

#define SMARTSDR_TOKEN_COUNT \
    ((int)(sizeof(smartsdr_tokens) / sizeof(smartsdr_tokens[0])))


/* An initialised, unopened rig. The caller cleans it up. */
static RIG *conf_rig(void)
{
    RIG *rig = rig_init(TEST_MODEL);

    if (!rig)
    {
        TEST_ASSERT(0 && "rig_init(SMARTSDR) failed");
    }

    return rig;
}


/* Naming the radio has to be enough. Without a port of its own the pathname
 * falls back to rigctld's 4532, where a SmartSDR radio does not answer, so
 * "-r <host>" was refused outright. */
void test_default_port_is_the_smartsdr_port(void)
{
    RIG *rig = conf_rig();
    hamlib_port_t *rp =
        (hamlib_port_t *)rig_data_pointer(rig, RIG_PTRX_RIGPORT);

    TEST_ASSERT(rp != NULL);
    TEST_CHECK(rp->default_port == 4992);
    TEST_MSG("expected 4992, got %d", rp->default_port);

    rig_cleanup(rig);
}


/* Set a setting by name and report what the backend made of it. */
static int set_by_name(RIG *rig, const char *name, const char *val)
{
    hamlib_token_t tok = rig_token_lookup(rig, name);

    TEST_CHECK(tok != RIG_CONF_END);
    TEST_MSG("token \"%s\" is not declared", name);

    return rig_set_conf(rig, tok, val);
}


/* Read a setting by name into out. Returns the rig_get_conf result. */
static int get_by_name(RIG *rig, const char *name, char *out, size_t out_len)
{
    hamlib_token_t tok = rig_token_lookup(rig, name);
    int rc;

    TEST_CHECK(tok != RIG_CONF_END);
    TEST_MSG("token \"%s\" is not declared", name);

    out[0] = '\0';
    rc = rig_get_conf2(rig, tok, out, (int)out_len);

    return rc;
}


/* Set a value and require it to come back unchanged. */
static void check_roundtrip(RIG *rig, const char *name, const char *val)
{
    char got[256];
    int rc = set_by_name(rig, name, val);

    TEST_CHECK(rc == RIG_OK);
    TEST_MSG("set %s=\"%s\" returned %d (%s)", name, val, rc, rigerror(rc));

    if (rc != RIG_OK)
    {
        return;
    }

    rc = get_by_name(rig, name, got, sizeof(got));
    TEST_CHECK(rc == RIG_OK);
    TEST_MSG("get %s returned %d (%s)", name, rc, rigerror(rc));

    TEST_CHECK(strcmp(got, val) == 0);
    TEST_MSG("%s round-tripped as \"%s\", expected \"%s\"", name, got, val);
}


/* Set a value the backend must refuse, and require the refusal to be
 * -RIG_EINVAL rather than some other error or a silent acceptance. */
static void check_rejected(RIG *rig, const char *name, const char *val)
{
    int rc = set_by_name(rig, name, val);

    TEST_CHECK(rc == -RIG_EINVAL);
    TEST_MSG("set %s=\"%s\" returned %d (%s), expected -RIG_EINVAL",
             name, val, rc, rigerror(rc));
}


/* ================================================================== */
/* Discovery                                                           */
/* ================================================================== */

/* An application finds a setting by name, so every one of them must be
 * declared in the table the frontend searches. */
void test_every_token_is_discoverable(void)
{
    RIG *rig = conf_rig();
    int i;
    int j;

    for (i = 0; i < SMARTSDR_TOKEN_COUNT; i++)
    {
        hamlib_token_t tok = rig_token_lookup(rig, smartsdr_tokens[i]);

        TEST_CHECK(tok != RIG_CONF_END);
        TEST_MSG("rig_token_lookup(\"%s\") found nothing", smartsdr_tokens[i]);

        TEST_CHECK(rig_confparam_lookup(rig, smartsdr_tokens[i]) != NULL);
        TEST_MSG("rig_confparam_lookup(\"%s\") found nothing",
                 smartsdr_tokens[i]);
    }

    /* Two settings sharing a token number would silently overwrite one
     * another. */
    for (i = 0; i < SMARTSDR_TOKEN_COUNT; i++)
    {
        for (j = i + 1; j < SMARTSDR_TOKEN_COUNT; j++)
        {
            TEST_CHECK(rig_token_lookup(rig, smartsdr_tokens[i])
                       != rig_token_lookup(rig, smartsdr_tokens[j]));
            TEST_MSG("\"%s\" and \"%s\" share a token",
                     smartsdr_tokens[i], smartsdr_tokens[j]);
        }
    }

    rig_cleanup(rig);
}


/* A setting nobody declared must be refused rather than quietly ignored. */
void test_unknown_token_is_rejected(void)
{
    RIG *rig = conf_rig();

    TEST_CHECK(rig_token_lookup(rig, "no_such_setting") == RIG_CONF_END);
    TEST_MSG("an undeclared name was resolved to a token");

    rig_cleanup(rig);
}


/* ================================================================== */
/* Defaults                                                            */
/* ================================================================== */

/* What the table declares as the default has to be what an application gets
 * when it never sets the value, or the documentation is wrong. */
void test_defaults_are_what_the_table_declares(void)
{
    RIG *rig;
    int i;

#if !defined(_WIN32) && !defined(__WIN32__)
    /* nat_traversal has an environment alias, which would otherwise decide
     * this rig's default instead of the table. */
    unsetenv("HAMLIB_SMARTSDR_WAN");
#endif

    rig = conf_rig();

    for (i = 0; i < SMARTSDR_TOKEN_COUNT; i++)
    {
        const struct confparams *cfp =
            rig_confparam_lookup(rig, smartsdr_tokens[i]);
        char got[256];
        int rc;

        if (!TEST_CHECK(cfp != NULL))
        {
            continue;
        }

        TEST_CHECK(cfp->dflt != NULL);
        TEST_MSG("\"%s\" declares no default", smartsdr_tokens[i]);

        rc = get_by_name(rig, smartsdr_tokens[i], got, sizeof(got));
        TEST_CHECK(rc == RIG_OK);
        TEST_MSG("get %s returned %d (%s)", smartsdr_tokens[i], rc,
                 rigerror(rc));

        TEST_CHECK(cfp->dflt != NULL && strcmp(got, cfp->dflt) == 0);
        TEST_MSG("%s defaults to \"%s\" but the table declares \"%s\"",
                 smartsdr_tokens[i], got, cfp->dflt ? cfp->dflt : "(none)");
    }

    rig_cleanup(rig);
}


/* ================================================================== */
/* nat_traversal, auto_reconnect, spectrum                             */
/* ================================================================== */

void test_nat_traversal_roundtrip(void)
{
    RIG *rig = conf_rig();

    check_roundtrip(rig, "nat_traversal", "1");
    check_roundtrip(rig, "nat_traversal", "0");

    rig_cleanup(rig);
}


void test_auto_reconnect_roundtrip(void)
{
    RIG *rig = conf_rig();

    check_roundtrip(rig, "auto_reconnect", "1");
    check_roundtrip(rig, "auto_reconnect", "0");

    rig_cleanup(rig);
}


void test_spectrum_roundtrip(void)
{
    RIG *rig = conf_rig();

    check_roundtrip(rig, "spectrum", "1");
    check_roundtrip(rig, "spectrum", "0");

    rig_cleanup(rig);
}


/* These three are on/off settings, so anything that is not zero is on and
 * reads back as the one canonical spelling of on. */
void test_checkbutton_values_normalise(void)
{
    static const char *const names[] =
    { "nat_traversal", "auto_reconnect", "spectrum" };
    RIG *rig = conf_rig();
    int i;

    for (i = 0; i < 3; i++)
    {
        char got[256];

        TEST_CHECK(set_by_name(rig, names[i], "2") == RIG_OK);
        TEST_CHECK(get_by_name(rig, names[i], got, sizeof(got)) == RIG_OK);
        TEST_CHECK(strcmp(got, "1") == 0);
        TEST_MSG("%s=2 read back as \"%s\", expected \"1\"", names[i], got);

        TEST_CHECK(set_by_name(rig, names[i], "-1") == RIG_OK);
        TEST_CHECK(get_by_name(rig, names[i], got, sizeof(got)) == RIG_OK);
        TEST_CHECK(strcmp(got, "1") == 0);
        TEST_MSG("%s=-1 read back as \"%s\", expected \"1\"", names[i], got);

        TEST_CHECK(set_by_name(rig, names[i], "0") == RIG_OK);
        TEST_CHECK(get_by_name(rig, names[i], got, sizeof(got)) == RIG_OK);
        TEST_CHECK(strcmp(got, "0") == 0);
        TEST_MSG("%s=0 read back as \"%s\", expected \"0\"", names[i], got);
    }

    /* All three are declared as on/off settings, which is what makes the
     * normalisation above the right behaviour rather than a quirk. */
    for (i = 0; i < 3; i++)
    {
        const struct confparams *cfp = rig_confparam_lookup(rig, names[i]);

        TEST_CHECK(cfp != NULL && cfp->type == RIG_CONF_CHECKBUTTON);
        TEST_MSG("%s is not declared as a checkbutton", names[i]);
    }

    rig_cleanup(rig);
}


/* ================================================================== */
/* status_timeout                                                      */
/* ================================================================== */

void test_status_timeout_roundtrip(void)
{
    RIG *rig = conf_rig();
    const struct confparams *cfp;

    check_roundtrip(rig, "status_timeout", "0");
    check_roundtrip(rig, "status_timeout", "1");
    check_roundtrip(rig, "status_timeout", "2500");
    check_roundtrip(rig, "status_timeout", "3600000");

    cfp = rig_confparam_lookup(rig, "status_timeout");
    TEST_CHECK(cfp != NULL && cfp->type == RIG_CONF_NUMERIC);
    TEST_MSG("status_timeout is not declared as numeric");

    if (cfp != NULL)
    {
        TEST_CHECK(cfp->u.n.min == 0.0f);
        TEST_MSG("declared minimum is %f, expected 0", (double)cfp->u.n.min);
        TEST_CHECK(cfp->u.n.max == 3600000.0f);
        TEST_MSG("declared maximum is %f, expected 3600000",
                 (double)cfp->u.n.max);
    }

    rig_cleanup(rig);
}


/* ================================================================== */
/* liveness_timeout                                                    */
/* ================================================================== */

void test_liveness_timeout_roundtrip(void)
{
    RIG *rig = conf_rig();
    const struct confparams *cfp;

    check_roundtrip(rig, "liveness_timeout", "500");
    check_roundtrip(rig, "liveness_timeout", "2000");
    check_roundtrip(rig, "liveness_timeout", "20000");
    check_roundtrip(rig, "liveness_timeout", "3600000");

    cfp = rig_confparam_lookup(rig, "liveness_timeout");
    TEST_CHECK(cfp != NULL && cfp->type == RIG_CONF_NUMERIC);
    TEST_MSG("liveness_timeout is not declared as numeric");

    if (cfp != NULL)
    {
        TEST_CHECK(cfp->u.n.min == 500.0f);
        TEST_MSG("declared minimum is %f, expected 500", (double)cfp->u.n.min);
        TEST_CHECK(cfp->u.n.max == 3600000.0f);
        TEST_MSG("declared maximum is %f, expected 3600000",
                 (double)cfp->u.n.max);
    }

    rig_cleanup(rig);
}


/* ================================================================== */
/* vita_port                                                           */
/* ================================================================== */

/* Every radio uses 4991, and that is what the setting defaults to; it exists
 * for a host where something else already holds the port -- a second client,
 * or the test harness standing in for the radio, which has to be reachable at
 * the port the client transmits to. */
void test_vita_port_defaults_to_4991(void)
{
    RIG *rig = conf_rig();
    char val[16] = "";

    TEST_CHECK(rig_get_conf2(rig, rig_token_lookup(rig, "vita_port"), val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK_(!strcmp(val, "4991"), "vita_port defaults to \"%s\"", val);

    rig_cleanup(rig);
}


void test_vita_port_roundtrip(void)
{
    RIG *rig = conf_rig();
    const struct confparams *cfp;

    check_roundtrip(rig, "vita_port", "4992");
    check_roundtrip(rig, "vita_port", "4998");
    check_roundtrip(rig, "vita_port", "65535");

    cfp = rig_confparam_lookup(rig, "vita_port");
    TEST_CHECK(cfp != NULL && cfp->type == RIG_CONF_NUMERIC);
    TEST_MSG("vita_port is not declared as numeric");

    rig_cleanup(rig);
}


/* A port outside the range addresses nothing, so it is refused rather than
 * silently truncated into one that does. */
void test_vita_port_rejects_out_of_range(void)
{
    RIG *rig = conf_rig();
    token_t tok = rig_token_lookup(rig, "vita_port");

    TEST_CHECK(rig_set_conf(rig, tok, "0") != RIG_OK);
    TEST_MSG("port 0 was accepted");
    TEST_CHECK(rig_set_conf(rig, tok, "65536") != RIG_OK);
    TEST_MSG("port 65536 was accepted");
    TEST_CHECK(rig_set_conf(rig, tok, "-1") != RIG_OK);
    TEST_MSG("a negative port was accepted");

    rig_cleanup(rig);
}


/* A timeout of zero, or one shorter than a round trip, would end every
 * session the moment it started, so the range the setting advertises is
 * enforced rather than merely documented. */
void test_liveness_timeout_rejects_out_of_range(void)
{
    RIG *rig = conf_rig();

    check_rejected(rig, "liveness_timeout", "0");
    check_rejected(rig, "liveness_timeout", "499");
    check_rejected(rig, "liveness_timeout", "-1");
    check_rejected(rig, "liveness_timeout", "3600001");

    rig_cleanup(rig);
}


/* ================================================================== */
/* slice                                                               */
/* ================================================================== */

void test_slice_accepts_a_to_h(void)
{
    RIG *rig = conf_rig();
    char letter[2];
    int i;

    for (i = 0; i < 8; i++)
    {
        letter[0] = (char)('A' + i);
        letter[1] = '\0';
        check_roundtrip(rig, "slice", letter);
    }

    rig_cleanup(rig);
}


/* Operators write the letter either way round, and the radio has one slice
 * either way, so a lower-case letter names the same slice. */
void test_slice_accepts_lower_case(void)
{
    RIG *rig = conf_rig();
    char got[256];

    TEST_CHECK(set_by_name(rig, "slice", "c") == RIG_OK);
    TEST_CHECK(get_by_name(rig, "slice", got, sizeof(got)) == RIG_OK);
    TEST_CHECK(strcmp(got, "C") == 0);
    TEST_MSG("slice=c read back as \"%s\", expected \"C\"", got);

    rig_cleanup(rig);
}


/* The radio has eight slices, so a letter past H names nothing and must be
 * refused rather than silently addressing the wrong receiver. */
void test_slice_rejects_out_of_range(void)
{
    RIG *rig = conf_rig();
    char got[256];

    TEST_CHECK(set_by_name(rig, "slice", "B") == RIG_OK);

    check_rejected(rig, "slice", "Z");
    check_rejected(rig, "slice", "I");
    check_rejected(rig, "slice", "");
    check_rejected(rig, "slice", "1");
    check_rejected(rig, "slice", "@");

    /* A refused setting must leave the previous one standing. */
    TEST_CHECK(get_by_name(rig, "slice", got, sizeof(got)) == RIG_OK);
    TEST_CHECK(strcmp(got, "B") == 0);
    TEST_MSG("a rejected slice changed the setting to \"%s\"", got);

    rig_cleanup(rig);
}


/* ================================================================== */
/* split_slice                                                         */
/* ================================================================== */

void test_split_slice_roundtrip(void)
{
    RIG *rig = conf_rig();
    char got[256];

    /* This rig is on slice A by default, so any other slice may transmit. */
    check_roundtrip(rig, "split_slice", "B");
    check_roundtrip(rig, "split_slice", "H");

    /* Empty means "whichever slice the radio has marked as transmitting",
     * which is a value in its own right and must read back as empty. */
    TEST_CHECK(set_by_name(rig, "split_slice", "") == RIG_OK);
    TEST_CHECK(get_by_name(rig, "split_slice", got, sizeof(got)) == RIG_OK);
    TEST_CHECK(got[0] == '\0');
    TEST_MSG("split_slice=\"\" read back as \"%s\", expected empty", got);

    rig_cleanup(rig);
}


/* A transmit slice that is this rig's own receive slice is not split at all,
 * and accepting it would report split on while both frequencies were one. */
void test_split_slice_rejects_own_slice(void)
{
    RIG *rig = conf_rig();
    char got[256];

    check_rejected(rig, "split_slice", "A");

    TEST_CHECK(set_by_name(rig, "slice", "D") == RIG_OK);
    check_rejected(rig, "split_slice", "D");

    /* Another slice on the same rig is still fine. */
    check_roundtrip(rig, "split_slice", "E");

    TEST_CHECK(get_by_name(rig, "slice", got, sizeof(got)) == RIG_OK);
    TEST_CHECK(strcmp(got, "D") == 0);
    TEST_MSG("slice became \"%s\" while setting split_slice", got);

    rig_cleanup(rig);
}


void test_split_slice_rejects_out_of_range(void)
{
    RIG *rig = conf_rig();

    check_rejected(rig, "split_slice", "Z");
    check_rejected(rig, "split_slice", "I");
    check_rejected(rig, "split_slice", "9");

    rig_cleanup(rig);
}


/* ================================================================== */
/* slice_missing                                                       */
/* ================================================================== */

void test_slice_missing_roundtrip(void)
{
    RIG *rig = conf_rig();

    check_roundtrip(rig, "slice_missing", "fail");
    check_roundtrip(rig, "slice_missing", "create");

    rig_cleanup(rig);
}


void test_slice_missing_rejects_bogus(void)
{
    RIG *rig = conf_rig();
    char got[256];

    TEST_CHECK(set_by_name(rig, "slice_missing", "fail") == RIG_OK);

    check_rejected(rig, "slice_missing", "bogus");
    check_rejected(rig, "slice_missing", "");
    check_rejected(rig, "slice_missing", "Create");

    TEST_CHECK(get_by_name(rig, "slice_missing", got, sizeof(got)) == RIG_OK);
    TEST_CHECK(strcmp(got, "fail") == 0);
    TEST_MSG("a rejected slice_missing changed the setting to \"%s\"", got);

    rig_cleanup(rig);
}


/* ================================================================== */
/* tx_audio_source                                                     */
/* ================================================================== */

void test_tx_audio_source_roundtrip(void)
{
    RIG *rig = conf_rig();

    check_roundtrip(rig, "tx_audio_source", "mic");
    check_roundtrip(rig, "tx_audio_source", "acc");
    check_roundtrip(rig, "tx_audio_source", "pc");
    check_roundtrip(rig, "tx_audio_source", "dax");

    rig_cleanup(rig);
}


void test_tx_audio_source_rejects_bogus(void)
{
    RIG *rig = conf_rig();
    char got[256];

    TEST_CHECK(set_by_name(rig, "tx_audio_source", "dax") == RIG_OK);

    check_rejected(rig, "tx_audio_source", "bogus");
    check_rejected(rig, "tx_audio_source", "");
    check_rejected(rig, "tx_audio_source", "MIC");
    check_rejected(rig, "tx_audio_source", "line");

    TEST_CHECK(get_by_name(rig, "tx_audio_source", got, sizeof(got)) == RIG_OK);
    TEST_CHECK(strcmp(got, "dax") == 0);
    TEST_MSG("a rejected tx_audio_source changed the setting to \"%s\"", got);

    rig_cleanup(rig);
}


/* ================================================================== */
/* Declared choices                                                    */
/* ================================================================== */

/* A setting offered as a list of choices must accept every choice it lists,
 * and only those. An application builds its menu from that list. */
void test_combo_choices_are_the_accepted_values(void)
{
    static const char *const combos[] = { "tx_audio_source", "slice_missing" };
    RIG *rig = conf_rig();
    int i;

    for (i = 0; i < 2; i++)
    {
        const struct confparams *cfp = rig_confparam_lookup(rig, combos[i]);
        int j;

        if (!TEST_CHECK(cfp != NULL))
        {
            continue;
        }

        TEST_CHECK(cfp->type == RIG_CONF_COMBO);
        TEST_MSG("%s is not declared as a combo", combos[i]);

        for (j = 0; j < RIG_COMBO_MAX && cfp->u.c.combostr[j] != NULL; j++)
        {
            check_roundtrip(rig, combos[i], cfp->u.c.combostr[j]);
        }

        TEST_CHECK(j > 0);
        TEST_MSG("%s lists no choices", combos[i]);

        /* The declared default has to be one of the declared choices. */
        {
            int found = 0;
            int k;

            for (k = 0; k < j; k++)
            {
                if (strcmp(cfp->u.c.combostr[k], cfp->dflt) == 0)
                {
                    found = 1;
                    break;
                }
            }

            TEST_CHECK(found);
            TEST_MSG("%s defaults to \"%s\", which it does not list",
                     combos[i], cfp->dflt);
        }
    }

    rig_cleanup(rig);
}


/* ================================================================== */
/* Independence                                                        */
/* ================================================================== */

/* Settings are read into one structure, so a bug in one handler shows up as
 * another setting changing. Set them all, then read them all back. */
void test_settings_do_not_disturb_each_other(void)
{
    static const char *const values[SMARTSDR_TOKEN_COUNT] =
    {
        "1",        /* nat_traversal   */
        "1",        /* auto_reconnect  */
        "G",        /* split_slice     */
        "1500",     /* status_timeout  */
        "C",        /* slice           */
        "1",        /* spectrum        */
        "fail",     /* slice_missing   */
        "acc",      /* tx_audio_source */
        "45000",    /* liveness_timeout */
        "4993",     /* vita_port       */
    };
    RIG *rig = conf_rig();
    int i;

    /* slice is set before split_slice, which is checked against it. */
    TEST_CHECK(set_by_name(rig, "slice", "C") == RIG_OK);

    for (i = 0; i < SMARTSDR_TOKEN_COUNT; i++)
    {
        int rc = set_by_name(rig, smartsdr_tokens[i], values[i]);

        TEST_CHECK(rc == RIG_OK);
        TEST_MSG("set %s=\"%s\" returned %d (%s)", smartsdr_tokens[i],
                 values[i], rc, rigerror(rc));
    }

    for (i = 0; i < SMARTSDR_TOKEN_COUNT; i++)
    {
        char got[256];

        TEST_CHECK(get_by_name(rig, smartsdr_tokens[i], got,
                               sizeof(got)) == RIG_OK);
        TEST_CHECK(strcmp(got, values[i]) == 0);
        TEST_MSG("%s is \"%s\" after every setting was written, expected \"%s\"",
                 smartsdr_tokens[i], got, values[i]);
    }

    rig_cleanup(rig);
}


/* ================================================================== */
/* Test list                                                           */
/* ================================================================== */

TEST_LIST =
{
    { "every_token_is_discoverable",        test_every_token_is_discoverable },
    { "unknown_token_is_rejected",          test_unknown_token_is_rejected },
    { "defaults_are_what_the_table_declares", test_defaults_are_what_the_table_declares },
    { "nat_traversal_roundtrip",            test_nat_traversal_roundtrip },
    { "auto_reconnect_roundtrip",           test_auto_reconnect_roundtrip },
    { "spectrum_roundtrip",                 test_spectrum_roundtrip },
    { "checkbutton_values_normalise",       test_checkbutton_values_normalise },
    { "status_timeout_roundtrip",           test_status_timeout_roundtrip },
    { "liveness_timeout_roundtrip",         test_liveness_timeout_roundtrip },
    { "liveness_timeout_rejects_out_of_range", test_liveness_timeout_rejects_out_of_range },
    { "vita_port_defaults_to_4991",         test_vita_port_defaults_to_4991 },
    { "vita_port_roundtrip",                test_vita_port_roundtrip },
    { "vita_port_rejects_out_of_range",     test_vita_port_rejects_out_of_range },
    { "slice_accepts_a_to_h",               test_slice_accepts_a_to_h },
    { "slice_accepts_lower_case",           test_slice_accepts_lower_case },
    { "slice_rejects_out_of_range",         test_slice_rejects_out_of_range },
    { "split_slice_roundtrip",              test_split_slice_roundtrip },
    { "split_slice_rejects_own_slice",      test_split_slice_rejects_own_slice },
    { "split_slice_rejects_out_of_range",   test_split_slice_rejects_out_of_range },
    { "slice_missing_roundtrip",            test_slice_missing_roundtrip },
    { "slice_missing_rejects_bogus",        test_slice_missing_rejects_bogus },
    { "tx_audio_source_roundtrip",          test_tx_audio_source_roundtrip },
    { "tx_audio_source_rejects_bogus",      test_tx_audio_source_rejects_bogus },
    { "combo_choices_are_the_accepted_values", test_combo_choices_are_the_accepted_values },
    { "settings_do_not_disturb_each_other", test_settings_do_not_disturb_each_other },
    { "default_port_is_the_smartsdr_port", test_default_port_is_the_smartsdr_port },
    { NULL, NULL }
};
