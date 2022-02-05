/*
 * rigmatrix.c - Copyright (C) 2000-2012 Stephane Fillod
 * This program generates the supported rig matrix in HTML format.
 * The code is rather ugly since this is only a try out.
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <gd.h>
#include <gdfontg.h>
#include <gdfonts.h>

#include <hamlib/rig.h>
#include "misc.h"

static setting_t bitmap_func, bitmap_level, bitmap_parm;

int create_png_range(const freq_range_t rx_range_list[],
                     const freq_range_t tx_range_list[], int num);

int print_caps_sum(const struct rig_caps *caps, void *data)
{

    printf("<TR><TD><A HREF=\"support/model%u.txt\">%s</A></TD><TD>%s</TD>"
           "<TD>%s</TD><TD>%s</TD><TD>",
           caps->rig_model,
           caps->model_name,
           caps->mfg_name,
           caps->version,
           rig_strstatus(caps->status));

    switch (caps->rig_type & RIG_TYPE_MASK)
    {
    case RIG_TYPE_TRANSCEIVER:
        printf("Transceiver");
        break;

    case RIG_TYPE_HANDHELD:
        printf("Handheld");
        break;

    case RIG_TYPE_MOBILE:
        printf("Mobile");
        break;

    case RIG_TYPE_RECEIVER:
        printf("Receiver");
        break;

    case RIG_TYPE_PCRECEIVER:
        printf("PC Receiver");
        break;

    case RIG_TYPE_SCANNER:
        printf("Scanner");
        break;

    case RIG_TYPE_TRUNKSCANNER:
        printf("Trunking scanner");
        break;

    case RIG_TYPE_COMPUTER:
        printf("Computer");
        break;

    case RIG_TYPE_OTHER:
        printf("Other");
        break;

    default:
        printf("Unknown");
    }

    printf("</TD><TD><A HREF=\"#rng%u\">range</A></TD>"
           "<TD><A HREF=\"#parms%u\">parms</A></TD>"
           "<TD><A HREF=\"#caps%u\">caps</A></TD>"
           "<TD><A HREF=\"#getfunc%u\">funcs</A></TD>"
           "<TD><A HREF=\"#setfunc%u\">funcs</A></TD>"
           "<TD><A HREF=\"#getlevel%u\">levels</A></TD>"
           "<TD><A HREF=\"#setlevel%u\">levels</A></TD>"
           "<TD><A HREF=\"#getparm%u\">parms</A></TD>"
           "<TD><A HREF=\"#setparm%u\">parms</A></TD>"
           "</TR>\n",
           caps->rig_model, caps->rig_model, caps->rig_model,
           caps->rig_model, caps->rig_model, caps->rig_model,
           caps->rig_model, caps->rig_model, caps->rig_model);

    return 1;   /* !=0, we want them all ! */
}


/*
 * IO params et al.
 */
int print_caps_parameters(const struct rig_caps *caps, void *data)
{
    printf("<A NAME=\"parms%u\"><TR><TD>%s</TD><TD>",
           caps->rig_model,
           caps->model_name);

    switch (caps->ptt_type)
    {
    case RIG_PTT_RIG:
        printf("rig");
        break;

    case RIG_PTT_PARALLEL:
        printf("parallel");
        break;

    case RIG_PTT_SERIAL_RTS:
    case RIG_PTT_SERIAL_DTR:
        printf("serial");
        break;

    case RIG_PTT_NONE:
        printf("None");
        break;

    default:
        printf("Unknown");
    }

    printf("</TD><TD>");

    switch (caps->dcd_type)
    {
    case RIG_DCD_RIG:
        printf("rig");
        break;

    case RIG_DCD_PARALLEL:
        printf("parallel");
        break;

    case RIG_DCD_SERIAL_CTS:
    case RIG_DCD_SERIAL_DSR:
        printf("serial");
        break;

    case RIG_DCD_NONE:
        printf("None");
        break;

    default:
        printf("Unknown");
    }

    printf("</TD><TD>");

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        printf("serial");
        break;

    case RIG_PORT_DEVICE:
        printf("device");
        break;

    case RIG_PORT_NETWORK:
        printf("network");
        break;

    case RIG_PORT_UDP_NETWORK:
        printf("UDP network");
        break;

    case RIG_PORT_NONE:
        printf("None");
        break;

    default:
        printf("Unknown");
    }

    printf("</TD><TD>%d</TD><TD>%d</TD><TD>%d%c%d</TD><TD>%s</TD>",
           caps->serial_rate_min,
           caps->serial_rate_max,
           caps->serial_data_bits,
           caps->serial_parity == RIG_PARITY_NONE ? 'N' :
           caps->serial_parity == RIG_PARITY_ODD ? 'O' :
           caps->serial_parity == RIG_PARITY_EVEN ? 'E' :
           caps->serial_parity == RIG_PARITY_MARK ? 'M' : 'S',
           caps->serial_stop_bits,
           caps->serial_handshake == RIG_HANDSHAKE_NONE ? "none" :
           (caps->serial_handshake == RIG_HANDSHAKE_XONXOFF ? "XONXOFF" : "CTS/RTS"));

    printf("<TD>%dms</TD><TD>%dms</TD><TD>%dms</TD><TD>%d</TD></TR></A>\n",
           caps->write_delay,
           caps->post_write_delay,
           caps->timeout,
           caps->retry);

    return 1;
}

