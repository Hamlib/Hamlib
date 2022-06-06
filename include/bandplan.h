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
 * Generic bands -- eventually we should get rid of all the ITU regions -- not Hamlib's job and subject to change -- nobody in the mainstream uses these ranges anyways that we know of -- Mike W9MDB 20220531
 */
#if 0 // future use
/* MF: 300 kHz - 3 MHz */
#define FRQ_RNG_160m(md,lp,hp,v,a) \
            { kHz(1800),MHz(2), (md), (lp), (hp), (v), (a), "Generic" }

/* HF: 3 MHz - 30 MHz */
#define FRQ_RNG_80m(md,lp,hp,v,a) \
            { kHz(3500),MHz(4), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_60m(md,lp,hp,v,a) \
            { kHz(5351.5),kHz(5366.5), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_40m(md,lp,hp,v,a) \
            { MHz(7),kHz(7300), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_30m(md,lp,hp,v,a) \
            { kHz(10100),kHz(10150), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_20m(md,lp,hp,v,a) \
            { MHz(14),kHz(14350), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_17m(md,lp,hp,v,a) \
            { kHz(18068),kHz(18168), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_15m(md,lp,hp,v,a) \
            { MHz(21),kHz(21450), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_12m(md,lp,hp,v,a) \
            { kHz(24890),kHz(24990), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_10m(md,lp,hp,v,a) \
            { MHz(28),kHz(29700), (md), (lp), (hp), (v), (a), "Generic" }

/* VHF: 30 MHz - 300 MHz */
#define FRQ_RNG_6m(md,lp,hp,v,a) \
            { MHz(50),MHz(54), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_4m(md,lp,hp,v,a) \
            { MHz(70),MHz(70.5), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_2m(md,lp,hp,v,a) \
            { MHz(144),MHz(148), (md), (lp), (hp), (v), (a), "Generic" }

/* UHF: 300 MHz - 3 GHz */
#define FRQ_RNG_70cm(md,lp,hp,v,a) \
            { MHz(430),MHz(450), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_23cm(md,lp,hp,v,a) \
            { MHz(1240),MHz(1300), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_13cm(md,lp,hp,v,a) \
            { MHz(2320),MHz(2450), (md), (lp), (hp), (v), (a), "Generic" }

#endif


/* SHF: 3 GHz to 30GHz */
#define FRQ_RNG_9cm(md,lp,hp,v,a) \
            { MHz(3300),MHz(3500), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_6cm(md,lp,hp,v,a) \
            { MHz(5650),MHz(5925), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_3cm(md,lp,hp,v,a) \
            { MHz(10000),MHz(10500), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_1_2mm(md,lp,hp,v,a) \
            { MHz(24000),MHz(24250), (md), (lp), (hp), (v), (a), "Generic" }

/* EHF: 30 Ghz to 300 GHz */
#define FRQ_RNG_6mm(md,lp,hp,v,a) \
            { MHz(47000),MHz(47200), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_4mm(md,lp,hp,v,a) \
            { MHz(75500),MHz(81000), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_2_5mm(md,lp,hp,v,a) \
            { MHz(119980),MHz(123000), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_2mm(md,lp,hp,v,a) \
            { MHz(134000),MHz(149000), (md), (lp), (hp), (v), (a), "Generic" }

#define FRQ_RNG_1mm(md,lp,hp,v,a) \
            { MHz(241000),MHz(250000), (md), (lp), (hp), (v), (a), "Generic" }

/* THF: 0.3 THz to 3 THz */

#define FRQ_RNG_THF(md,lp,hp,v,a) \
            { GHz(300),GHz(3000), (md), (lp), (hp), (v), (a), "Generic" }

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
            { MHz(7),kHz(7200), (md), (lp), (hp), (v), (a), "ITU1" }

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

#define FRQ_RNG_4m_REGION2(md,lp,hp,v,a) \
            { MHz(70),MHz(70.5), (md), (lp), (hp), (v), (a), "ITU2" }

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
 * https://web.archive.org/web/20171216012537/http://www.iaru-r3.org/wp-content/files/R3-004%20Band%20Plans%20IARU%20Region%203.docx
 */

/* MF: 300 kHz - 3 MHz */
#define FRQ_RNG_160m_REGION3(md,lp,hp,v,a) \
            { kHz(1800),MHz(2), (md), (lp), (hp), (v), (a), "ITU3" }

/* HF: 3 MHz - 30 MHz */
#define FRQ_RNG_80m_REGION3(md,lp,hp,v,a) \
            { kHz(3500),kHz(3900), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_60m_REGION3(md,lp,hp,v,a) \
            { kHz(5351.5),kHz(5366.5), (md), (lp), (hp), (v), (a), "ITU3" }

#define FRQ_RNG_40m_REGION3(md,lp,hp,v,a) \
            { MHz(7),kHz(7300), (md), (lp), (hp), (v), (a), "ITU3" }

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
            { MHz(430),MHz(450), (md), (lp), (hp), (v), (a), "ITU3" }

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
