/*
 * password.c
 *
 * AESStringCrypt 1.1
 * Copyright (C) 2007, 2008, 2009, 2012, 2015
 *
 * Contributors:
 *     Glenn Washburn <crass@berlios.de>
 *     Paul E. Jones <paulej@packetizer.com>
 *     Mauro Gilardi <galvao.m@gmail.com>
 *
 * This software is licensed as "freeware."  Permission to distribute
 * this software in source and binary forms is hereby granted without a
 * fee.  THIS SOFTWARE IS PROVIDED 'AS IS' AND WITHOUT ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * THE AUTHOR SHALL NOT BE HELD LIABLE FOR ANY DAMAGES RESULTING FROM
 * THE USE OF THIS SOFTWARE, EITHER DIRECTLY OR INDIRECTLY, INCLUDING,
 * BUT NOT LIMITED TO, LOSS OF DATA OR DATA BEING RENDERED INACCURATE.
 *
 */

#include <stdio.h>
//#include <iconv.h>
//#include <langinfo.h>
#include <errno.h>
#include "password.h"

#ifdef NOTWORKING
/*
 *  passwd_to_utf16
 *
 *  Convert String to UTF-16LE for windows compatibility
 */
int passwd_to_utf16(char *in_passwd,
                    int length,
                    int max_length,
                    char *out_passwd)
{
    char *ic_outbuf,
         *ic_inbuf;
    iconv_t condesc;
    size_t ic_inbytesleft,
           ic_outbytesleft;

    ic_inbuf = in_passwd;
    ic_inbytesleft = length;
    ic_outbytesleft = max_length;
    ic_outbuf = out_passwd;

    if ((condesc = iconv_open("UTF-16LE", nl_langinfo(CODESET))) ==
            (iconv_t)(-1))
    {
        perror("Error in iconv_open");
        return -1;
    }

    if (iconv(condesc,
              &ic_inbuf,
              &ic_inbytesleft,
              &ic_outbuf,
              &ic_outbytesleft) == -1)
    {
        switch (errno)
        {
        case E2BIG:
            fprintf(stderr, "Error: password too long\n");
            iconv_close(condesc);
            return -1;
            break;

        default:
#if 0
            printf("EILSEQ(%d), EINVAL(%d), %d\n", EILSEQ, EINVAL, errno);
#endif
            fprintf(stderr,
                    "Error: Invalid or incomplete multibyte sequence\n");
            iconv_close(condesc);
            return -1;
        }
    }

    iconv_close(condesc);
    return (max_length - ic_outbytesleft);
}
#endif