/* used by print_caps_caps and print_caps_level */
#define print_yn(fn) printf("<TD>%c</TD>", (fn) ? 'Y':'N')

/*
 * backend functions defined
 *
 * TODO: add new API calls!
 */
int print_caps_caps(const struct rig_caps *caps, void *data)
{
    printf("<A NAME=\"caps%u\"><TR><TD>%s</TD>",
           caps->rig_model,
           caps->model_name);

    /* targetable_vfo is not a function, but a boolean */
    print_yn(caps->targetable_vfo);

    print_yn(caps->set_freq);
    print_yn(caps->get_freq);
    print_yn(caps->set_mode);
    print_yn(caps->get_mode);
    print_yn(caps->set_vfo);
    print_yn(caps->get_vfo);
    print_yn(caps->set_ptt);
    print_yn(caps->get_ptt);
    print_yn(caps->get_dcd);
    print_yn(caps->set_rptr_shift);
    print_yn(caps->get_rptr_shift);
    print_yn(caps->set_rptr_offs);
    print_yn(caps->get_rptr_offs);
    print_yn(caps->set_split_freq);
    print_yn(caps->get_split_freq);
    print_yn(caps->set_split_vfo);
    print_yn(caps->get_split_vfo);
    print_yn(caps->set_ts);
    print_yn(caps->get_ts);
    print_yn(caps->set_ctcss_tone);
    print_yn(caps->get_ctcss_tone);
    print_yn(caps->set_dcs_code);
    print_yn(caps->get_dcs_code);
    print_yn(caps->set_powerstat);
    print_yn(caps->get_powerstat);
    print_yn(caps->set_trn);
    print_yn(caps->set_trn);
    print_yn(caps->decode_event);
    print_yn(caps->get_info);

    printf("</TR></A>\n");

    return 1;
}


/*
 * Get/Set parm abilities
 */
int print_caps_parm(const struct rig_caps *caps, void *data)
{
    setting_t parm;
    int i;

    if (!data)
    {
        return 0;
    }

    parm = (*(int *)data) ? caps->has_set_parm : caps->has_get_parm;

    printf("<A NAME=\"%sparm%u\"><TR><TD>%s</TD>",
           (*(int *)data) ? "set" : "get",
           caps->rig_model,
           caps->model_name);

    /*
     * bitmap_parm: only those who have a label
     */
    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        if (rig_idx2setting(i) & bitmap_parm)
        {
            print_yn(parm & rig_idx2setting(i));
        }
    }

    printf("</TR></A>\n");

    return 1;
}


/*
 * Get/Set level abilities
 */
int print_caps_level(const struct rig_caps *caps, void *data)
{
    setting_t level;
    int i;

    if (!data)
    {
        return 0;
    }

    level = (*(int *)data) ? caps->has_set_level : caps->has_get_level;

    printf("<A NAME=\"%slevel%u\"><TR><TD>%s</TD>",
           (*(int *)data) ? "set" : "get",
           caps->rig_model,
           caps->model_name);

    /*
     * bitmap_level: only those who have a label
     */
    for (i = 0; i < 32; i++)
    {
        if (rig_idx2setting(i) & bitmap_level)
        {
            print_yn(level & rig_idx2setting(i));
        }
    }

    printf("</TR></A>\n");

    return 1;
}


/*
 * Get/Set func abilities
 */
