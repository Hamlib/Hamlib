/*
 *  Hamlib Rotator backend - LinuxCNC no hardware port
 *  Copyright (c) 2015 by Robert Freeman
 *  Adapted from AMSAT code by Stephane Fillod
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "hamlib/rotator.h"
#include "hamlib/rig.h"

#include "misc.h"
#include "serial.h"
#include "token.h"
#include "network.h"

#include "register.h"

#define RSIZE   (1024)

#define XDEGREE2MM(d)   ((d)/9.0)

// 42-gear-22
#define YDEGREE2MM(d)   ((d)/9.0)

//#define YDEGREE2MM(d)   ((d) * ((1710) / (77*9)))
//#define YDEGREE2MM(d)   ((d) * ((190) / (77)))

/*

$0=10 (step pulse, usec)
$1=25 (step idle delay, msec)
$2=0 (step port invert mask:00000000)
$3=0 (dir port invert mask:00000000)
$4=0 (step enable invert, bool)
$5=0 (limit pins invert, bool)
$6=0 (probe pin invert, bool)
$10=3 (status report mask:00000011)
$11=0.010 (junction deviation, mm)
$12=0.002 (arc tolerance, mm)
$13=0 (report inches, bool)
$20=0 (soft limits, bool)
$21=0 (hard limits, bool)
$22=0 (homing cycle, bool)
$23=0 (homing dir invert mask:00000000)
$24=25.000 (homing feed, mm/min)
$25=500.000 (homing seek, mm/min)
$26=250 (homing debounce, msec)
$27=1.000 (homing pull-off, mm)
$30=1000. (rpm max)
$31=0. (rpm min)
$32=0 (motor lock bool)
$33=7 (motor mode mask:00000111)
$100=80.000 (x, step/mm)
$101=80.000 (y, step/mm)
$102=80.000 (z, step/mm)
$110=10000.000 (x max rate, mm/min)
$111=10000.000 (y max rate, mm/min)
$112=10000.000 (z max rate, mm/min)
$120=250.000 (x accel, mm/sec^2)
$121=250.000 (y accel, mm/sec^2)
$122=250.000 (z accel, mm/sec^2)
$130=500.000 (x max travel, mm)
$131=500.000 (y max travel, mm)
$132=500.000 (z max travel, mm)

*/

char *grbl_get_config = "$$\r\n";
char *grbl_get_pos = "?\r\n";

char *grbl_init_list[] =
{
    "$1=255\r\n",     /* lock motors */
    "$3=3\r\n",       /* invert X and Y direction */
    //"$100=80\r\n",    /* axis-x a4988   16 microstep */
    "$100=1776\r\n",  /* axis-x a4988   16 microstep 42-gear-motor-22-(1710/77) (80*1710)/77 */
    //"$101=160\n", /* axis-y drv8825 32 microstep */
    //"$101=80\n",  /* axis-y a4988   16 microstep */
    "$101=1776\r\n",  /* axis-y a4988   16 microstep 42-gear-motor-22-(1710/77) (80*1710)/77 */
    "$110=50\r\n",
    "$111=50\r\n",
    "$120=25\r\n",
    "$121=25\r\n",
    "G90\r\n",
    "G0 X0 Y0\r\n",
};

static int
grbl_request(ROT *rot, char *request, uint32_t req_size, char *response,
             uint32_t *resp_size)
{
    int retval;
    static int fail_count = 0;

    rot_debug(RIG_DEBUG_ERR, "req: [%s][%d]\n", request, fail_count);

    if (rot->caps->rot_model == ROT_MODEL_GRBLTRK_SER
            || rot->caps->rot_model == ROT_MODEL_GRBLTRK_NET)
    {
        //fprintf(stderr, "ctrl by serial/network\n");

        if ((retval = write_block(&rot->state.rotport, (unsigned char *)request,
                                  req_size)) != RIG_OK)
        {
            rot_debug(RIG_DEBUG_ERR, "%s write_block fail!\n", __func__);
            //exit(-1);
            fail_count++;
            //return RIG_EIO;
        }
        else
        {
            fail_count = 0;
        }

        rig_flush(&rot->state.rotport);

        usleep(300000);

        if ((retval = read_string(&rot->state.rotport, (unsigned char *)response, 1024,
                                  "\n", 1, 0, 1)) < 0)
        {
            rot_debug(RIG_DEBUG_ERR, "%s read_string fail! (%d) \n", __func__, retval);
            //exit(-1);
            fail_count++;
            //return RIG_EIO;
        }
        else
        {
            fail_count = 0;
        }

        if (fail_count >= 10)
        {
            rot_debug(RIG_DEBUG_ERR, "%s too much xfer fail! exit\n", __func__);
            exit(-1);
        }

        rig_flush(&rot->state.rotport);

        rot_debug(RIG_DEBUG_ERR, "rsp: [%s]\n", response);
        //fprintf(stderr, "rsp: [%s]\n", response);

        *resp_size = retval;

    }

    return RIG_OK;
}

