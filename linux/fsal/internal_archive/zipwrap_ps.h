#ifndef ZIPWRAP_PS_H
#define ZIPWRAP_PS_H

#ifdef __cplusplus
extern "C" {
#endif

int zip_extract_with_powershell(const char* zipPath, const char* destDir, char* err, int errCap);

#ifdef __cplusplus
}
#endif

#endif