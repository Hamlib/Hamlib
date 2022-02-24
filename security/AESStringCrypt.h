/*
 * AESStringCrypt.h
 *
 * AES String Crypt 1.1
 * Copyright (C) 2007, 2008, 2009, 2012, 2015
 *
 * Author: Paul E. Jones <paulej@packetizer.com>
 *
 * This library will encrypt octet strings of the specified length up
 * to ULLONG_MAX - 70 octet in length.  If there is an error, the return
 * value from the encryption or decryption function will be
 * AESSTRINGCRYPT_ERROR.  Any other value, including zero, is a valid
 * length value.  Note that an encrypted string can be up to 69 octets
 * longer than the original plaintext string, thus the restriction on the
 * input string size.
 *
 * The output of the string encryption function is a string that is
 * compliant with the AES Crypt version 0 file format.  For reference,
 * see: https://www.aescrypt.com/aes_file_format.html.
 *
 * This software is licensed as "freeware."  Permission to distribute
 * this software in source and binary forms is hereby granted without a
 * fee.  THIS SOFTWARE IS PROVIDED 'AS IS' AND WITHOUT ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * THE AUTHOR SHALL NOT BE HELD LIABLE FOR ANY DAMAGES RESULTING FROM
 * THE USE OF THIS SOFTWARE, EITHER DIRECTLY OR INDIRECTLY, INCLUDING,
 * BUT NOT LIMITED TO, LOSS OF DATA OR DATA BEING RENDERED INACCURATE.
 */

#ifndef __STRINGCRYPT_H
#define __STRINGCRYPT_H

#include <limits.h>

#include "aes.h"
#include "sha256.h"

/*
 * Define the value to return to indicate an error
 */
#define AESSTRINGCRYPT_ERROR -1

typedef unsigned char sha256_t[32];

/*
 *  AESStringCrypt
 *
 *  Description
 *      This function is called to encrypt the string "plaintext".
 *      The encrypted string is placed in "ciphertext".  Note that
 *      the encrypted string is up to 68 bytes larger than the
 *      plaintext string.  This is to accomodate the header defined
 *      by "AES Crypt File Format 0" and to store the last cipher
 *      block (which is padded to 16 octets).
 *
 *  Parameters
 *      password [in]
 *          The password used to encrypt the string in UCS-16
 *          format.
 *      password_length [in]
 *          The length of the password in octets
 *      plaintext [in]
 *          The plaintext string to be encrypted
 *      plaintext_length [in]
 *          The length of the plaintext string
 *      ciphertext [out]
 *              The encrypted string
 *
 *  Returns
 *      Returns the length of the ciphertext string or AESSTRINGCRYPT_ERROR
 *      if there was an error when trying to encrypt the string.
 */
unsigned long long AESStringCrypt(  unsigned char *password,
                                    unsigned long password_length,
                                    unsigned char *plaintext,
                                    unsigned long long plaintext_length,
                                    unsigned char *ciphertext);

/*
 *  AESStringDecrypt
 *
 *  Description
 *      This function is called to decrypt the string "ciphertext".
 *      The decrypted string is placed in "plaintext".
 *
 *  Parameters
 *      password [in]
 *          The password used to encrypt the string in UCS-16
 *          format.
 *      password_length [in]
 *          The length of the password in octets
 *      ciphertext [in]
 *          The ciphertext string to be decrypted
 *      ciphertext_length [in]
 *          The length of the ciphertext string
 *      plaintext [out]
 *          The decrypted string
 *
 *  Returns
 *      Returns the length of the plaintext string or AESSTRINGCRYPT_ERROR
 *      if there was an error when trying to encrypt the string.
 */
unsigned long long AESStringDecrypt(unsigned char *password,
                                    unsigned long password_length,
                                    unsigned char *ciphertext,
                                    unsigned long long ciphertext_length,
                                    unsigned char *plaintext);

#endif /* __STRINGCRYPT_H */
