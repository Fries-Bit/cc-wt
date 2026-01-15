#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pw_download_progress_cb)(size_t current, size_t total, void* userdata);

int pw_shell(const char* cmd, char* outBuf, size_t outCap);
int pw_expand_archive(const char* zipPath, const char* destDir, char* err, size_t errCap);
int pw_download_file_http(const char* url, const char* destPath, pw_download_progress_cb cb, void* userdata);
int pw_ensure_dir(const char* path);
int pw_file_exists(const char* path);
int pw_write_text(const char* path, const char* text);
int pw_read_text(const char* path, char** outText, size_t* outSize);
int pw_join(const char* a, const char* b, char* out, size_t cap);
int pw_get_appdata(char* out, size_t cap);
int pw_add_to_user_path(const char* dir, char* err, size_t errCap);

#ifdef __cplusplus
}
#endif

#endif