int print_caps_func(const struct rig_caps *caps, void *data)
{
    setting_t func;
    int i;

    if (!data)
    {
        return 0;
    }

    func = (*(int *)data) ? caps->has_set_func : caps->has_get_func;

    printf("<A NAME=\"%sfunc%u\"><TR><TD>%s</TD>",
           (*(int *)data) ? "set" : "get",
           caps->rig_model,
           caps->model_name);

    /*
     * bitmap_func: only those who have a label
     */
    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        if (rig_idx2setting(i) & bitmap_func)
        {
            print_yn(func & rig_idx2setting(i));
        }
    }

    printf("</TR></A>\n");

    return 1;
}


/*
 * inlined PNG graphics
 *
 * FIXME: default output pics is for region2: add region 1 too!
 */
int print_caps_range(const struct rig_caps *caps, void *data)
{
    create_png_range(caps->rx_range_list2, caps->tx_range_list2,
                     caps->rig_model);

    printf("<A NAME=\"rng%u\"><TR><TD>%s</TD>"
           "<TD><IMG SRC=\"range%u.png\"></TD></TR></A>",
           caps->rig_model,
           caps->model_name,
           caps->rig_model);
    return 1;
}


#define RANGE_WIDTH 600
#define RANGE_HEIGHT 16
#define RANGE_MIDHEIGHT (RANGE_HEIGHT/2)

#define RX_R 0
#define RX_G 255
#define RX_B 255
#define TX_R 0
#define TX_G 0
#define TX_B 255

#define HF_H 10
#define VHF_H 30
#define UHF_H 50
#define IM_LGD 21


static void draw_range(const freq_range_t range_list[],
                       gdImagePtr im_rng,
                       int h1,
                       int h2,
                       int rgb)
{
    int i;

    for (i = 0; i < HAMLIB_FRQRANGESIZ; i++)
    {
        float start_pix, end_pix;

        if (range_list[i].startf == 0 && range_list[i].endf == 0)
        {
            break;
        }

        start_pix = range_list[i].startf;
        end_pix = range_list[i].endf;

        /*
         * HF
         */
        if (range_list[i].startf < MHz(30))
        {
            start_pix = start_pix / MHz(30) * RANGE_WIDTH;
            end_pix = end_pix / MHz(30) * RANGE_WIDTH;

            if (end_pix >= RANGE_WIDTH)
            {
                end_pix = RANGE_WIDTH - 1;
            }

            start_pix += IM_LGD;
            end_pix += IM_LGD;
            gdImageFilledRectangle(im_rng,
                                   start_pix,
                                   HF_H + h1,
                                   end_pix,
                                   HF_H + h2,
                                   rgb);
        }

        /*
         * VHF
         */
        start_pix = range_list[i].startf;
        end_pix = range_list[i].endf;

        if ((range_list[i].startf > MHz(30) && range_list[i].startf < MHz(300))
                || (range_list[i].startf < MHz(30)
                    && range_list[i].endf > MHz(30)))
        {

            start_pix = (start_pix - MHz(30)) / MHz(300) * RANGE_WIDTH;
            end_pix = (end_pix - MHz(30)) / MHz(300) * RANGE_WIDTH;

            if (start_pix < 0)
            {
                start_pix = 0;
            }

            if (end_pix >= RANGE_WIDTH)
            {
                end_pix = RANGE_WIDTH - 1;
            }

            start_pix += IM_LGD;
            end_pix += IM_LGD;
            gdImageFilledRectangle(im_rng,
                                   start_pix,
                                   VHF_H + h1,
                                   end_pix,
                                   VHF_H + h2,
                                   rgb);
        }

        /*
         * UHF
         */
        start_pix = range_list[i].startf;
        end_pix = range_list[i].endf;

        if ((range_list[i].startf > MHz(300) && range_list[i].startf < GHz(3))
                || (range_list[i].startf < MHz(300)
                    && range_list[i].endf > MHz(300)))
        {

            start_pix = (start_pix - MHz(300)) / GHz(3) * RANGE_WIDTH;
            end_pix = (end_pix - MHz(300)) / GHz(3) * RANGE_WIDTH;

            if (start_pix < 0)
            {
                start_pix = 0;
            }

            if (end_pix >= RANGE_WIDTH)
            {
                end_pix = RANGE_WIDTH - 1;
            }

            start_pix += IM_LGD;
            end_pix += IM_LGD;
            gdImageFilledRectangle(im_rng,
                                   start_pix,
                                   UHF_H + h1,
                                   end_pix,
                                   UHF_H + h2,
                                   rgb);
        }

    }
}


