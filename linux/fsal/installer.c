#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "platform.h"
#include "ui.h"
#include "config.h"

static int get_self_exe(char* out, size_t cap){
    ssize_t len = readlink("/proc/self/exe", out, cap - 1);
    if (len != -1) {
        out[len] = '\0';
        return 0;
    }
    return -1;
}

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

int main(int argc, char** argv) {
    ui_print_step("Installing", "FSAL Runtime (Linux)");

    const char* home = getenv("HOME");
    char defDir[512];
    if (home) snprintf(defDir, sizeof(defDir), "%s/.fsal_root", home);
    else {
        char app[512]; 
        if (pw_get_appdata(app, sizeof(app)) != 0) { strcpy(app, "."); }
        snprintf(defDir, sizeof(defDir), "%s/FSAL", app);
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

    char exe[512]; if(get_self_exe(exe, sizeof(exe)) != 0) strcpy(exe, argv[0]);
    char curDir[512]; snprintf(curDir, sizeof(curDir), "%s", exe);
    char* slash = strrchr(curDir, '/'); if (slash) *slash = '\0';

    char srcFsal[512]; pw_join(curDir, "fsal", srcFsal, sizeof(srcFsal));
    char dstFsal[512]; pw_join(dir, "fsal", dstFsal, sizeof(dstFsal));
    
    ui_print_step("Setting up", "fsal");
    
    int fsal_ready = 0;
    if (pw_file_exists(srcFsal)) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "cp -f \"%s\" \"%s\"", srcFsal, dstFsal);
        if (pw_shell(cmd, NULL, 0) == 0) {
            fsal_ready = 1;
            chmod(dstFsal, 0755);
        }
    }
    if (!fsal_ready) {
        ui_print_step("Extracting", "embedded fsal");
        if (extract_embedded_fsal(exe, dstFsal) == 0) {
            fsal_ready = 1;
            chmod(dstFsal, 0755);
        } else {
            ui_print_warning("Could not prepare fsal manager, skipping package installation");
        }
    }

    char bin[512]; pw_join(dir, ".fsal/bin", bin, sizeof(bin));
    pw_ensure_dir(bin);
    
    char err[256];
    if (pw_add_to_user_path(bin, err, sizeof(err)) != 0) {
        ui_print_warning("Failed to add to PATH");
    }

    char shim[512]; pw_join(bin, "fsal", shim, sizeof(shim));
    char body[1024]; snprintf(body, sizeof(body), "#!/bin/bash\n\"%s\" \"$@\"\n", dstFsal);
    pw_write_text(shim, body);
    chmod(shim, 0755);

    char* text = NULL;
    size_t textSize = 0;
    char configPath[512];
    char projRoot[512];
    char* lastSlash = strrchr(curDir, '/');
    if (lastSlash) {
        size_t len = (size_t)(lastSlash - curDir);
        if (len >= sizeof(projRoot)) len = sizeof(projRoot) - 1;
        strncpy(projRoot, curDir, len);
        projRoot[len] = '\0';
    } else {
        snprintf(projRoot, sizeof(projRoot), "%s", curDir);
    }
    pw_join(projRoot, "config/r_config.fsal", configPath, sizeof(configPath));
    
    if (pw_read_text(configPath, &text, &textSize) == 0 && text) {
        FsalDep deps[10];
        int count = parse_fsal_deps(text, deps, 10, NULL, 0);
        for (int i = 0; i < count; i++) {
            if (ui_confirm(deps[i].name, deps[i].description, deps[i].version, deps[i].git)) {
                ui_print_step("Unpacking", deps[i].name);
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "\"%s\" unpack %s", dstFsal, deps[i].git);
                pw_shell(cmd, NULL, 0);
            }
        }
        free(text);
    } else {
        ui_print_warning("Could not read config/r_config.fsal, skipping recommended packages");
    }

    if (ui_confirm("Welt", "The Welt programming language,\nincludes the standard library and compiler.", 0, "https://github.com/Fries-Bit/Welt :: Official Welt Git Repository")) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "\"%s\" unpack welt", dstFsal);
        pw_shell(cmd, NULL, 0);
    }

    return 0;
}
