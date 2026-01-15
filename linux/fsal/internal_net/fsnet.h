#ifndef FSNET_H
#define FSNET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"

int fs_download_github_zip(const char* repoUrl, const char* destZip, pw_download_progress_cb cb, void* userdata);

#ifdef __cplusplus
}
#endif

#endif