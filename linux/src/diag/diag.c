#include "internal.h"

void welt_diagnostic(const char* path, int line, int col, const char* codeLine, const char* msg){
    char line_buf[512] = {0};
    if(codeLine && codeLine[0] != '\0'){
        snprintf(line_buf, sizeof(line_buf), "%s", codeLine);
    } else if(g_current_src) {
        const char* p = g_current_src;
        int cur_line = 1;
        while(*p && cur_line < line){
            if(*p == '\n') cur_line++;
            p++;
        }
        int i = 0;
        while(*p && *p != '\n' && *p != '\r' && i < 511){
            line_buf[i++] = *p++;
        }
        line_buf[i] = '\0';
    }

    // Rust-like styling with ANSI colors
    fprintf(stderr, "\033[1;31merror\033[0m\033[1m: %s\033[0m\n", msg);
    fprintf(stderr, "  \033[1;34m-->\033[0m %s:%d:%d\n", path, line, col);
    fprintf(stderr, "\033[1;34m   |\033[0m\n");
    fprintf(stderr, "\033[1;34m%2d |\033[0m %s\n", line, line_buf);
    fprintf(stderr, "\033[1;34m   |\033[0m ");
    for(int i = 0; i < col - 1; i++) fprintf(stderr, " ");
    fprintf(stderr, "\033[1;31m^ here\033[0m\n");
    fprintf(stderr, "\033[1;34m   |\033[0m\n");
}
