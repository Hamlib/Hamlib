/*
 * Hamlib Gemini status parser tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <string.h>

#include "gemini.h"
#include "hamlib/amp_state.h"
#include "hamlib/port.h"

static int test_responses(void)
{
    char normal[] =
        "BAND=14MHZ,ANTENNA=1,POWER=100W150,VSWR=1.2,CURRENT=10,"
        "TEMPERATURE=35,STATE=OPERATE,PTT=RX,TRIP=NONE\n";
    char unknown[] = "UNKNOWN=1\n";
    char malformed[] = "BAND=14,POWER=100W,PTT=\n";
    char truncated[] = "TRIP\n";
    char valid_then_truncated[] = "ANTENNA=2,TRIP\n";
    struct gemini_priv_data priv;

    memset(&priv, 0, sizeof(priv));

    if (gemini_parse_status_response(&priv, normal) != RIG_OK ||
            priv.band != 14000000 || priv.antenna != '1' ||
            priv.power_current != 100 || priv.power_peak != 150 ||
            priv.vswr != 1.2 || priv.current != 10 ||
            priv.temperature != 35 || strcmp(priv.state, "OPERATE") != 0 ||
            priv.ptt != 0 || strcmp(priv.trip, "NONE") != 0)
    {
        fprintf(stderr, "normal Gemini status response was not preserved\n");
        return 1;
    }

    if (gemini_parse_status_response(&priv, unknown) != -RIG_EPROTO)
    {
        fprintf(stderr, "unknown Gemini status field was accepted\n");
        return 1;
    }

    priv.band = 123;
    priv.power_current = 45;
    priv.power_peak = 67;
    priv.ptt = 7;

    if (gemini_parse_status_response(&priv, malformed) != -RIG_EPROTO ||
            priv.band != 123 || priv.power_current != 45 ||
            priv.power_peak != 67 || priv.ptt != 7)
    {
        fprintf(stderr, "malformed Gemini status changed parser state\n");
        return 1;
    }

    strcpy(priv.trip, "SWR");

    if (gemini_parse_status_response(&priv, truncated) != -RIG_EPROTO ||
            strcmp(priv.trip, "SWR") != 0)
    {
        fprintf(stderr, "truncated Gemini status field was accepted\n");
        return 1;
    }

    priv.antenna = '1';

    if (gemini_parse_status_response(&priv, valid_then_truncated) != RIG_OK ||
            priv.antenna != '2' || strcmp(priv.trip, "SWR") != 0)
    {
        fprintf(stderr, "valid Gemini status field before truncation was rejected\n");
        return 1;
    }

    return 0;
}

static int test_transaction_error(void)
{
    AMP *amp;
    freq_t freq;
    int retval;

    amp_register(&gemini_amp_caps);
    amp = amp_init(AMP_MODEL_GEMINI_DX1200);

    if (amp == NULL)
    {
        fprintf(stderr, "unable to initialize Gemini amplifier\n");
        return 1;
    }

    AMPPORT(amp)->fd = -1;
    retval = gemini_get_freq(amp, &freq);
    amp_cleanup(amp);

    if (retval == RIG_OK)
    {
        fprintf(stderr, "Gemini transaction error was not returned\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_responses() != 0) { return 1; }

    return test_transaction_error();
}
