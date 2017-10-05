/* NT Parport Access stuff - Matthew Duggan (2002) */

/*
 * ParallelVdm Device (0x2C) is mostly undocumented, used by VDM for parallel
 * port compatibility.
 */

/*
 * Different from CTL_CODE in DDK, limited to ParallelVdm but makes this
 * code cleaner.
 */

#ifndef _PAR_NT_H

#define _PAR_NT_H

#define NT_CTL_CODE( Function ) ( (0x2C<<16) | ((Function) << 2) )

/* IOCTL codes */
#define NT_IOCTL_DATA      NT_CTL_CODE(1) /* Write Only */
#define NT_IOCTL_CONTROL   NT_CTL_CODE(2) /* Read/Write */
#define NT_IOCTL_STATUS    NT_CTL_CODE(3) /* Read Only  */


/*
 * Raw port access (PC-style port registers but within inversions)
 * Functions returning int may fail.
 */

/* The status pin functions operate in terms of these bits: */
enum ieee1284_status_bits
{
    S1284_NFAULT = 0x08,
    S1284_SELECT = 0x10,
    S1284_PERROR = 0x20,
    S1284_NACK   = 0x40,
    S1284_BUSY   = 0x80,
    /* To convert those values into PC-style register values, use this: */
    S1284_INVERTED = S1284_BUSY
};

/* The control pin functions operate in terms of these bits: */
enum ieee1284_control_bits
{
    C1284_NSTROBE   = 0x01,
    C1284_NAUTOFD   = 0x02,
    C1284_NINIT     = 0x04,
    C1284_NSELECTIN = 0x08,
    /* To convert those values into PC-style register values, use this: */
    C1284_INVERTED = (C1284_NSTROBE
                      | C1284_NAUTOFD
                      | C1284_NSELECTIN)
};

#endif  /* _PAR_NT_H */
