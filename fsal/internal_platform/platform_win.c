#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_win.h"

static void safe_strcpy(char* dst, size_t cap, const char* src){
    if(!dst||!cap) return; 
    dst[0]='\0'; 
    if(!src) return; 
    strncpy(dst, src, cap-1); 
    dst[cap-1]='\0';
}

int pw_shell(const char* cmd, char* outBuf, size_t outCap){
    if(outBuf && outCap) outBuf[0]='\0';
    char fullCmd[4096];
    snprintf(fullCmd, sizeof(fullCmd), "cmd /c %s", cmd);
    SECURITY_ATTRIBUTES sa={0}; sa.nLength=sizeof(sa); sa.bInheritHandle=TRUE;
    HANDLE r=0,w=0; if(!CreatePipe(&r,&w,&sa,0)) return -1;
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si={0}; si.cb=sizeof(si);
    si.dwFlags=STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput=w; si.hStdError=w; si.wShowWindow=SW_HIDE;

    PROCESS_INFORMATION pi={0};
    if(!CreateProcessA(NULL,(LPSTR)fullCmd,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        CloseHandle(r); CloseHandle(w); return -2;
    }
    CloseHandle(w);
    DWORD read=0; size_t total=0; BOOL ok=FALSE; char buf[1024];
    while((ok=ReadFile(r,buf,sizeof(buf)-1,&read,NULL)) && read){
        buf[read]=0;
        if(outBuf && outCap){
            size_t remain = (total<outCap? outCap-total-1:0);
            if(remain){ strncat(outBuf, buf, remain); }
        }
        total += read;
    }
    CloseHandle(r);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec=0; GetExitCodeProcess(pi.hProcess,&ec);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return (int)ec;
}

int pw_expand_archive(const char* zipPath, const char* destDir, char* err, size_t errCap){
    char cmd[4096];
    snprintf(cmd,sizeof(cmd),
        "powershell -NoProfile -Command \"$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath '%s' -DestinationPath '%s' -Force\"",
        zipPath,destDir);
    int rc = pw_shell(cmd, NULL, 0);
    if(rc!=0 && err){ safe_strcpy(err, errCap, "PowerShell Expand-Archive failed"); }
    return rc==0?0:-1;
}

#include <winhttp.h>

int pw_download_file_http(const char* url, const char* destPath, pw_download_progress_cb cb, void* userdata) {
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    URL_COMPONENTS urlComp = { sizeof(URL_COMPONENTS) };
    wchar_t host[256], path[1024];
    urlComp.lpszHostName = host; urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = path; urlComp.dwUrlPathLength = 1024;

    wchar_t wUrl[2048]; MultiByteToWideChar(CP_UTF8, 0, url, -1, wUrl, 2048);
    if (!WinHttpCrackUrl(wUrl, 0, 0, &urlComp)) return -1;

    hSession = WinHttpOpen(L"FSAL/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return -2;

    hConnect = WinHttpConnect(hSession, host, urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return -3; }

    hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -4; }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -5;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -6;
    }

    DWORD dwContentLength = 0, dwSize = sizeof(dwContentLength);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwContentLength, &dwSize, WINHTTP_NO_HEADER_INDEX);

    HANDLE hFile = CreateFileA(destPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -7;
    }

    DWORD dwDownloaded = 0, dwTotalDownloaded = 0;
    BYTE buffer[8192];
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        if (dwSize > sizeof(buffer)) dwSize = sizeof(buffer);
        if (!WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded)) break;
        DWORD dwWritten;
        WriteFile(hFile, buffer, dwDownloaded, &dwWritten, NULL);
        dwTotalDownloaded += dwDownloaded;
        if (cb) cb(dwTotalDownloaded, dwContentLength, userdata);
    } while (dwSize > 0);

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return 0;
}

