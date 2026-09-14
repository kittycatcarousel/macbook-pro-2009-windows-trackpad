/* SPDX-License-Identifier: MIT
 * Apply patch records to the specified original Apple driver.
 * Write the patched driver to a new output file.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "patch-data.h"

#include "../sha256.h"

static int failure(const char *message)
{
    fprintf(stderr,"%s\n",message);return 1;
}
int wmain(int argc,WCHAR **argv)
{
    static BYTE data[TARGET_SIZE];
    BYTE digest[32];FILE *f=NULL;BOOL ok;
    if(argc!=3) {
        fputs("Command: driver-patch ORIGINAL.sys OUTPUT.sys\nUse a new output file name.\n",stderr);
        return 2;
    }
    if(_wfopen_s(&f,argv[1],L"rb"))return failure("The program cannot open the original driver.");
    ok=fread(data,1,SOURCE_SIZE,f)==SOURCE_SIZE&&fgetc(f)==EOF&&!ferror(f);
    fclose(f);
    if(!ok)return failure("The program cannot read the original driver or its size is incorrect.");
    if(!hash_sha256(data,SOURCE_SIZE,digest))return failure("The program cannot calculate the driver hash.");
    if(memcmp(digest,source_hash,32))return failure("Use AppleMTP.sys 3.1.0.10 from Boot Camp 3.2 x86.");
    patch(data);
    if(_wfopen_s(&f,argv[2],L"wbx"))return failure("The program cannot make the output file. Use a new file name.");
    ok=fwrite(data,1,TARGET_SIZE,f)==TARGET_SIZE;
    if(fclose(f))ok=FALSE;
    if(!ok) { _wremove(argv[2]);return failure("The program cannot write the full output file."); }
    puts("The updated driver is ready.");return 0;
}
