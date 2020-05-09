/*
 *  Hamlib Interface - definition of radioamateur frequency band plan.
 *  Copyright (c) 2002 by Stephane Fillod
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

#ifndef _BANDPLAN_H
#define _BANDPLAN_H 1

#include <hamlib/rig.h>

/*
 * This header file is internal to Hamlib and its backends,
 * thus not part of the API.
 *
 * Note: don't change this file if ITU band plan changes. A lot of existing
 * rigs are built this way, so leave them alone.
 *
 * As a reminder:
 *  struct freq_range_list {
 *      freq_t start;
 *      freq_t end;
 *      rmode_t modes;
 *      int low_power;
 *      int high_power;
 *  vfo_t vfo;
 *  ant_t ant;
 *  };
 */


/*
 * ITU Region 1: Europe, Africa and Northern Asia
 */

/* MF: 300 kHz - 3 MHz */
#define FRQ_RNG_160m_REGION1(md,lp,hp,v,a) \
            { kHz(1810), MHz(2), (md), (lp), (hp), (v), (a), "ITU1" }

/* HF: 3 MHz - 30 MHz */
#define FRQ_RNG_80m_REGION1(md,lp,hp,v,a) \
            { kHz(3500),kHz(3800), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_60m_REGION1(md,lp,hp,v,a) \
            { kHz(5351.5),kHz(5366.5), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_40m_REGION1(md,lp,hp,v,a) \
            { MHz(7),kHz(7100), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_30m_REGION1(md,lp,hp,v,a) \
            { kHz(10100),kHz(10150), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_20m_REGION1(md,lp,hp,v,a) \
            { MHz(14),kHz(14350), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_17m_REGION1(md,lp,hp,v,a) \
            { kHz(18068),kHz(18168), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_15m_REGION1(md,lp,hp,v,a) \
            { MHz(21),kHz(21450), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_12m_REGION1(md,lp,hp,v,a) \
            { kHz(24890),kHz(24990), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_10m_REGION1(md,lp,hp,v,a) \
            { MHz(28),kHz(29700), (md), (lp), (hp), (v), (a), "ITU1" }

/* VHF: 30 MHz - 300 MHz */
#define FRQ_RNG_6m_REGION1(md,lp,hp,v,a) \
            { MHz(50),MHz(54), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_4m_REGION1(md,lp,hp,v,a) \
            { MHz(70),MHz(70.5), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_2m_REGION1(md,lp,hp,v,a) \
            { MHz(144),MHz(146), (md), (lp), (hp), (v), (a), "ITU1" }

/* UHF: 300 MHz - 3 GHz */
#define FRQ_RNG_70cm_REGION1(md,lp,hp,v,a) \
            { MHz(430),MHz(440), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_23cm_REGION1(md,lp,hp,v,a) \
            { MHz(1240),MHz(1300), (md), (lp), (hp), (v), (a), "ITU1" }

#define FRQ_RNG_13cm_REGION1(md,lp,hp,v,a) \
            { MHz(2300),MHz(2450), (md), (lp), (hp), (v), (a), "ITU1" }


/*
 * ITU Region 2: North America, South America and Greenland
 */

/* MF: 300 kHz - 3 MHz */
#define FRQ_RNG_160m_REGION2(md,lp,hp,v,a) \
            { kHz(1800),MHz(2), (md), (lp), (hp), (v), (a), "ITU2" }

/* HF: 3 MHz - 30 MHz */
#define FRQ_RNG_80m_REGION2(md,lp,hp,v,a) \
            { kHz(3500),MHz(4), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_60m_REGION2(md,lp,hp,v,a) \
            { kHz(5351.5),kHz(5366.5), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_40m_REGION2(md,lp,hp,v,a) \
            { MHz(7),kHz(7300), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_30m_REGION2(md,lp,hp,v,a) \
            { kHz(10100),kHz(10150), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_20m_REGION2(md,lp,hp,v,a) \
            { MHz(14),kHz(14350), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_17m_REGION2(md,lp,hp,v,a) \
            { kHz(18068),kHz(18168), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_15m_REGION2(md,lp,hp,v,a) \
            { MHz(21),kHz(21450), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_12m_REGION2(md,lp,hp,v,a) \
            { kHz(24890),kHz(24990), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_10m_REGION2(md,lp,hp,v,a) \
            { MHz(28),kHz(29700), (md), (lp), (hp), (v), (a), "ITU2" }

/* VHF: 30 MHz - 300 MHz */
#define FRQ_RNG_6m_REGION2(md,lp,hp,v,a) \
            { MHz(50),MHz(54), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_2m_REGION2(md,lp,hp,v,a) \
            { MHz(144),MHz(148), (md), (lp), (hp), (v), (a), "ITU2" }

/* UHF: 300 MHz - 3 GHz */
#define FRQ_RNG_70cm_REGION2(md,lp,hp,v,a) \
            { MHz(430),MHz(450), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_23cm_REGION2(md,lp,hp,v,a) \
            { MHz(1240),MHz(1300), (md), (lp), (hp), (v), (a), "ITU2" }

#define FRQ_RNG_13cm_REGION2(md,lp,hp,v,a) \
            { MHz(2320),MHz(2450), (md), (lp), (hp), (v), (a), "ITU2" }

/*
 * ITU Region 3: South Pacific and Southern Asia
 */

/* MF: 300 kHz - 3 MHz */
#define FRQ_RNG_160m_REGION3(md,lp,hp,v,a) \
            { kHz(1810),MHz(2), (md), (lp), (hp), (v), (a), "ITU3" }

/* HF: 3 MHz - 30 MHz */
#define FRQ_RNG_80m_REGION3(md,lp,hp,v,a) \
            { kHz(3500),kHz(3900), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_60m_REGION3(md,lp,hp,v,a) \
            { kHz(5351.5),kHz(5366.5), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_40m_REGION3(md,lp,hp,v,a) \
            { MHz(7),kHz(7100), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_30m_REGION3(md,lp,hp,v,a) \
            { kHz(10100),kHz(10150), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_20m_REGION3(md,lp,hp,v,a) \
            { MHz(14),kHz(14350), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_17m_REGION3(md,lp,hp,v,a) \
            { kHz(18068),kHz(18168), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_15m_REGION3(md,lp,hp,v,a) \
            { MHz(21),kHz(21450), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_12m_REGION3(md,lp,hp,v,a) \
            { kHz(24890),kHz(24990), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_10m_REGION3(md,lp,hp,v,a) \
            { MHz(28),kHz(29700), (md), (lp), (hp), (v), (a), "ITU3" }

/* VHF: 30 MHz - 300 MHz */
#define FRQ_RNG_6m_REGION3(md,lp,hp,v,a) \
            { MHz(50),MHz(54), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_2m_REGION3(md,lp,hp,v,a) \
            { MHz(144),MHz(148), (md), (lp), (hp), (v), (a), "ITU3" }

/* UHF: 300 MHz - 3 GHz */
#define FRQ_RNG_70cm_REGION3(md,lp,hp,v,a) \
            { MHz(430),MHz(440), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_23cm_REGION3(md,lp,hp,v,a) \
            { MHz(1240),MHz(1300), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_13cm_REGION3(md,lp,hp,v,a) \
            { MHz(2320),MHz(2450), (md), (lp), (hp), (v), (a), "ITU3" }


/*
 * Now we're done with boring definition
 * Let's define FRQ_RNG_HF for REGION1, FRQ_RNG_HF_REGION2,
 * and FRQ_RNG_HF_REGION3 all at once!
 * NB: FRQ_RNG_HF defines non-AM/AM freq_range for all HF bands,
 *      plus 160m which is not an HF band strictly speaking.
 */

#define FRQ_RNG_HF(r,m,lp,hp,v,a) \
    FRQ_RNG_160m_REGION##r((m), (lp), (hp), (v), (a)), \
    FRQ_RNG_80m_REGION##r((m), (lp), (hp), (v), (a)), \
    FRQ_RNG_40m_REGION##r((m), (lp), (hp), (v), (a)), \
    FRQ_RNG_30m_REGION##r((m), (lp), (hp), (v), (a)), \
    FRQ_RNG_20m_REGION##r((m), (lp), (hp), (v), (a)), \
    FRQ_RNG_17m_REGION##r((m), (lp), (hp), (v), (a)), \
    FRQ_RNG_15m_REGION##r((m), (lp), (hp), (v), (a)), \
    FRQ_RNG_12m_REGION##r((m), (lp), (hp), (v), (a)), \
    FRQ_RNG_10m_REGION##r((m), (lp), (hp), (v), (a))  \

#define FRQ_RNG_60m(r,m,lp,hp,v,a) \
    FRQ_RNG_60m_REGION##r((m), (lp), (hp), (v), (a)) \

#define FRQ_RNG_6m(r,m,lp,hp,v,a) \
    FRQ_RNG_6m_REGION##r((m), (lp), (hp), (v), (a)) \
 
#define FRQ_RNG_4m(r,m,lp,hp,v,a) \
    FRQ_RNG_4m_REGION##r((m), (lp), (hp), (v), (a)) \

#define FRQ_RNG_2m(r,m,lp,hp,v,a) \
    FRQ_RNG_2m_REGION##r((m), (lp), (hp), (v), (a)) \
 
#define FRQ_RNG_70cm(r,m,lp,hp,v,a) \
    FRQ_RNG_70cm_REGION##r((m), (lp), (hp), (v), (a)) \

#define FRQ_RNG_23cm(r,m,lp,hp,v,a) \
    FRQ_RNG_23cm_REGION##r((m), (lp), (hp), (v), (a)) \
 

#endif  /* _BANDPLAN_H */
