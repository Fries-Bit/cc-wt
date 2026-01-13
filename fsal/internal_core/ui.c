#include "ui.h"
#include <stdio.h>
#include <string.h>

void ui_print_step(const char* action, const char* target) {
    printf("%s%s%s %s\n", FSAL_COLOR_GREEN, FSAL_COLOR_BOLD, action, FSAL_COLOR_RESET);
    if (target) {
        printf("    %s\n", target);
    }
}

void ui_print_error(const char* msg) {
    fprintf(stderr, "%s%serror:%s %s\n", FSAL_COLOR_RED, FSAL_COLOR_BOLD, FSAL_COLOR_RESET, msg);
}

void ui_print_warning(const char* msg) {
    fprintf(stderr, "%s%swarning:%s %s\n", FSAL_COLOR_YELLOW, FSAL_COLOR_BOLD, FSAL_COLOR_RESET, msg);
}

void ui_loading_bar(size_t current, size_t total, const char* label) {
    const int width = 20;
    float progress = (total > 0) ? (float)current / total : 0;
    int pos = (int)(width * progress);

    printf("\r%s [", label ? label : "");
    for (int i = 0; i < width; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf("-");
    }
    
    // Convert to KB or MB for display
    if (total > 1024 * 1024) {
        printf("] %.2fMB/%.2fMB", (float)current / (1024 * 1024), (float)total / (1024 * 1024));
    } else {
        printf("] %zuKB/%zuKB", current / 1024, total / 1024);
    }
    fflush(stdout);
    if (current >= total && total > 0) printf("\n");
}

int ui_confirm(const char* name, const char* desc, int version, const char* git) {
    printf("%s%s%s\n", FSAL_COLOR_BOLD, name, FSAL_COLOR_RESET);
    
    for (const char* p = desc; *p; p++) {
        if (p[0] == '\\' && p[1] == 'n') {
            printf("\n");
            p++;
        } else {
            printf("%c", *p);
        }
    }
    printf("\n");
    
    if (version > 0) {
        printf("version: %d\n", version);
    }
    printf("git: %s\n", git);
    
    printf("\nThis package was recommended with FSAL, would you wish to install it? [Y/n]? ");
    fflush(stdout);
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin)) {
        if (buf[0] == 'n' || buf[0] == 'N') return 0;
    }
    return 1;
}
