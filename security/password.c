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
#include <stdlib.h>
#include <string.h>
#include "hamlib/rig.h"
#include "password.h"
#include "md5.h"

#define HAMLIB_SECRET_LENGTH 32

// makes a 32-byte secret key from password using MD5
// yes -- this is security-by-obscurity
// but password hacking software doesn't use this type of logic
// we want a repeatable password that is not subject to "normal" md5 decryption logic
// could use a MAC address to make it more random but making that portable is TBD
HAMLIB_EXPORT(void) rig_password_generate_secret(char *pass,
        char result[HAMLIB_SECRET_LENGTH + 1])
{
    unsigned int product;
    char newpass[256];
    product = pass[0];

    int i;
    for (i = 1; pass[i]; ++i)
    {
        product *= pass[i];
    }

    srand(product);

    snprintf(newpass, sizeof(newpass) - 1, "%s\t%lu\t%lu", pass, (long)rand(),
             (long)time(NULL));
    //printf("debug=%s\n", newpass);
    char *md5str = rig_make_md5(newpass);

    strncpy(result, md5str, HAMLIB_SECRET_LENGTH);

    // now that we have the md5 we'll do the AES256

    printf("Shared Secret: %s\n", result);

    printf("\nCan be used with rigctl --password [secret]\nOr can be place in ~/.hamlib_settings\n");
}

//#define TESTPASSWORD
#ifdef TESTPASSWORD
int main(int argc, char *argv[])
{
    char secret[HAMLIB_SECRET_LENGTH + 1];
    char password[HAMLIB_SECRET_LENGTH +
                                       1]; // maximum length usable for password too

    // anything longer will not be used to generate the secret
    if (argc == 1) { strcpy(password, "testpass"); }
    else { strcpy(password, argv[1]); }

    printf("Using password \"%s\" to generate shared key\n", password);
    rig_password_generate_secret(password, secret);
    return 0;
}
#endif
