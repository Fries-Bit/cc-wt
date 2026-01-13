#ifndef FSAL_UI_H
#define FSAL_UI_H

#include <stddef.h>

// Rust-style colors
#define FSAL_COLOR_RED     "\x1b[31m"
#define FSAL_COLOR_GREEN   "\x1b[32m"
#define FSAL_COLOR_YELLOW  "\x1b[33m"
#define FSAL_COLOR_BLUE    "\x1b[34m"
#define FSAL_COLOR_MAGENTA "\x1b[35m"
#define FSAL_COLOR_CYAN    "\x1b[36m"
#define FSAL_COLOR_BOLD    "\x1b[1m"
#define FSAL_COLOR_RESET   "\x1b[0m"

void ui_print_step(const char* action, const char* target);
void ui_print_error(const char* msg);
void ui_print_warning(const char* msg);
void ui_loading_bar(size_t current, size_t total, const char* label);
int ui_confirm(const char* name, const char* desc);

#endif