static int
grbl_init(ROT *rot)
{
    int i, retval;
    uint32_t init_count;
    char rsp[RSIZE];
    uint32_t resp_size;

    /* get total config */
    grbl_request(rot, grbl_get_config, strlen(grbl_get_config), rsp, &resp_size);

    if (strstr(rsp, grbl_init_list[0]) != NULL)
    {
        rot_debug(RIG_DEBUG_ERR, "%s: grbl already configured\n", __func__);
        return RIG_OK;
    }

    init_count = sizeof(grbl_init_list) / sizeof(grbl_init_list[0]);

    for (i = 0; i < init_count; i++)
    {
        rot_debug(RIG_DEBUG_ERR, "grbl_request [%s] ", grbl_init_list[i]);
        retval = grbl_request(rot, grbl_init_list[i], strlen(grbl_init_list[i]), rsp,
                              &resp_size);

        //fprintf(stderr, "done\n");
        if (retval != RIG_OK)
        {
            rot_debug(RIG_DEBUG_ERR, "grbl_request [%s] fail\n", grbl_init_list[i]);
            return RIG_EIO;
        }
    }

    return RIG_OK;
}

static int
grbltrk_rot_set_position(ROT *rot, azimuth_t curr_az, elevation_t curr_el)
{
    int i;
    int retval;
    static float prev_az, prev_el;

    static float prev_x, curr_x;
    float x[3], delta[3];

    float y;

    char req[RSIZE] = {0};
    char rsp[RSIZE] = {0};
    uint32_t rsp_size;

    float min_value;
    int   min_index;

    /* az:x: 0 - 360 */
    /* el:y: 0 - 90 */
    rot_debug(RIG_DEBUG_ERR,
              "%s: (prev_x) = (%.3f); (prev_az) = (%.3f); (prev_el) = (%.3f); (curr_az, curr_el) = (%.3f, %.3f)\n",
              __func__,
              prev_x, prev_az, prev_el, curr_az, curr_el);

    /* convert degree to mm, 360 degree = 40mm, 1 degree = 0.111mm */
    //x = az * 0.111;
    //y = el * 0.111;

    /* 360 -> 0 */
    if ((prev_az > 270 && prev_az < 360) &&
            (curr_az > 0   && curr_az < 90))
    {

        rot_debug(RIG_DEBUG_ERR, "%s:%d\n", __func__, __LINE__);

        if (prev_x >= XDEGREE2MM(270))
        {
            curr_x = XDEGREE2MM(curr_az) + XDEGREE2MM(360);
        }
        else
        {
            curr_x = XDEGREE2MM(curr_az);
        }

        /* 0 -> 360 */
    }
    else if ((prev_az > 0   && prev_az < 90) &&
             (curr_az > 270 && curr_az < 360))
    {
        rot_debug(RIG_DEBUG_ERR, "%s:%d\n", __func__, __LINE__);

        if (prev_x >= XDEGREE2MM(360))
        {
            curr_x = XDEGREE2MM(curr_az);
        }
        else
        {
            curr_x = XDEGREE2MM(curr_az) - XDEGREE2MM(360);
        }

        /* reset */
    }
    else if (curr_az == 0 && curr_el == 0)
    {
        rot_debug(RIG_DEBUG_ERR, "%s: reset\n", __func__);
        curr_x = 0;
    }
    else
    {
        rot_debug(RIG_DEBUG_ERR, "%s:%d prev_x: %.3f\n", __func__, __LINE__, prev_x);

        x[0] = XDEGREE2MM(curr_az) - XDEGREE2MM(360);
        x[1] = XDEGREE2MM(curr_az);
        x[2] = XDEGREE2MM(curr_az) + XDEGREE2MM(360);

        delta[0] = prev_x - x[0];
        delta[1] = prev_x - x[1];
        delta[2] = prev_x - x[2];

        if (delta[0] < 0) { delta[0] = -1 * delta[0]; }

        if (delta[1] < 0) { delta[1] = -1 * delta[1]; }

        if (delta[2] < 0) { delta[2] = -1 * delta[2]; }

        min_value = delta[0];
        min_index = 0;

        for (i = 0; i < 3; i++)
        {
            if (delta[i] <= min_value)
            {
                min_value = delta[i];
                min_index = i;
            }
        }

        curr_x = x[min_index];
        rot_debug(RIG_DEBUG_ERR, "min_index: %d; curr_x: %.3f\n", min_index, curr_x);
    }

    y = YDEGREE2MM(curr_el);

    /**/

    snprintf(req, sizeof(req), "G0 X%.3f Y%.3f\n", curr_x, y);

    retval = grbl_request(rot, req, strlen(req), rsp, &rsp_size);

    if (retval != RIG_OK)
    {
        return retval;
    }

    prev_az = curr_az;
    prev_el = curr_el;

    prev_x  = curr_x;
    return RIG_OK;

}

