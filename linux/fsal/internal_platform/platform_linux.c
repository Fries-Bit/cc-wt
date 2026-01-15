#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

static void safe_strcpy(char* dst, size_t cap, const char* src){
    if(!dst||!cap) return; 
    dst[0]='\0'; 
    if(!src) return; 
    strncpy(dst, src, cap-1); 
    dst[cap-1]='\0';
}

int pw_shell(const char* cmd, char* outBuf, size_t outCap){
    if(outBuf && outCap) outBuf[0]='\0';
    FILE* fp = popen(cmd, "r");
    if(!fp) return -1;
    char buf[1024];
    size_t total = 0;
    while(fgets(buf, sizeof(buf), fp)){
        size_t len = strlen(buf);
        if(outBuf && outCap){
            size_t remain = (total < outCap ? outCap - total - 1 : 0);
            if(remain > 0){
                strncat(outBuf, buf, remain);
            }
        }
        total += len;
    }
    int status = pclose(fp);
    return WEXITSTATUS(status);
}

int pw_expand_archive(const char* zipPath, const char* destDir, char* err, size_t errCap){
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "unzip -o \"%s\" -d \"%s\"", zipPath, destDir);
    int rc = system(cmd);
    if(rc != 0 && err){ safe_strcpy(err, errCap, "unzip failed"); }
    return rc == 0 ? 0 : -1;
}

int pw_download_file_http(const char* url, const char* destPath, pw_download_progress_cb cb, void* userdata) {
    char cmd[4096];
    // Using curl for simplicity
    snprintf(cmd, sizeof(cmd), "curl -L -o \"%s\" \"%s\"", destPath, url);
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}

static int mkdir_p(const char* path){
    char tmp[1024]; safe_strcpy(tmp, sizeof(tmp), path);
    size_t len = strlen(tmp); if(len == 0) return 0;
    if(tmp[len-1] == '/') tmp[len-1] = '\0';
    for(char* p = tmp + 1; *p; ++p){
        if(*p == '/'){
            *p = '\0';
            mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            *p = '/';
        }
    }
    if(mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0){
        if(errno != EEXIST) return -1;
    }
    return 0;
}

int pw_ensure_dir(const char* path){ return mkdir_p(path); }

int pw_file_exists(const char* path){ return access(path, F_OK) == 0; }

int pw_write_text(const char* path, const char* text){
    char dir[1024];
    safe_strcpy(dir, sizeof(dir), path);
    char* slash = strrchr(dir, '/');
    if(slash){ *slash = '\0'; mkdir_p(dir); }
    FILE* f = fopen(path, "w");
    if(!f) return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

int pw_read_text(const char* path, char** outText, size_t* outSize){
    FILE* f = fopen(path, "r");
    if(!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = (char*)malloc(sz + 1);
    if(!data){ fclose(f); return -2; }
    size_t rd = fread(data, 1, sz, f);
    fclose(f);
    data[rd] = '\0';
    if(outText) *outText = data;
    if(outSize) *outSize = rd;
    else free(data);
    return 0;
}

int pw_join(const char* a, const char* b, char* out, size_t cap){
    if(!a || !b || !out || !cap) return -1;
    size_t la = strlen(a);
    int needs = (la > 0 && a[la-1] == '/') ? 0 : 1;
    snprintf(out, cap, "%s%s%s", a, needs ? "/" : "", b);
    return 0;
}

int pw_get_appdata(char* out, size_t cap){
    const char* home = getenv("HOME");
    if(!home) return -1;
    snprintf(out, cap, "%s/.local/share", home);
    return 0;
}

int pw_add_to_user_path(const char* dir, char* err, size_t errCap){
    const char* home = getenv("HOME");
    if(!home) return -1;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.bashrc", home);
    FILE* f = fopen(path, "a");
    if(!f) return -1;
    fprintf(f, "\nexport PATH=\"$PATH:%s\"\n", dir);
    fclose(f);
    return 0;
}