int create_png_range(const freq_range_t rx_range_list[],
                     const freq_range_t tx_range_list[],
                     int num)
{
    FILE *out;

    /* Input and output images */
    gdImagePtr im_rng;
    char rng_fname[128];

    /* Color indexes */
#if 0
    int white;
#endif
    int black;
    int rx_rgb, tx_rgb;

    /* Create output image, x by y pixels. */
    im_rng  = gdImageCreate(RANGE_WIDTH + IM_LGD, UHF_H + RANGE_HEIGHT);

    /* First color allocated is background. */
//    white = gdImageColorAllocate(im_rng, 255, 255, 255);
    gdImageColorAllocate(im_rng, 255, 255, 255);
    black = gdImageColorAllocate(im_rng, 0, 0, 0);

#if 0
    /* Set transparent color. */
    gdImageColorTransparent(im_rng, white);
#endif

    /* Try to load demoin.png and paste part of it into the
        output image. */

    tx_rgb = gdImageColorAllocate(im_rng, TX_R, TX_G, TX_B);
    rx_rgb = gdImageColorAllocate(im_rng, RX_R, RX_G, RX_B);

    draw_range(rx_range_list, im_rng, 0, RANGE_MIDHEIGHT - 1, rx_rgb);
    draw_range(tx_range_list, im_rng, RANGE_MIDHEIGHT, RANGE_HEIGHT - 1, tx_rgb);

    gdImageRectangle(im_rng,
                     IM_LGD,
                     HF_H,
                     IM_LGD + RANGE_WIDTH - 1,
                     HF_H + RANGE_HEIGHT - 1,
                     black);

    gdImageRectangle(im_rng,
                     IM_LGD,
                     VHF_H,
                     IM_LGD + RANGE_WIDTH - 1,
                     VHF_H + RANGE_HEIGHT - 1,
                     black);

    gdImageRectangle(im_rng,
                     IM_LGD,
                     UHF_H,
                     IM_LGD + RANGE_WIDTH - 1,
                     UHF_H + RANGE_HEIGHT - 1,
                     black);

    /* gdImageStringUp */
    gdImageString(im_rng,
                  gdFontSmall,
                  1,
                  HF_H + 1,
                  (unsigned char *) "HF",
                  black);

    gdImageString(im_rng,
                  gdFontSmall,
                  1,
                  VHF_H + 1,
                  (unsigned char *) "VHF",
                  black);

    gdImageString(im_rng,
                  gdFontSmall,
                  1,
                  UHF_H + 1,
                  (unsigned char *) "UHF",
                  black);

    /* Make output image interlaced (allows "fade in" in some viewers,
        and in the latest web browsers) */
    gdImageInterlace(im_rng, 1);

    snprintf(rng_fname, sizeof(rng_fname), "range%d.png", num);
    out = fopen(rng_fname, "wb");

    /* Write PNG */
    gdImagePng(im_rng, out);
    fclose(out);
    gdImageDestroy(im_rng);

    return 0;
}


