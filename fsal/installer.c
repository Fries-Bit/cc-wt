#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include "platform_win.h"
#include "ui.h"
#include "config.h"

static const char* EMBEDDED_RECOMMENDED_FSAL = 
    "[listdep]\n"
    "1dep = [\n"
    "   name = \"test1\"\n"
    "   description = \"This is a test package for FSAL\":string\n"
    "   version = 1:integer\n"
    "   git = \"https://github.com/Fries-Bit/package-fsal-test\"\n"
    "]\n";

static int extract_embedded_fsal(const char* selfPath, const char* outputPath) {
    FILE* self = fopen(selfPath, "rb");
    if (!self) return -1;
    
    fseek(self, -12, SEEK_END);
    uint32_t magic = 0, fsal_size = 0;
    fread(&fsal_size, 4, 1, self);
    fread(&magic, 4, 1, self);
    
    if (magic != 0x4C534146) {
        fclose(self);
        return -2;
    }
    
    fseek(self, -12 - (long)fsal_size, SEEK_END);
    FILE* out = fopen(outputPath, "wb");
    if (!out) {
        fclose(self);
        return -3;
    }
    
    char buffer[8192];
    uint32_t remaining = fsal_size;
    while (remaining > 0) {
        uint32_t to_read = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        size_t n = fread(buffer, 1, to_read, self);
        if (n <= 0) break;
        fwrite(buffer, 1, n, out);
        remaining -= n;
    }
    
    fclose(out);
    fclose(self);
    return remaining == 0 ? 0 : -4;
}

int main() {
    ui_print_step("Installing", "FSAL Runtime");

    const char* up = getenv("USERPROFILE");
    char defDir[512];
    if (up) snprintf(defDir, sizeof(defDir), "%s", up);
    else {
        char app[512]; 
        if (pw_get_appdata(app, sizeof(app)) != 0) { strcpy(app, "."); }
        snprintf(defDir, sizeof(defDir), "%s\\FSAL", app);
    }

    char dir[512];
    printf("Install directory [%s]: ", defDir);
    fflush(stdout);
    if (fgets(dir, sizeof(dir), stdin) == NULL || dir[0] == '\n' || dir[0] == '\r') {
        snprintf(dir, sizeof(dir), "%s", defDir);
    }
    size_t L = strlen(dir);
    if (L && (dir[L - 1] == '\n' || dir[L - 1] == '\r')) dir[L - 1] = '\0';

    if (pw_ensure_dir(dir) != 0) {
        ui_print_error("Cannot create installation directory");
        return 1;
    }

    char exe[512]; GetModuleFileNameA(NULL, exe, sizeof(exe));
    char curDir[512]; snprintf(curDir, sizeof(curDir), "%s", exe);
    char* slash = strrchr(curDir, '\\'); if (slash) *slash = '\0';

    char srcFsal[512]; pw_join(curDir, "fsal.exe", srcFsal, sizeof(srcFsal));
    char dstFsal[512]; pw_join(dir, "fsal.exe", dstFsal, sizeof(dstFsal));
    
    ui_print_step("Setting up", "fsal.exe");
    
    if (!pw_file_exists(srcFsal)) {
        ui_print_step("Extracting", "embedded fsal.exe");
        if (extract_embedded_fsal(exe, dstFsal) != 0) {
            ui_print_error("Failed to extract embedded fsal.exe");
            return 2;
        }
    } else {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "powershell -NoProfile -Command \"Copy-Item -Force -LiteralPath '%s' -Destination '%s'\"", srcFsal, dstFsal);
        if (pw_shell(cmd, NULL, 0) != 0) {
            ui_print_error("Failed to copy fsal.exe");
            return 2;
        }
    }

    char bin[512]; pw_join(dir, ".fsal\\bin", bin, sizeof(bin));
    pw_ensure_dir(bin);
    
    char err[256];
    if (pw_add_to_user_path(bin, err, sizeof(err)) != 0) {
        ui_print_warning("Failed to add to PATH");
    }

    char shim[512]; pw_join(bin, "fsal.cmd", shim, sizeof(shim));
    char body[1024]; snprintf(body, sizeof(body), "@echo off\r\n\"%s\" %%*\r\n", dstFsal);
    pw_write_text(shim, body);

    // Recommended packages - use embedded content
    char* text = malloc(strlen(EMBEDDED_RECOMMENDED_FSAL) + 1);
    strcpy(text, EMBEDDED_RECOMMENDED_FSAL);
    
    FsalDep deps[10];
    int count = parse_fsal_deps(text, deps, 10, NULL, 0);
    for (int i = 0; i < count; i++) {
        if (ui_confirm(deps[i].name, deps[i].description)) {
            ui_print_step("Unpacking", deps[i].name);
            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "\"%s\" unpack %s", dstFsal, deps[i].git);
            pw_shell(cmd, NULL, 0);
        }
    }
    free(text);

    // Welt is recommended by default
    if (ui_confirm("Welt", "The Welt programming language runtime and compiler.")) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "\"%s\" unpack welt", dstFsal);
        pw_shell(cmd, NULL, 0);
    }

    return 0;
}
