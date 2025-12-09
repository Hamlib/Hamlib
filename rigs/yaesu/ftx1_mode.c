/*
 * Hamlib Yaesu backend - FTX-1 Mode Commands
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * Mode commands are handled by newcat_set_mode/newcat_get_mode.
 * This file documents the FTX-1 specific mode codes for reference.
 *
 * CAT Commands:
 *   MD P1 P2;  - Operating Mode (P1=VFO 0/1, P2=mode code)
 *
 * Mode Codes (P2 for MD command):
 *   1=LSB, 2=USB, 3=CW-U, 4=FM, 5=AM, 6=RTTY-L, 7=CW-L,
 *   8=DATA-L, 9=RTTY-U, A=DATA-FM, B=FM-N, C=DATA-U, D=AM-N,
 *   E=PSK, F=DATA-FM-N, H=C4FM-DN, I=C4FM-VW
 *
 * Note: SH (Width) command - firmware doesn't persist set values (read-only).
 *       NA (Notch Auto) - handled via ftx1_noise.c
 */

/* No FTX-1 specific mode functions needed - newcat handles MD command */