static int
grbltrk_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{

    //static char req[RSIZE];
    static char rsp[RSIZE];
    uint32_t rsp_size;

    float mpos[3];
    char dummy0[256];
    char dummy1[256];

    int retval;
    int i;

    rot_debug(RIG_DEBUG_ERR, "%s called\n", __func__);

    //snprintf(req, sizeof(req), "?\r\n");

    for (i = 0; i < 5; i++)
    {
        retval = grbl_request(rot, grbl_get_pos, strlen(grbl_get_pos), rsp, &rsp_size);

        /*FIXME: X Y safe check */

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (strstr(rsp, "MPos") == NULL)
        {
            rot_debug(RIG_DEBUG_ERR, "%s no MPos found, continue\n", __func__);
            continue;
        }

        /* grbl 0.9 mega328p */
        /* <Idle,MPos:0.000,0.000,0.000,WPos:0.000,0.000,0.000,SC:0> */
        //sscanf(rsp, "%[^','],MPos:%f,%f,%f,WPos:%f,%f,%f,%s", &dummy[0], &mpos[0], &mpos[1], &mpos[2], &wpos[0], &wpos[1], &wpos[2], &dummy[1]);

        /* grbl 1.3a esp32 */
        //<Idle|MPos:0.000,0.000,0.000|FS:0,0|Pn:P|WCO:5.000,0.000,0.000>
        sscanf(rsp, "%[^'|']|MPos:%f,%f,%s", dummy0, &mpos[0], &mpos[1], dummy1);

        //rot_debug(RIG_DEBUG_ERR, "%s: (%.3f, %.3f) (%.3f, %.3f)\n", __func__, mpos[0], mpos[1], wpos[0], wpos[1]);

        //*az = (azimuth_t) mpos[0] / 0.111;
        //*el = (elevation_t) mpos[1] / 0.111;
        *az = (azimuth_t) mpos[0] * 9;
        *el = (elevation_t) mpos[1] * 9;

        if ((*az) < 0)
        {
            (*az) = (*az) + 360;
        }

        //rot_debug(RIG_DEBUG_ERR, "%s: (az, el) = (%.3f, %.3f)\n", __func__, *az, *el);

        rot_debug(RIG_DEBUG_ERR, "%s: (az, el) = (%.3f, %.3f)\n", __func__, *az, *el);

        return RIG_OK;

    }

    *az = (azimuth_t) 0;
    *el = (elevation_t) 0;
    return RIG_OK;
}

static int
grbltrk_rot_set_conf(ROT *rot, token_t token, const char *val)
{
    int i, retval;
    char req[RSIZE] = {0};
    char rsp[RSIZE];
    uint32_t resp_size, len;

    rot_debug(RIG_DEBUG_ERR, "token: %ld; value: [%s]\n", token, val);

    len = strlen(val);

    if ((len != 0) && (val[0] == 'G'))
    {

        for (i = 0; i < len; i++)
        {

            if (val[i] == '@')
            {
                req[i] = ' ';
            }
            else
            {
                req[i] = val[i];
            }
        }

        req[i] = '\n';
        len = strlen(req);

        rot_debug(RIG_DEBUG_ERR, "send gcode [%s]\n", req);
        retval = grbl_request(rot, req, len, rsp, &resp_size);

        if (retval < 0)
        {
            rot_debug(RIG_DEBUG_ERR, "grbl_request [%s] fail\n", val);
            return RIG_EIO;
        }
    }

    return RIG_OK;
}

static int
grbltrk_rot_init(ROT *rot)
{
    int r = RIG_OK;

    rot_debug(RIG_DEBUG_ERR, "%s:%d rot->caps->rot_model: %d\n", __func__, __LINE__,
              rot->caps->rot_model);

    return r;
}

