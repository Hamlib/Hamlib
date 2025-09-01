/*
 * AESStringCrypt.c
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

#include "hamlib/config.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#endif

#include "AESStringCrypt.h"

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
unsigned long long AESStringCrypt(unsigned char *password,
                                  unsigned long password_length,
                                  unsigned char *plaintext,
                                  unsigned long long plaintext_length,
                                  unsigned char *ciphertext)
{
    aes_context         aes_ctx;
    sha256_context      sha_ctx;
    sha256_t            digest;
    unsigned char       IV[16];
    int                 i, n;
    unsigned char       buffer[32];
    unsigned char       ipad[64], opad[64];
#ifdef _WIN32
    HCRYPTPROV          hProv;
    DWORD               result_code;
#else
    time_t              current_time;
    pid_t               process_id;
    FILE                *randfp = NULL;
#endif
    unsigned char       *p;

    /*
     * Write an AES signature at the head of the file, along
     * with the AES file format version number.
     */
    ciphertext[0] = 'A';
    ciphertext[1] = 'E';
    ciphertext[2] = 'S';
    ciphertext[3] = 0x00;   /* Version 0                    */
    ciphertext[4] = (plaintext_length & 0x0F);

    /*
     * We will use p as the pointer into the cipher
     */
    p = ciphertext + 5;

#ifdef _WIN32

    /*
     * Prepare for random number generation
     */
    if (!CryptAcquireContext(&hProv,
                             NULL,
                             NULL,
                             PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT))
    {
        result_code = GetLastError();

        if (GetLastError() == NTE_BAD_KEYSET)
        {
            if (!CryptAcquireContext(&hProv,
                                     NULL,
                                     NULL,
                                     PROV_RSA_FULL,
                                     CRYPT_NEWKEYSET | CRYPT_VERIFYCONTEXT))
            {
                result_code = GetLastError();
            }
            else
            {
                result_code = ERROR_SUCCESS;
            }
        }

        if (result_code != ERROR_SUCCESS)
        {
            return AESSTRINGCRYPT_ERROR;
        }
    }

    /*
     * Create the 16-bit IV used for encrypting the plaintext.
     * We do not fully trust the system's randomization functions,
     * so we improve on that by also hashing the random octets
     * and using only a portion of the hash.  This IV
     * generation could be replaced with any good random
     * source of data.
     */
    memset(IV, 0, 16);
    memset(buffer, 0, 32);

    sha256_starts(&sha_ctx);

    for (i = 0; i < 256; i++)
    {
        if (!CryptGenRandom(hProv,
                            32,
                            (BYTE *) buffer))
        {
            CryptReleaseContext(hProv, 0);
            return AESSTRINGCRYPT_ERROR;
        }

        sha256_update(&sha_ctx, buffer, 32);
    }

    sha256_finish(&sha_ctx, digest);

    /*
     * We're finished collecting random data
     */
    CryptReleaseContext(hProv, 0);

    /*
     * Get the IV from the digest buffer
     */
    memcpy(IV, digest, 16);

#else

    /*
     * Open the source for random data.  Note that while the entropy
     * might be lower with /dev/urandom than /dev/random, it will not
     * fail to produce something.  Also, we're going to hash the result
     * anyway.
     */
    if ((randfp = fopen("/dev/urandom", "r")) == NULL)
    {
        return AESSTRINGCRYPT_ERROR;
    }

    /*
     * We will use an initialization vector comprised of the current time
     * process ID, and random data, all hashed together with SHA-256.
     */
    current_time = time(NULL);

    for (i = 0; i < 8; i++)
    {
        buffer[i] = (unsigned char)
                    (current_time >> (i * 8));
    }

    process_id = getpid();

    for (i = 0; i < 8; i++)
    {
        buffer[i + 8] = (unsigned char)
                        (process_id >> (i * 8));
    }

    sha256_starts(&sha_ctx);
    sha256_update(&sha_ctx, buffer, 16);

    for (i = 0; i < 256; i++)
    {
        if (fread(buffer, 1, 32, randfp) != 32)
        {
            fclose(randfp);
            return AESSTRINGCRYPT_ERROR;
        }

        sha256_update(&sha_ctx,
                      buffer,
                      32);
    }

    sha256_finish(&sha_ctx, digest);

    /*
     * We're finished collecting random data
     */
    fclose(randfp);

    /*
     * Get the IV from the digest buffer
     */
    memcpy(IV, digest, 16);
