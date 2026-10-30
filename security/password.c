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

#include <stdlib.h>
#include <string.h>
#include "hamlib/rig.h"
#include "password.h"
#include "md5.h"

HAMLIB_EXPORT(void) rig_password_generate_secret(const char *pass,
        char result[HAMLIB_SECRET_LENGTH + 1])
{
    char *secret;

    if (result == NULL)
    {
        return;
    }

    result[0] = '\0';

    if (pass == NULL)
    {
        return;
    }

    secret = rig_make_md5(pass);

    if (secret == NULL)
    {
        return;
    }

    memcpy(result, secret, HAMLIB_SECRET_LENGTH + 1);
    free(secret);
}
