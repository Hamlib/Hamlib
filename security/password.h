/*
 * password.h
 *
 * AESStringCrypt 1.1
 * Copyright (C) 2007, 2008, 2009, 2012, 2015
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

#ifndef __STRINGCRYPT_PASSWORD_H
#define __STRINGCRYPT_PASSWORD_H

#define MAX_PASSWD_LEN  1024

#ifdef NOTWORKING
/*
 * Function Prototypes
 */
int passwd_to_utf16(char *in_passwd,
                    int length,
                    int max_length,
                    char *out_passwd);
#endif

#endif /* __STRINGCRYPT_PASSWORD_H */
