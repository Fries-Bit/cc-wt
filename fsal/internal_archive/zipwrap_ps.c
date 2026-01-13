#include "platform_win.h"
#include "zipwrap_ps.h"
#include <stdio.h>

int zip_extract_with_powershell(const char* zipPath, const char* destDir, char* err, int errCap){
    return pw_expand_archive(zipPath, destDir, err, errCap);
}
