/*
 *  String Crypt Test (Linux)
 *  Copyright (C) 2012, 2015
 *
 *  Author: Paul E. Jones <paulej@packetizer.com>
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
#include <string.h>
#ifdef _WIN32
#include <windows.h>
//#include <Wincrypt.h>
#else
#include <unistd.h>
#endif

#include "AESStringCrypt.h"
#include "password.h"

/*
 * Define how many test vectors to consider
 */
#define TEST_COUNT 21

/*
 * Dummy string
 */
#define DUMMY_STRING \
    "VOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOID" \
    "VOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOID" \
    "VOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOID" \
    "VOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOIDVOID"

/*
 * Main will just perform some simple test
 */
int main(int argc, char *argv[])
{
#ifdef __linux__
    char pass_input[MAX_PASSWD_LEN + 1];
#endif
    char plaintext[512],
         ciphertext[512 + 68];
#ifdef _WIN32
    wchar_t pass[MAX_PASSWD_LEN + 1];
#else
    char pass[MAX_PASSWD_LEN * 2 + 2];
#endif
    int  i,
         passlen;
    unsigned long long plaintext_length,
             ciphertext_length;
    char *plaintext_tests[TEST_COUNT] =
    {
        "",
        "0",
        "012",
        "0123456789ABCDE",
        "0123456789ABCDEF",
        "0123456789ABCDEF0",
        "0123456789ABCDEF0123456789ABCDE",
        "0123456789ABCDEF0123456789ABCDEF",
        "0123456789ABCDEF0123456789ABCDEF0",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDE",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDE",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDE",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDE",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0"
    };

    /*
     * Run through tests
     */
    for (i = 0; i < TEST_COUNT; i++)
    {
        printf("\nStarting test %i\n", i + 1);

        /*
         * We will use the password "Hello"
         */
#ifdef _WIN32
        wcscpy(pass, L"Hello");
        passlen = (int) wcslen(pass) * 2;
#else
        strcpy(pass_input, "Hello");
        passlen = passwd_to_utf16(pass_input,
                                  strlen(pass_input),
                                  MAX_PASSWD_LEN + 1,
                                  pass);
#endif

        if (passlen <= 0)
        {
            printf("Error converting the password to UTF-16LE\n");
            return -1;
        }

        /*
         * Put the text vector into "plaintext"
         */
        strcpy(plaintext, pass_input);
        printf("Plaintext: %s\n", plaintext);
        printf("Plaintext length: %lu\n", strlen(plaintext));

        /*
         * Encrypt the string
         */
        printf("Encrypting...\n");
        ciphertext_length = AESStringCrypt((unsigned char *) pass,
                                           passlen,
                                           (unsigned char *) plaintext,
                                           strlen(plaintext),
                                           (unsigned char *) ciphertext);

        if (ciphertext_length == AESSTRINGCRYPT_ERROR)
        {
            printf("Error encrypting the string\n");
        }

        printf("Ciphertext length: %llu\n", ciphertext_length);

#if 0
        /*
         * One could verify that the data encrypted properly using
         * any version of AES Crypt.
         */
        {
            char file[64];
            FILE *fp;
            sprintf(file, "test-%i.txt.aes", i);
            fp = fopen(file, "wb");
            fwrite(ciphertext, ciphertext_length, 1, fp);
            fclose(fp);
        }
#endif

        /*
         * Decrypt the ciphertext
         */
        strcpy(plaintext, DUMMY_STRING);
        printf("Decrypting...\n");
        plaintext_length = AESStringDecrypt((unsigned char *) pass,
                                            passlen,
                                            (unsigned char *) ciphertext,
                                            ciphertext_length,
                                            (unsigned char *) plaintext);

        if (plaintext_length == AESSTRINGCRYPT_ERROR)
        {
            printf("Error decrypting the string\n");
        }

        printf("Decrypted plaintext length: %llu, %s\n", plaintext_length, plaintext);

        if (plaintext_length != strlen(plaintext_tests[i]))
        {
            printf("Decrypted length does not match original input length!\n");
            return -1;
        }

        /*
         * Let's insert a string terminator
         */
        plaintext[plaintext_length] = '\0';

        if (plaintext_length && strcmp(plaintext_tests[i], plaintext))
        {
            printf("Decrypted string does not match!\n");
            return -1;
        }

        printf("Plaintext matches input: %s\n", plaintext);
    }

    printf("\nAll tests passed successfully\n\n");

    return 0;
}