int main(int argc, char *argv[])
{
    time_t gentime;
    int set_or_get;
    int i, nbytes, nbytes_total = 0;
    char *pbuf, prntbuf[4096];

    rig_load_all_backends();


    printf("<TABLE BORDER=1>");
    printf("<TR><TD>Model</TD><TD>Mfg</TD><TD>Vers.</TD><TD>Status</TD>"
           "<TD>Type</TD><TD>Freq. range</TD><TD>Parameters</TD>"
           "<TD>Capabilities</TD>"
           "<TD>Get func</TD>"
           "<TD>Set func</TD>"
           "<TD>Get level</TD>"
           "<TD>Set level</TD>"
           "<TD>Get parm</TD>"
           "<TD>Set parm</TD>"
           "</TR>\n");
    rig_list_foreach(print_caps_sum, NULL);
    printf("</TABLE>\n");

    printf("<P>");

    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD><TD>PTT</TD><TD>DCD</TD><TD>Port</TD>"
           "<TD>Speed min</TD><TD>Speed max</TD>"
           "<TD>Parm.</TD><TD>Handshake</TD><TD>Write delay</TD>"
           "<TD>Post delay</TD><TD>Timeout</TD><TD>Retry</TD></TR>\n");
    rig_list_foreach(print_caps_parameters, NULL);
    printf("</TABLE>\n");

    printf("<P>");

    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD><TD>Freq. range</TD></TR>\n");
    rig_list_foreach(print_caps_range, NULL);
    printf("</TABLE>\n");

    printf("<P>");

    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD><TD>Target VFO</TD>"
           "<TD>Set freq</TD><TD>Get freq</TD>"
           "<TD>Set mode</TD><TD>Get mode</TD>"
           "<TD>Set VFO</TD><TD>Get VFO</TD>"
           "<TD>Set PTT</TD><TD>Get PTT</TD><TD>Get DCD</TD>"
           "<TD>Set rptr shift</TD><TD>Get rptr shift</TD>"
           "<TD>Set rptr offs</TD><TD>Get rptr offs</TD>"
           "<TD>Set split frq</TD><TD>Get split frq</TD>"
           "<TD>Set split</TD><TD>Get split</TD>"
           "<TD>Set ts</TD><TD>Get ts</TD>"
           "<TD>Set CTCSS</TD><TD>Get CTCSS</TD>"
           "<TD>Set DCS</TD><TD>Get DCS</TD>"
           "<TD>Set Power Stat</TD><TD>Get Power Stat</TD>"
           "<TD>Set trn</TD><TD>Get trn</TD>"
           "<TD>Decode</TD><TD>Get info</TD>"
           "</TR>\n");
    rig_list_foreach(print_caps_caps, NULL);
    printf("</TABLE>\n");

    printf("<P>");

    bitmap_func = 0;
    prntbuf[0] = '\0';
    pbuf = prntbuf;

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        setting_t func = rig_idx2setting(i);
        const char *s = rig_strfunc(func);

        if (!s)
        {
            continue;
        }

        bitmap_func |= func;
        nbytes = strlen("<TD></TD>") + strlen(s) + 1;
        nbytes_total += nbytes;
        pbuf += snprintf(pbuf, sizeof(pbuf) - nbytes_total, "<TD>%s</TD>", s);

        if (strlen(pbuf) > sizeof(pbuf) + nbytes)
        {
            printf("Buffer overflow in %s\n", __func__);
        }
    }

    printf("Has set func");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD>%s</TR>\n", prntbuf);
    set_or_get = 1;
    rig_list_foreach(print_caps_func, &set_or_get);
    printf("</TABLE>\n");

    printf("<P>");

    printf("Has get func");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD>%s</TR>\n", prntbuf);
    set_or_get = 0;
    rig_list_foreach(print_caps_func, &set_or_get);
    printf("</TABLE>\n");

    printf("<P>");

    bitmap_level = 0;
    prntbuf[0] = '\0';
    pbuf = prntbuf;

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        setting_t level = rig_idx2setting(i);
        const char *s = rig_strlevel(level);

        if (!s)
        {
            continue;
        }

        bitmap_level |= level;
        nbytes = strlen("<TD></TD>") + strlen(s) + 1;
        nbytes_total += nbytes;
        pbuf += snprintf(pbuf, sizeof(pbuf) - nbytes_total, "<TD>%s</TD>", s);

        if (strlen(pbuf) > sizeof(pbuf) + nbytes)
        {
            printf("Buffer overflow in %s\n", __func__);
        }
    }

    printf("Set level");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD>%s</TR>\n", prntbuf);
    set_or_get = 1;
    rig_list_foreach(print_caps_level, &set_or_get);
    printf("</TABLE>\n");

    printf("<P>");

    printf("Get level");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD>%s</TR>\n", prntbuf);
    set_or_get = 0;
    rig_list_foreach(print_caps_level, &set_or_get);
    printf("</TABLE>\n");

    printf("<P>");

    bitmap_parm = 0;
    prntbuf[0] = '\0';
    pbuf = prntbuf;

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        setting_t parm = rig_idx2setting(i);
        const char *s = rig_strparm(parm);

        if (!s)
        {
            continue;
        }

        bitmap_parm |= parm;
        nbytes = strlen("<TD></TD>") + strlen(s) + 1;
        nbytes_total += nbytes;
        pbuf += snprintf(pbuf, sizeof(pbuf) - nbytes_total, "<TD>%s</TD>", s);

        if (strlen(pbuf) > sizeof(pbuf) + nbytes)
        {
            printf("Buffer overflow in %s\n", __func__);
        }
    }

    printf("Set parm");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD>%s</TR>\n", prntbuf);
    set_or_get = 1;
    rig_list_foreach(print_caps_parm, &set_or_get);
    printf("</TABLE>\n");

    printf("<P>");

    printf("Get parm");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TD>Model</TD>%s</TR>\n", prntbuf);
    set_or_get = 0;
    rig_list_foreach(print_caps_parm, &set_or_get);
    printf("</TABLE>\n");

    printf("<P>");

    time(&gentime);
    printf("Rigmatrix generated %s\n", ctime(&gentime));

    printf("</body></html>\n");

    return 0;
}
