/**************************************************************************
*            Unix-like crypt(3) Algorithm for Password Encryption
*
*   File    : crypt3.h
*   Purpose : Provides crypt(3) prototypes to ANSI C compilers
*             without a need for the crypt library.
*   Author  : Fan, Chun-wei
*   Date    : June 24, 2013
*
*   I am releasing the source that I have provided into public
*   domain without any restrictions, warranties, or copyright
*   claims of my own.
*
***************************************************************************/

#ifndef __ANSI_CRYPT_H__
#define __ANSI_CRYPT_H__

#ifdef __cplusplus
extern "C"
{
#endif

void encrypt (char *block, int edflag);
void setkey (char *key);
char* crypt (const char *key, const char *salt);

#ifdef __cplusplus
}
#endif

#endif /* __ANSI_CRYPT_H__ */