static int
grbl_net_open(ROT *rot, int port)
{
    //network_open(&rot->state.rotport, port);

    //rot_debug(RIG_DEBUG_ERR, "%s:%d network_fd: %d\n", __func__, __LINE__, (&rot->state.rotport)->fd);
    rot_debug(RIG_DEBUG_ERR, "%s:%d \n", __func__, __LINE__);

    return 0;
}

static int
grbltrk_rot_open(ROT *rot)
{
    int r = RIG_OK;
    char host[128] = {0};
    //char ip[32];
    //int port;

    //rot_debug(RIG_DEBUG_ERR, "%s:%d rot->caps->rot_model: %d\n", __func__, __LINE__, rot->caps->rot_model);
    if (rot->caps->rot_model == ROT_MODEL_GRBLTRK_SER)
    {
        rot_debug(RIG_DEBUG_ERR, "%s:%d ctrl via serial\n", __func__, __LINE__);
    }
    else if (rot->caps->rot_model == ROT_MODEL_GRBLTRK_NET)
    {
        rot_get_conf(rot, TOK_PATHNAME, host);
        rot_debug(RIG_DEBUG_ERR, "%s:%d ctrl via net, host [%s]\n", __func__, __LINE__,
                  host);
        grbl_net_open(rot, 23);

#if 0

        if (sscanf(host, "%[^:]:%d", ip, &port) == 2)
        {
            grbl_net_open(rot, ip, port);
        }
        else
        {
            grbl_net_open(rot, NULL, 0); /* use default ip & port */
        }

#endif

    }

    grbl_init(rot);

    //rot_debug(RIG_DEBUG_ERR, "%s:%d\n", __func__, __LINE__);

    return r;
}

static void
grbl_net_close(ROT *rot)
{
    port_close(&rot->state.rotport, RIG_PORT_NETWORK);
}

static int
grbltrk_rot_close(ROT *rot)
{
    int r = RIG_OK;

    if (rot->caps->rot_model == ROT_MODEL_GRBLTRK_SER)
    {
        rot_debug(RIG_DEBUG_ERR, "%s:%d\n", __func__, __LINE__);
    }
    else if (rot->caps->rot_model == ROT_MODEL_GRBLTRK_NET)
    {
        rot_debug(RIG_DEBUG_ERR, "%s:%d\n", __func__, __LINE__);
        grbl_net_close(rot);
    }

    rot_debug(RIG_DEBUG_ERR, "%s:%d\n", __func__, __LINE__);

    return r;
}


/** CNCTRK implements essentially only the set position function.
    it assumes there is a LinuxCNC running with the Axis GUI */
const struct rot_caps grbltrk_serial_rot_caps =
{
    ROT_MODEL(ROT_MODEL_GRBLTRK_SER),
    .model_name =     "GRBLTRK via Serial",
    .mfg_name =       "BG5DIW",
    .version =        "20220515.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   9600,
    .serial_rate_max =   115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  400,
    .retry =  0,


    .min_az =     0,
    .max_az =     360,
    .min_el =     0,
    .max_el =     90,

    .rot_init     =  grbltrk_rot_init,
    .rot_open     =  grbltrk_rot_open,

    .set_position =  grbltrk_rot_set_position,
    .get_position =  grbltrk_rot_get_position,
    .set_conf    =  grbltrk_rot_set_conf,
};

const struct rot_caps grbltrk_net_rot_caps =
{
    ROT_MODEL(ROT_MODEL_GRBLTRK_NET),
    .model_name =     "GRBLTRK via Net",
    .mfg_name =       "BG5DIW",
    .version =        "20220515.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_NETWORK, /* RIG_PORT_NONE */
    //.port_type =      RIG_PORT_NONE, /* RIG_PORT_NONE */

    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  300,
    .retry =  0,
    //.retry =  3,


    .min_az =     0,
    .max_az =     360,
    .min_el =     0,
    .max_el =     90,

    .rot_init     =  grbltrk_rot_init,
    .rot_open     =  grbltrk_rot_open,
    .rot_close    =  grbltrk_rot_close,

    .set_position =  grbltrk_rot_set_position,
    .get_position =  grbltrk_rot_get_position,
    .set_conf    =  grbltrk_rot_set_conf,
};


/* ************************************************************************* */

DECLARE_INITROT_BACKEND(grbltrk)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    //rot_debug(RIG_DEBUG_ERR, "%s: _init called\n", __func__);

    rot_register(&grbltrk_serial_rot_caps);

    rot_register(&grbltrk_net_rot_caps);

    return RIG_OK;
}