#endif

    /*
     * Copy the IV to the ciphertext string
     */
    memcpy(p, IV, 16);
    p += 16;

    /*
     * Hash the IV and password 8192 times
     */
    memset(digest, 0, 32);
    memcpy(digest, IV, 16);

    for (i = 0; i < 8192; i++)
    {
        sha256_starts(&sha_ctx);
        sha256_update(&sha_ctx, digest, 32);
        sha256_update(&sha_ctx,
                      password,
                      password_length);
        sha256_finish(&sha_ctx,
                      digest);
    }

    /*
     * Set the AES encryption key
     */
    aes_set_key(&aes_ctx, digest, 256);

    /*
     * Set the ipad and opad arrays with values as
     * per RFC 2104 (HMAC).  HMAC is defined as
     *   H(K XOR opad, H(K XOR ipad, text))
     */
    memset(ipad, 0x36, 64);
    memset(opad, 0x5C, 64);

    for (i = 0; i < 32; i++)
    {
        ipad[i] ^= digest[i];
        opad[i] ^= digest[i];
    }

    sha256_starts(&sha_ctx);
    sha256_update(&sha_ctx, ipad, 64);

    while (plaintext_length > 0)
    {
        /*
         * Grab the next block of plaintext
         */
        if (plaintext_length >= 16)
        {
            n = 16;
        }
        else
        {
            n = (int) plaintext_length;
        }

        plaintext_length -= n;

        memcpy(buffer, plaintext, n);
        plaintext += n;

        /*
         * XOR plain text block with previous encrypted
         * output (i.e., use CBC)
         */
        for (i = 0; i < 16; i++)
        {
            buffer[i] ^= IV[i];
        }

        /*
         * Encrypt the contents of the buffer
         */
        aes_encrypt(&aes_ctx, buffer, buffer);

        /*
         * Concatenate the "text" as we compute the HMAC
         */
        sha256_update(&sha_ctx, buffer, 16);

        /*
         * Write the encrypted block
         */
        memcpy(p, buffer, 16);
        p += 16;

        /*
         * Update the IV (CBC mode)
         */
        memcpy(IV, buffer, 16);
    }

    /*
     * Write the HMAC
     */
    sha256_finish(&sha_ctx, digest);
    sha256_starts(&sha_ctx);
    sha256_update(&sha_ctx, opad, 64);
    sha256_update(&sha_ctx, digest, 32);
    sha256_finish(&sha_ctx, digest);
    memcpy(p, digest, 32);
    p += 32;

    return (p - ciphertext);
}

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
                                    unsigned char *plaintext)
{
    aes_context                 aes_ctx;
    sha256_context              sha_ctx;
    sha256_t                    digest;
    unsigned char               IV[16];
    int                         i, n;
    unsigned char               buffer[64], buffer2[32];
    unsigned char               ipad[64], opad[64];
    unsigned char               *p;
    int                         final_block_size;

    /*
     * Encrypted strings will be at least 53 octets in length
     * and the rest must be a multiple of 16 octets
     */
    if (ciphertext_length < 53)
    {
        return AESSTRINGCRYPT_ERROR;
    }

    if (!(ciphertext[0] == 'A' && ciphertext[1] == 'E' &&
            ciphertext[2] == 'S'))
    {
        return AESSTRINGCRYPT_ERROR;
    }

    /*
     * Validate the version number and take any version-specific actions
     */
    if (ciphertext[3] > 0)
    {
        return AESSTRINGCRYPT_ERROR;
    }

    /*
     * Take note of the final block size
     */
    final_block_size = ciphertext[4];

    /*
     * Move pointers and count beyond header
     */
    ciphertext += 5;
    ciphertext_length -= 5;

    /*
     * We will use p to write into the plaintext buffer
     */
    p = plaintext;

    /*
     * Read the initialization vector
     */
    memcpy(IV, ciphertext, 16);
    ciphertext += 16;
    ciphertext_length -= 16;

    /*
     * Hash the IV and password 8192 times
     */
    memset(digest, 0, 32);
    memcpy(digest, IV, 16);

    for (i = 0; i < 8192; i++)
    {
        sha256_starts(&sha_ctx);
        sha256_update(&sha_ctx, digest, 32);
        sha256_update(&sha_ctx,
                      password,
                      password_length);
        sha256_finish(&sha_ctx,
                      digest);
    }

    /*
     * Set the AES encryption key
     */
    aes_set_key(&aes_ctx, digest, 256);

    /*
     * Set the ipad and opad arrays with values as
     * per RFC 2104 (HMAC).  HMAC is defined as
     *   H(K XOR opad, H(K XOR ipad, text))
     */
    memset(ipad, 0x36, 64);
    memset(opad, 0x5C, 64);

    for (i = 0; i < 32; i++)
    {
        ipad[i] ^= digest[i];
        opad[i] ^= digest[i];
    }

    sha256_starts(&sha_ctx);
    sha256_update(&sha_ctx, ipad, 64);

    while (ciphertext_length > 32)
    {
        memcpy(buffer, ciphertext, 16);
        memcpy(buffer2, ciphertext, 16);
        ciphertext += 16;
        ciphertext_length -= 16;

        sha256_update(&sha_ctx, buffer, 16);
        aes_decrypt(&aes_ctx, buffer, buffer);

        /*
         * XOR plain text block with previous encrypted output (i.e., use CBC)
         */
        for (i = 0; i < 16; i++)
        {
            buffer[i] ^= IV[i];
        }

        /*
         * Update the IV (CBC mode)
         */
        memcpy(IV, buffer2, 16);

        /*
         * If this is the final block, then we may
         * write less than 16 octets
         */
        if ((ciphertext_length > 32) || (!final_block_size))
        {
            n = 16;
        }
        else
        {
            n = final_block_size;
        }

        /*
         * Write the decrypted block
         */
        memcpy(p, buffer, n);
        p += n;
    }

    /*
     * Verify that the HMAC is correct
     */
    if (ciphertext_length != 32)
    {
        return AESSTRINGCRYPT_ERROR;
    }

    sha256_finish(&sha_ctx, digest);
    sha256_starts(&sha_ctx);
    sha256_update(&sha_ctx, opad, 64);
    sha256_update(&sha_ctx, digest, 32);
    sha256_finish(&sha_ctx, digest);

    if (memcmp(digest, ciphertext, 32))
    {
        return AESSTRINGCRYPT_ERROR;
    }

    return (p - plaintext);
}

