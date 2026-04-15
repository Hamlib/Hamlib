/*
 * Hamlib Yaesu backend - FTX-1 Mode Commands
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * Mode commands are mostly handled by newcat_set_mode/newcat_get_mode.
 * This file adds an FTX-1 set_mode wrapper that forces the radio out
 * of Memory mode before delegating to newcat, because Memory-mode MD
 * sets on the Main side are accepted by firmware but treated as a
 * transient memory-tune overlay that does not persist when the user
 * leaves the channel.
 *
 * CAT Commands:
 *   MD P1 P2;  - Operating Mode (P1=VFO 0/1, P2=mode code)
 *
 * Mode Codes (P2 for MD command):
 *   1=LSB, 2=USB, 3=CW-U, 4=FM, 5=AM, 6=RTTY-L, 7=CW-L,
 *   8=DATA-L, 9=RTTY-U, A=DATA-FM, B=FM-N, C=DATA-U, D=AM-N,
 *   E=PSK, F=DATA-FM-N, H=C4FM-DN, I=C4FM-VW
 *
 * Note: SH (Width) command - read-write per CAT manual (Set/Read/Answer).
 *       NA (Notch Auto) - handled via ftx1_noise.c
 */

#include <hamlib/rig.h>
#include "newcat.h"
#include "ftx1.h"

int ftx1_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    /* Memory-mode MD sets on Main are accepted but do not persist —
     * they act as a transient memory-tune overlay. Force an exit so
     * the user's mode change actually sticks. */
    ftx1_ensure_vfo_mode(rig);
    return newcat_set_mode(rig, vfo, mode, width);
}