static int mkdir_p(const char* path){
    char tmp[MAX_PATH]; safe_strcpy(tmp, sizeof(tmp), path);
    size_t len=strlen(tmp); if(len==0) return 0;
    if(tmp[len-1]=='\\' || tmp[len-1]=='/') tmp[len-1]='\0';
    for(char* p=tmp+1; *p; ++p){
        if(*p=='\\' || *p=='/'){
            char c=*p; *p='\0'; CreateDirectoryA(tmp,NULL); *p=c;
        }
    }
    if(!CreateDirectoryA(tmp,NULL)){
        DWORD e=GetLastError(); if(e!=ERROR_ALREADY_EXISTS) return -1;
    }
    return 0;
}

int pw_ensure_dir(const char* path){ return mkdir_p(path); }

int pw_file_exists(const char* path){ DWORD a=GetFileAttributesA(path); return (a!=INVALID_FILE_ATTRIBUTES); }

int pw_write_text(const char* path, const char* text){
    char dir[MAX_PATH];
    safe_strcpy(dir,sizeof(dir),path);
    char* slash = strrchr(dir,'\\'); if(!slash) slash=strrchr(dir,'/');
    if(slash){ *slash='\0'; mkdir_p(dir); }
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE) return -1;
    DWORD wr=0; BOOL ok=WriteFile(h, text, (DWORD)strlen(text), &wr, NULL);
    CloseHandle(h);
    return ok?0:-1;
}

int pw_read_text(const char* path, char** outText, size_t* outSize){
    HANDLE h=CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE) return -1;
    DWORD sz=GetFileSize(h,NULL);
    char* data=(char*)malloc(sz+1); if(!data){ CloseHandle(h); return -2; }
    DWORD rd=0; BOOL ok=ReadFile(h,data,sz,&rd,NULL); CloseHandle(h);
    if(!ok){ free(data); return -3; }
    data[rd]='\0'; if(outText) *outText=data; if(outSize) *outSize=rd; else free(data);
    return 0;
}

int pw_join(const char* a, const char* b, char* out, size_t cap){
    if(!a||!b||!out||!cap) return -1;
    size_t la=strlen(a);
    int needs = (la>0 && (a[la-1]=='\\' || a[la-1]=='/')) ? 0 : 1;
    snprintf(out,cap,"%s%s%s", a, needs?"\\":"", b);
    return 0;
}

int pw_get_appdata(char* out, size_t cap){
    char path[MAX_PATH];
    if(SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))){
        snprintf(out,cap,"%s", path);
        return 0;
    }
    return -1;
}

static int path_in_user_env(const char* dir){
    HKEY hKey; if(RegOpenKeyExA(HKEY_CURRENT_USER, "Environment",0,KEY_READ,&hKey)!=ERROR_SUCCESS) return 0;
    char buf[8192]; DWORD type=REG_EXPAND_SZ, size=sizeof(buf);
    LONG r = RegQueryValueExA(hKey, "Path", NULL, &type, (LPBYTE)buf, &size);
    RegCloseKey(hKey);
    if(r!=ERROR_SUCCESS) return 0;
    return (strstr(buf, dir)!=NULL);
}

int pw_add_to_user_path(const char* dir, char* err, size_t errCap){
    if(path_in_user_env(dir)) return 0;
    HKEY hKey; if(RegOpenKeyExA(HKEY_CURRENT_USER, "Environment",0,KEY_SET_VALUE,&hKey)!=ERROR_SUCCESS){
        if(err) safe_strcpy(err, errCap, "Open reg key failed");
        return -1;
    }
    char buf[8192]; DWORD type=REG_EXPAND_SZ, size=sizeof(buf);
    LONG r = RegGetValueA(HKEY_CURRENT_USER, "Environment", "Path", RRF_RT_ANY, &type, buf, &size);
    if(r!=ERROR_SUCCESS){ buf[0]='\0'; }
    if(buf[0]){ strncat(buf, ";", sizeof(buf)-strlen(buf)-1); }
    strncat(buf, dir, sizeof(buf)-strlen(buf)-1);
    r = RegSetValueExA(hKey, "Path", 0, REG_EXPAND_SZ, (const BYTE*)buf, (DWORD)(strlen(buf)+1));
    RegCloseKey(hKey);
    if(r!=ERROR_SUCCESS){ if(err) safe_strcpy(err, errCap, "Set PATH failed"); return -2; }
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
    return 0;
}
