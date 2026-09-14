/* SPDX-License-Identifier: MIT
 * Read and change the live driver settings on XP.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>
#include <limits.h>
#include "resource.h"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define GET_TIMING 0xf2010
#define SET_TIMING 0xf2014
static const WCHAR profile_key[]=L"Software\\XPTrackpadSettings";
typedef struct { unsigned tap,gap,motion; } Settings;
typedef struct { DWORD version; Settings settings; } Config;
static WCHAR message[1024];
static BOOL errorf(const WCHAR *format,...)
{
    va_list ap; va_start(ap,format); _vsnwprintf_s(message,COUNT(message),_TRUNCATE,format,ap); va_end(ap); return FALSE;
}
static BOOL winerror(const WCHAR *operation,DWORD code)
{
    WCHAR detail[512]=L"";
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,code,0,detail,COUNT(detail),NULL);
    return errorf(L"%s Windows error %lu: %s",operation,code,detail);
}
static BOOL number(const WCHAR *text,unsigned *out)
{
    unsigned n=0; const WCHAR *p=text; if(!*p) return FALSE;
    for(;*p;p++) { unsigned digit; if(*p<L'0'||*p>L'9') return FALSE; digit=*p-L'0';
        if(n>(UINT_MAX-digit)/10) return FALSE; n=n*10+digit; }
    *out=n; return TRUE;
}
static BOOL same(Settings a,Settings b) { return a.tap==b.tap&&a.gap==b.gap&&a.motion==b.motion; }
static HANDLE open_driver(void)
{
    HANDLE d=CreateFileW(L"\\\\.\\AppleTrackpad",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if(d==INVALID_HANDLE_VALUE) winerror(L"The program cannot open the trackpad.",GetLastError()); return d;
}
static BOOL read_live(HANDLE device,Settings *s)
{
    DWORD bytes=0; Config c={0};
    if(!DeviceIoControl(device,GET_TIMING,NULL,0,&c,sizeof(c),&bytes,NULL))
        return winerror(L"The program cannot read the runtime settings.",GetLastError());
    if(bytes!=sizeof(c)||c.version!=2) return errorf(L"Load the driver with runtime settings. Then restart Windows.");
    *s=c.settings; return TRUE;
}
static BOOL write_live(HANDLE device,Settings s)
{
    DWORD bytes=0; Config c; Settings readback;
    c.version=2; c.settings=s;
    if(!DeviceIoControl(device,SET_TIMING,&c,sizeof(c),NULL,0,&bytes,NULL))
        return winerror(L"The program cannot apply the runtime settings.",GetLastError());
    if(!read_live(device,&readback)) return FALSE;
    return same(readback,s) || errorf(L"The driver returned different values.");
}
static BOOL saved(Settings *s,BOOL *exists)
{
    HKEY k; DWORD type=0,size=sizeof(Config); Config c={0}; LONG r; *exists=FALSE;
    r=RegOpenKeyExW(HKEY_CURRENT_USER,profile_key,0,KEY_QUERY_VALUE,&k);
    if(r==ERROR_FILE_NOT_FOUND) return TRUE;
    if(r!=ERROR_SUCCESS) return winerror(L"The program cannot open the saved settings.",r);
    r=RegQueryValueExW(k,L"TimingV2",NULL,&type,(BYTE*)&c,&size);
    RegCloseKey(k);
    if(r==ERROR_FILE_NOT_FOUND) return TRUE;
    if(r!=ERROR_SUCCESS) return winerror(L"The program cannot read the saved settings.",r);
    if(type!=REG_BINARY||size!=sizeof(c)||c.version!=2) return errorf(L"The saved settings have an incorrect format.");
    *s=c.settings; *exists=TRUE; return TRUE;
}
static BOOL save(Settings s)
{
    HKEY k; Config c; LONG r;
    c.version=2; c.settings=s;
    r=RegCreateKeyExW(HKEY_CURRENT_USER,profile_key,0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL);
    if(r!=ERROR_SUCCESS) return winerror(L"The values are active. Opening the settings registry key failed.",r);
    r=RegSetValueExW(k,L"TimingV2",0,REG_BINARY,(BYTE*)&c,sizeof(c));
    RegCloseKey(k);
    return r==ERROR_SUCCESS || winerror(L"The values are active. Saving them failed.",r);
}
static BOOL apply(Settings s,BOOL persist)
{
    HANDLE d=open_driver(); BOOL ok;
    if(d==INVALID_HANDLE_VALUE) return FALSE;
    ok=write_live(d,s); CloseHandle(d);
    if(ok&&persist) ok=save(s);
    if(ok) wcscpy_s(message,COUNT(message),L"The values are active and saved.");
    return ok;
}
static BOOL query(Settings *settings)
{
    HANDLE d=open_driver(); BOOL ok; if(d==INVALID_HANDLE_VALUE) return FALSE;
    ok=read_live(d,settings); CloseHandle(d); return ok;
}
static void fill(HWND dialog,Settings s)
{
    SetDlgItemInt(dialog,IDC_TAP,s.tap,FALSE); SetDlgItemInt(dialog,IDC_GAP,s.gap,FALSE); SetDlgItemInt(dialog,IDC_MOTION,s.motion,FALSE);
}
static BOOL field(HWND dialog,int id,unsigned *value,const WCHAR *label)
{
    HWND control=GetDlgItem(dialog,id); int length=GetWindowTextLengthW(control); WCHAR *v; BOOL ok;
    if(length<0||length>INT_MAX/(int)sizeof(WCHAR)-1) return errorf(L"The program cannot read this quantity of input text.");
    v=(WCHAR*)LocalAlloc(LMEM_FIXED,((SIZE_T)length+1)*sizeof(WCHAR));
    if(!v) return winerror(L"The program cannot read the setting.",ERROR_NOT_ENOUGH_MEMORY);
    ok=GetWindowTextW(control,v,length+1)==length&&number(v,value); LocalFree(v);
    return ok||errorf(L"%s: Enter an integer from 0 to 4294967295. This is the full 32-bit range.",label);
}
static BOOL from_ui(HWND dialog,Settings *s)
{
    return field(dialog,IDC_TAP,&s->tap,L"Maximum tap time")&&field(dialog,IDC_GAP,&s->gap,L"Time before the second touch")&&field(dialog,IDC_MOTION,&s->motion,L"Minimum finger movement for dragging");
}
static INT_PTR CALLBACK dialog_proc(HWND dialog,UINT msg,WPARAM wp,LPARAM lp)
{
    Settings s={250,300,8}; BOOL ok; (void)lp;
    switch(msg) {
    case WM_INITDIALOG:
        ok=query(&s); fill(dialog,s);
        if(ok) wcscpy_s(message,COUNT(message),L"These are the live settings. Apply changes the values immediately and saves them.");
        EnableWindow(GetDlgItem(dialog,IDC_APPLY),ok); SetDlgItemTextW(dialog,IDC_STATUS,message);
        return TRUE;
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case IDCANCEL: EndDialog(dialog,0); return TRUE;
        case IDC_APPLY:
            ok=from_ui(dialog,&s)&&apply(s,TRUE); SetDlgItemTextW(dialog,IDC_STATUS,message);
            if(!ok) MessageBoxW(dialog,message,L"Trackpad Settings",MB_OK|MB_ICONERROR); return TRUE;
        case IDC_RESTORE: fill(dialog,s); SetDlgItemTextW(dialog,IDC_STATUS,L"The fields show the default values. Click Apply to use these values."); return TRUE;
        } break;
    case WM_CLOSE: EndDialog(dialog,0); return TRUE;
    } return FALSE;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR command,int show)
{
    int argc,rc=1; WCHAR **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    Settings s; BOOL exists=FALSE;
    (void)previous; (void)command; (void)show; if(!argv) return 2;
    if(argc==1) {
        rc=(int)DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_SETTINGS),NULL,dialog_proc,0);
        if(rc<0) rc=1;
    } else if(argc==2&&!wcscmp(argv[1],L"--apply-saved")) {
        if(saved(&s,&exists)) rc=(!exists||apply(s,FALSE))?0:1;
    } else MessageBoxW(NULL,L"Command: trackpad-settings.exe [--apply-saved]",L"Trackpad Settings",MB_OK|MB_ICONERROR);
    LocalFree(argv); return rc;
}
