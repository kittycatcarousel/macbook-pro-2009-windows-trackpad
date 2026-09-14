/* SPDX-License-Identifier: MIT */
#include <wincrypt.h>

static BOOL hash_sha256(const BYTE *data,DWORD size,BYTE out[32])
{
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;DWORD length=32,error;BOOL ok;
    /* Use the default provider name for XP SP3 and later Windows versions. */
    if(!CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))return FALSE;
    ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)&&
        CryptHashData(hash,data,size,0)&&CryptGetHashParam(hash,HP_HASHVAL,out,&length,0);
    error=GetLastError();
    if(hash)CryptDestroyHash(hash);
    CryptReleaseContext(provider,0);
    if(!ok)SetLastError(error);
    return ok;
}
