#include "platform_win.h"
#include "fsnet.h"
#include <string.h>
#include <stdio.h>

// Convert e.g. https://github.com/user/repo or https://github.com/user/repo.git
// into https://codeload.github.com/user/repo/zip/refs/heads/main first, and try master if main fails.
static void make_codeload_url(const char* repoUrl, const char* branch, char* out, size_t cap){
    // assume repoUrl starts with https://github.com/
    const char* p = strstr(repoUrl, "github.com/");
    if(!p){ snprintf(out,cap,"%s",repoUrl); return; }
    p += strlen("github.com/");
    char user[256]={0}, repo[256]={0};
    sscanf(p, "%255[^/]/%255[^/?#]", user, repo);
    size_t L=strlen(repo);
    if(L>4 && strcmp(repo+L-4, ".git")==0) repo[L-4]='\0';
    snprintf(out,cap,"https://codeload.github.com/%s/%s/zip/refs/heads/%s", user, repo, branch);
}

int fs_download_github_zip(const char* repoUrl, const char* destZip, pw_download_progress_cb cb, void* userdata){
    char url[1024];
    make_codeload_url(repoUrl, "main", url, sizeof(url));
    if(pw_download_file_http(url, destZip, cb, userdata)==0) return 0;
    make_codeload_url(repoUrl, "master", url, sizeof(url));
    if(pw_download_file_http(url, destZip, cb, userdata)==0) return 0;
    return -1;
}
