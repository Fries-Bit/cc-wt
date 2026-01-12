#include "internal.h"
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static int get_fsal_value(const char* path, const char* section, const char* key, char* out, size_t cap) {
    FILE* f = fopen(path, "r");
    if(!f) return -1;
    char line[512];
    int in_section = 0;
    char section_bracket[128];
    snprintf(section_bracket, sizeof(section_bracket), "[%s]", section);
    while(fgets(line, sizeof(line), f)) {
        char* l = line;
        while(*l && isspace((unsigned char)*l)) l++;
        if(!*l || *l == '#' || *l == ';') continue;
        if(*l == '[') {
            char* rb = strchr(l, ']');
            if(rb) {
                size_t len = (size_t)(rb - l + 1);
                if(strncmp(l, section_bracket, len) == 0) in_section = 1;
                else in_section = 0;
            } else in_section = 0;
            continue;
        }
        if(!in_section) continue;
        char* eq = strchr(l, '=');
        if(!eq) continue;
        char k[64]; size_t klen = (size_t)(eq - l);
        if(klen >= sizeof(k)) klen = sizeof(k)-1;
        strncpy(k, l, klen); k[klen] = '\0';
        char* tk = k; while(*tk && isspace((unsigned char)*tk)) tk++;
        char* ek = tk + strlen(tk) - 1;
        while(ek > tk && isspace((unsigned char)*ek)) { *ek = '\0'; ek--; }
        if(strcmp(tk, key) == 0) {
            char* v = eq + 1;
            while(*v && isspace((unsigned char)*v)) v++;
            char* ev = v + strlen(v) - 1;
            while(ev > v && isspace((unsigned char)*ev)) { *ev = '\0'; ev--; }
            char* colon = strrchr(v, ':');
            if(colon) *colon = '\0';
            ev = v + strlen(v) - 1;
            while(ev > v && isspace((unsigned char)*ev)) { *ev = '\0'; ev--; }
            if(*v == '"') {
                v++;
                size_t vlen = strlen(v);
                if(vlen > 0 && v[vlen-1] == '"') v[vlen-1] = '\0';
            }
            snprintf(out, cap, "%s", v);
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -1;
}

static int evaluate_comparison(const char* left_val, const char* right_val, WeltTokenType op) {
    if (!left_val || !right_val) return 0;
    int is_numeric = 1;
    if(left_val[0] == '\0') is_numeric = 0;
    else {
        for(int i=0; left_val[i]; i++) {
            if(!isdigit(left_val[i]) && left_val[i]!='.' && left_val[i]!='-' && !isspace(left_val[i])) { is_numeric = 0; break; }
        }
    }
    if(is_numeric) {
        if(right_val[0] == '\0') is_numeric = 0;
        else {
            for(int i=0; right_val[i]; i++) {
                if(!isdigit(right_val[i]) && right_val[i]!='.' && right_val[i]!='-' && !isspace(right_val[i])) { is_numeric = 0; break; }
            }
        }
    }
    if(is_numeric) {
        double l = atof(left_val); double r = atof(right_val);
        switch(op) {
            case TOKEN_EQ: case TOKEN_STRICT_EQ: return l == r;
            case TOKEN_NE: case TOKEN_LT_GT: return l != r;
            case TOKEN_LT: return l < r;
            case TOKEN_LE: return l <= r;
            case TOKEN_GT: return l > r;
            case TOKEN_GE: return l >= r;
            default: return 0;
        }
    } else {
        int cmp = strcmp(left_val, right_val);
        switch(op) {
            case TOKEN_EQ: case TOKEN_STRICT_EQ: return cmp == 0;
            case TOKEN_NE: case TOKEN_LT_GT: return cmp != 0;
            case TOKEN_LT: return cmp < 0;
            case TOKEN_LE: return cmp <= 0;
            case TOKEN_GT: return cmp > 0;
            case TOKEN_GE: return cmp >= 0;
            default: return 0;
        }
    }
}

Token g_tokens[MAX_TOKENS];
int g_token_count = 0;
char* g_current_src = NULL;
int g_should_return = 0;
static char g_return_value[256] = {0};
static WeltType g_current_expected_ret_type = TYPE_UNDEFINED;

static int is_type_keyword(const char* word){
    const char* types[] = {"string", "integer", "float", "bool", "ss", "bit", "File", "enum", "def_fsal", "table", "array"};
    for(int i = 0; i < 11; i++) if(strcmp(word, types[i]) == 0) return 1;
    return is_alias(word);
}

static const char* resolve_identifier(const char* path, int* pc, int end_tok);

static int handle_function_call(const char* path, int* pc_ptr, int end_tok, int argc, const char** argv) {
    int pc = *pc_ptr;
    Token* tok = &g_tokens[pc];
    char fn_name[128]; strcpy(fn_name, tok->value);
    int fn_pc = -1;
    
    for(int i=0; i < g_token_count; i++){
        if(g_tokens[i].type == TOKEN_KEYWORD && strcmp(g_tokens[i].value, "fn") == 0){
            if(i+1 < g_token_count && strcmp(g_tokens[i+1].value, fn_name) == 0){
                fn_pc = i; break;
            }
        }
    }

    if(fn_pc != -1){
        pc += 2; // Skip name and (
        // Parse arguments
        char args[16][256]; int arg_cnt = 0;
        while(pc < end_tok && g_tokens[pc].type != TOKEN_RPAREN){
            if(g_tokens[pc].type == TOKEN_COMMA){ pc++; continue; }
            const char* v = NULL;
            if(g_tokens[pc].type == TOKEN_IDENTIFIER){
                const char* var_name = g_tokens[pc].value;
                v = resolve_identifier(path, &pc, end_tok);
                if(!v){
                    char err[256]; snprintf(err, sizeof(err), "E0013: use of undefined variable '%s'", var_name);
                    welt_diagnostic(path, g_tokens[pc].line, g_tokens[pc].col, "", err);
                    return 1;
                }
            } else {
                v = g_tokens[pc].value;
            }
            if(v) {
                if (arg_cnt < 16) {
                    strncpy(args[arg_cnt++], v, 255);
                }
            }
            pc++;
        }
        if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
        
        // Find function body and return type
        int p = fn_pc + 2;
        while(p < g_token_count && g_tokens[p].type != TOKEN_LPAREN) p++;
        // Param names mapping could go here
        while(p < g_token_count && g_tokens[p].type != TOKEN_RPAREN) p++;
        if(p < g_token_count) p++;
        WeltType ret_type = TYPE_UNDEFINED;
        if(p < g_token_count && g_tokens[p].type == TOKEN_ARROW){
            p++; if(p < g_token_count){ ret_type = parse_type(g_tokens[p].value); p++; }
        }
        while(p < g_token_count && g_tokens[p].type != TOKEN_LBRACE) p++;
        if(p < g_token_count){
            p++; int b_start = p, depth = 1;
            
            // Map parameters to arguments
            int pp = fn_pc + 2; // Skip name and (
            int cur_arg = 0;
            while(pp < g_token_count && g_tokens[pp].type != TOKEN_RPAREN){
                if(g_tokens[pp].type == TOKEN_KEYWORD || g_tokens[pp].type == TOKEN_IDENTIFIER){
                    if(is_type_keyword(g_tokens[pp].value)){
                        pp++; // Skip type
                    }
                    if(pp < g_token_count && g_tokens[pp].type == TOKEN_IDENTIFIER){
                        char param_name[128]; strcpy(param_name, g_tokens[pp].value);
                        if(cur_arg < arg_cnt){
                            set_variable(param_name, args[cur_arg], TYPE_UNDEFINED, path, g_tokens[pp].line, g_tokens[pp].col);
                        }
                        cur_arg++;
                        pp++;
                    }
                }
                if(pp < g_token_count && g_tokens[pp].type == TOKEN_COMMA) pp++;
                else if(pp < g_token_count && g_tokens[pp].type != TOKEN_IDENTIFIER && g_tokens[pp].type != TOKEN_KEYWORD) pp++;
            }

            while(p < g_token_count && depth > 0){
                if(g_tokens[p].type == TOKEN_LBRACE) depth++;
                else if(g_tokens[p].type == TOKEN_RBRACE) depth--;
                if(depth > 0) p++;
            }
            int b_end = p;
            WeltType old_ret = g_current_expected_ret_type;
            g_current_expected_ret_type = ret_type;
            execute_code_block(path, b_start, b_end, argc, argv);
            g_current_expected_ret_type = old_ret;
            g_should_return = 0; // Reset for caller
        }
        *pc_ptr = pc;
        return 0;
    } else {
        char err[256]; snprintf(err, sizeof(err), "E0014: call to undefined function '%s'", fn_name);
        welt_diagnostic(path, g_tokens[pc].line, g_tokens[pc].col, "", err);
        return 1;
    }
}

static WeltType get_token_value_type(const char* val) {
    if(!val) return TYPE_UNDEFINED;
    if(strcmp(val, "true") == 0 || strcmp(val, "false") == 0) return TYPE_BOOL;
    if(isdigit(val[0]) || (val[0] == '-' && isdigit(val[1]))) {
        if(strchr(val, '.')) return TYPE_FLOAT;
        return TYPE_INTEGER;
    }
    return TYPE_STRING;
}

static int find_main_tokens(int* body_start, int* body_end){
    for(int i = 0; i < g_token_count; i++){
        if(g_tokens[i].type == TOKEN_KEYWORD && strcmp(g_tokens[i].value, "fn") == 0){
            if(i + 1 < g_token_count && g_tokens[i+1].type == TOKEN_KEYWORD && strcmp(g_tokens[i+1].value, "main") == 0){
                int p = i + 2;
                while(p < g_token_count && g_tokens[p].type != TOKEN_LPAREN) p++;
                if(p < g_token_count){
                    while(p < g_token_count && g_tokens[p].type != TOKEN_RPAREN) p++;
                    if(p < g_token_count){
                        p++;
                        // Skip optional -> type
                        if(p < g_token_count && g_tokens[p].type == TOKEN_ARROW){
                            p++;
                            if(p < g_token_count){
                                g_current_expected_ret_type = parse_type(g_tokens[p].value);
                                p++;
                            }
                        }
                        while(p < g_token_count && g_tokens[p].type != TOKEN_LBRACE) p++;
                        if(p < g_token_count){
                            *body_start = p + 1;
                            int depth = 1; p++;
                            while(p < g_token_count && depth > 0){
                                if(g_tokens[p].type == TOKEN_LBRACE) depth++;
                                else if(g_tokens[p].type == TOKEN_RBRACE) depth--;
                                if(depth > 0) p++;
                            }
                            if(depth == 0){ *body_end = p; return 0; }
                        }
                    }
                }
            }
        }
    }
    return -1;
}

static void populate_argv(int argc, const char** argv, int has_sa_arg_param){
    char buf[256]; 
    snprintf(buf, sizeof(buf), "%d", argc);
    set_variable("argc", buf, TYPE_INTEGER, "cli", 0, 0);
    if(has_sa_arg_param){
        for(int i=0; i<argc; i++){
            char idx[64]; snprintf(idx, sizeof(idx), "sa_arg[%d]", i+1);
            set_variable(idx, argv[i], TYPE_STRING, "cli", 0, 0);
        }
    }
}

static const char* resolve_identifier(const char* path, int* pc, int end_tok) {
    static char full_name[256];
    strcpy(full_name, g_tokens[*pc].value);
    int current_pc = *pc;

    if (current_pc + 3 < end_tok && g_tokens[current_pc+1].type == TOKEN_LBRACKET && 
        (g_tokens[current_pc+2].type == TOKEN_NUMBER || g_tokens[current_pc+2].type == TOKEN_IDENTIFIER) && 
        g_tokens[current_pc+3].type == TOKEN_RBRACKET) {
        
        const char* index_val = g_tokens[current_pc+2].value;
        if (g_tokens[current_pc+2].type == TOKEN_IDENTIFIER) {
            index_val = get_variable_value(g_tokens[current_pc+2].value);
        }
        
        if (index_val) {
            snprintf(full_name, sizeof(full_name), "%s[%s]", g_tokens[current_pc].value, index_val);
            *pc += 3;
        }
    }
    
    const char* val = get_variable_value(full_name);
    if (!val && strncmp(full_name, "sa_arg[", 7) == 0) {
        return " ";
    }
    return val;
}

int execute_code_block(const char* path, int start_tok, int end_tok, int argc, const char** argv){
    int pc = start_tok;
    while(pc < end_tok && !g_should_return){
        Token* tok = &g_tokens[pc];
        if(tok->type == TOKEN_EOF) break;
        if(tok->type == TOKEN_SEMICOLON){ pc++; continue; }
        
        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "enum") == 0){
            pc++; // Skip enum name
            if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER) pc++;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_LBRACE){
                pc++; int depth = 1;
                while(pc < end_tok && depth > 0){
                    if(g_tokens[pc].type == TOKEN_LBRACE) depth++;
                    else if(g_tokens[pc].type == TOKEN_RBRACE) depth--;
                    pc++;
                }
            }
            continue;
        }
        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "imp") == 0){
            if(pc + 2 < end_tok && g_tokens[pc+1].type == TOKEN_DOT && g_tokens[pc+2].type == TOKEN_KEYWORD && strcmp(g_tokens[pc+2].value, "lib") == 0){
                pc += 3;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_LBRACE){
                    pc++;
                    while(pc < end_tok && g_tokens[pc].type != TOKEN_RBRACE){
                        if(g_tokens[pc].type == TOKEN_STRING){
                            char inc_file[256]; strcpy(inc_file, g_tokens[pc].value);
                            if(strcmp(inc_file, "fsal") == 0 || strcmp(inc_file, "C-lib") == 0) {
                                pc++; 
                                if(pc < end_tok && g_tokens[pc].type == TOKEN_COMMA) pc++;
                                continue;
                            }
                            char* code = NULL; size_t sz = 0;
                            if(read_welt_file(inc_file, &code, &sz) == 0 && code){
                                Token* saved_tokens = malloc(sizeof(Token) * g_token_count);
                                int saved_count = g_token_count;
                                memcpy(saved_tokens, g_tokens, sizeof(Token) * g_token_count);
                                interpret(inc_file, code, argc, argv);
                                memcpy(g_tokens, saved_tokens, sizeof(Token) * saved_count);
                                g_token_count = saved_count; free(saved_tokens); free(code);
                            }
                            pc++;
                        } else pc++;
                        if(pc < end_tok && g_tokens[pc].type == TOKEN_COMMA) pc++;
                    }
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_RBRACE) pc++;
                    continue;
                }
            }
        }
        
        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "sa_free") == 0){
            pc++;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                pc++;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER){
                    free_variable(g_tokens[pc].value); pc++;
                }
                if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
            }
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }

        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "fwww_at") == 0){
            pc++;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                pc++; char url[256] = {0}, var_name[128] = {0};
                if(pc < end_tok && g_tokens[pc].type == TOKEN_STRING){ strcpy(url, g_tokens[pc].value); pc++; }
                if(pc < end_tok && g_tokens[pc].type == TOKEN_COMMA) pc++;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER){ strcpy(var_name, g_tokens[pc].value); pc++; }
                set_variable(var_name, "content from url", TYPE_STRING, path, tok->line, tok->col);
                if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
            }
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }

        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "crp") == 0){
            if(pc + 1 < end_tok && g_tokens[pc+1].type == TOKEN_DOT){
                pc += 2;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER){
                    char method[64]; strcpy(method, g_tokens[pc].value); pc++;
                    if(strcmp(method, "EndRuntime") == 0) exit(0);
                    if(strcmp(method, "EndRuntimeOutput") == 0){
                        if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                             pc++; // Just exit for now
                             exit(0);
                        }
                    }
                }
            }
        }
        
        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "impdef") == 0){
            pc++;
            if(pc < end_tok && (g_tokens[pc].type == TOKEN_KEYWORD || g_tokens[pc].type == TOKEN_IDENTIFIER)){
                char original[64]; strcpy(original, g_tokens[pc].value); pc++;
                if(pc < end_tok && (g_tokens[pc].type == TOKEN_KEYWORD || g_tokens[pc].type == TOKEN_IDENTIFIER)){
                    char alias[64]; strcpy(alias, g_tokens[pc].value); pc++;
                    add_type_alias(original, alias);
                }
            }
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }

        if(tok->type == TOKEN_NOT && pc + 1 < end_tok && (g_tokens[pc+1].type == TOKEN_IDENTIFIER || g_tokens[pc+1].type == TOKEN_KEYWORD) && strcmp(g_tokens[pc+1].value, "include") == 0){
             pc += 2;
             if(g_tokens[pc].type == TOKEN_STRING){
                 char inc_file[256]; strcpy(inc_file, g_tokens[pc].value); pc++;
                 char* code = NULL; size_t sz = 0;
                 if(read_welt_file(inc_file, &code, &sz) == 0 && code){
                     Token* saved_tokens = malloc(sizeof(Token) * g_token_count);
                     int saved_count = g_token_count;
                     memcpy(saved_tokens, g_tokens, sizeof(Token) * g_token_count);
                     interpret(inc_file, code, argc, argv);
                     memcpy(g_tokens, saved_tokens, sizeof(Token) * saved_count);
                     g_token_count = saved_count; free(saved_tokens); free(code);
                 }
             }
             if(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON){
                 welt_diagnostic(path, g_tokens[pc-1].line, g_tokens[pc-1].col, "", "E0011: expected semicolon after include"); return 1;
             }
             if(pc < end_tok) pc++; continue;
        }

        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "return") == 0){
            pc++;
            char val[256] = {0};
            if(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON){
                const char* v = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                if(v) strcpy(val, v);
                pc++;
            }
            if(g_current_expected_ret_type != TYPE_UNDEFINED){
                WeltType actual = get_token_value_type(val);
                if(actual != g_current_expected_ret_type){
                    fprintf(stderr, "Fatal Error: Function expected to return type %d but returned type %d\n", g_current_expected_ret_type, actual);
                    exit(1);
                }
            }
            strcpy(g_return_value, val);
            g_should_return = 1;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }

        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "Throw") == 0){
            if(pc + 2 < end_tok && g_tokens[pc+1].type == TOKEN_DOT){
                pc += 2;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER){
                    char method[64]; strcpy(method, g_tokens[pc].value); pc++;
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                        pc++; char msg[256] = {0};
                        if(pc < end_tok && g_tokens[pc].type == TOKEN_STRING){ strcpy(msg, g_tokens[pc].value); pc++; }
                        if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                        if(strcmp(method, "error") == 0){
                            fprintf(stderr, "Fatal Error: %s\n", msg);
                            exit(1);
                        } else if(strcmp(method, "warning") == 0){
                            // Bold Purple: \033[1;35m
                            printf("\033[1;35mWarning: %s\033[0m\n", msg);
#ifdef _WIN32
                            Sleep(1000);
#else
                            sleep(1);
#endif
                        }
                    }
                }
            }
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }

        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "control-prog") == 0){
             pc++; // Skip control-prog if it's used as a library marker in imp.lib
             continue;
        }

        if(tok->type == TOKEN_IDENTIFIER && strcmp(tok->value, "ctrlprog") == 0){
            if(pc + 2 < end_tok && g_tokens[pc+1].type == TOKEN_DOT){
                pc += 2;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER){
                    char method[64]; strcpy(method, g_tokens[pc].value); pc++;
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                        pc++;
                        if(strcmp(method, "wait") == 0){
                            int ms = 0;
                            if(pc < end_tok && g_tokens[pc].type == TOKEN_NUMBER) { ms = atoi(g_tokens[pc].value); pc++; }
#ifdef _WIN32
                            Sleep(ms);
#else
                            usleep(ms * 1000);
#endif
                        } else if(strcmp(method, "stop") == 0){
                            exit(0);
                        } else if(strcmp(method, "back") == 0){
                            int target_line = 0;
                            if(pc < end_tok && g_tokens[pc].type == TOKEN_NUMBER) { target_line = atoi(g_tokens[pc].value); pc++; }
                            // Basic-like line jumping: find the first token with line >= target_line
                            for(int i = start_tok; i < end_tok; i++){
                                if(g_tokens[i].line >= target_line){
                                    pc = i; break;
                                }
                            }
                            if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
                            continue; // Jump to target pc
                        }
                        if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                    }
                }
            }
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }

        int is_const = 0, is_sys_ind = 0, saved_pc = pc;
        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "const") == 0){ is_const = 1; pc++; tok = &g_tokens[pc]; }
        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "sys_ind") == 0){ is_sys_ind = 1; pc++; tok = &g_tokens[pc]; }
        
        if(is_type_keyword(tok->value)){
            char type_name[32]; strcpy(type_name, tok->value); pc++;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER){
                char var_name[128]; strcpy(var_name, g_tokens[pc].value); pc++;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_LT){
                     while(pc < end_tok && g_tokens[pc].type != TOKEN_GT) pc++;
                     if(pc < end_tok && g_tokens[pc].type == TOKEN_GT) pc++;
                }
                // Skip optional [var type] in brackets if present (for function pointers or arrays)
                if(pc < end_tok && g_tokens[pc].type == TOKEN_LBRACKET){
                    while(pc < end_tok && g_tokens[pc].type != TOKEN_RBRACKET) pc++;
                    if(pc < end_tok) pc++;
                }
                int has_assign = 0; char value[512] = {0};
                if(pc < end_tok && g_tokens[pc].type == TOKEN_ASSIGN){
                    has_assign = 1; pc++;
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER && (strcmp(g_tokens[pc].value, "def_file") == 0 || strcmp(g_tokens[pc].value, "relate_file") == 0 || strcmp(g_tokens[pc].value, "fsal") == 0)){
                        char func[64]; strcpy(func, g_tokens[pc].value); pc++;
                        if(strcmp(func, "fsal") == 0 && pc < end_tok && g_tokens[pc].type == TOKEN_DOT){
                            pc++; if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER) { strcpy(func, g_tokens[pc].value); pc++; }
                        }
                        if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                            pc++;
                            if(pc < end_tok && (g_tokens[pc].type == TOKEN_STRING || g_tokens[pc].type == TOKEN_IDENTIFIER)){
                                const char* v = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                                if(v) strcpy(value, v);
                                pc++;
                            }
                            if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                        }
                        if(strcmp(func, "def_file") == 0){
                            FILE* f = fopen(value, "w"); if(f) fclose(f);
                        }
                    } else if(pc + 2 < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER && g_tokens[pc+1].type == TOKEN_DOT && strcmp(g_tokens[pc+2].value, "get") == 0){
                        char fsal_var[128]; strcpy(fsal_var, g_tokens[pc].value); pc += 3;
                        if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                            pc++; char section[128] = {0}, key[128] = {0};
                            if(pc < end_tok && (g_tokens[pc].type == TOKEN_STRING || g_tokens[pc].type == TOKEN_IDENTIFIER)){
                                const char* s = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                                if(s) strcpy(section, s); pc++;
                            }
                            if(pc < end_tok && g_tokens[pc].type == TOKEN_COMMA) pc++;
                            if(pc < end_tok && (g_tokens[pc].type == TOKEN_STRING || g_tokens[pc].type == TOKEN_IDENTIFIER)){
                                const char* k = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                                if(k) strcpy(key, k); pc++;
                            }
                            if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                            const char* path = get_variable_value(fsal_var);
                            if(path) get_fsal_value(path, section, key, value, sizeof(value));
                        }
                    } else {
                        double result = 0;
                        int is_math = 0;

                        while(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON){
                            if(pc + 1 < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER && g_tokens[pc+1].type == TOKEN_LPAREN){
                                // Function call in declaration
                                if (handle_function_call(path, &pc, end_tok, argc, argv) != 0) return 1;
                                strcat(value, g_return_value);
                                g_should_return = 0;
                            } else {
                                const char* v = NULL;
                                if(g_tokens[pc].type == TOKEN_IDENTIFIER){
                                    v = get_variable_value(g_tokens[pc].value);
                                    if(!v){
                                        char err[256]; snprintf(err, sizeof(err), "E0013: use of undefined variable '%s'", g_tokens[pc].value);
                                        welt_diagnostic(path, g_tokens[pc].line, g_tokens[pc].col, "", err);
                                        return 1;
                                    }
                                } else {
                                    v = g_tokens[pc].value;
                                }
                                if(v){
                                    if(g_tokens[pc].type == TOKEN_NUMBER || (g_tokens[pc].type == TOKEN_IDENTIFIER && isdigit(v[0]))){
                                         if(!is_math) { result = atof(v); is_math = 1; }
                                         else {
                                             if(pc > 0 && g_tokens[pc-1].type == TOKEN_PLUS) result += atof(v);
                                             else if(pc > 0 && g_tokens[pc-1].type == TOKEN_MINUS) result -= atof(v);
                                         }
                                    } else {
                                        strcat(value, v);
                                    }
                                }
                                pc++;
                            }
                        }
                        if(is_math && value[0] == '\0') snprintf(value, sizeof(value), "%g", result);
                        if(value[0] == '\0' && !is_math) strcpy(value, g_return_value);
                    }
                }
                while(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON) pc++;
                if(has_assign) set_variable(var_name, value, parse_type(type_name), path, g_tokens[saved_pc].line, g_tokens[saved_pc].col);
                else {
                    if(get_variable(var_name)){
                        WeltVariable* existing = get_variable(var_name);
                        char err[512]; 
                        snprintf(err, sizeof(err), "E0012: redefinition of variable '%s'", var_name);
                        welt_diagnostic(path, g_tokens[saved_pc].line, g_tokens[saved_pc].col, "", err);
                        
                        if (existing->path) {
                            fprintf(stderr, "  \033[1;34mnote\033[0m: first defined here in %s:%d:%d\n", existing->path, existing->line, existing->col);
                        }
                        return 1;
                    }
                    create_variable(var_name, parse_type(type_name), path, g_tokens[saved_pc].line, g_tokens[saved_pc].col);
                }
                WeltVariable* v = get_variable(var_name);
                if(v){ v->is_const = is_const; v->is_sys_ind = is_sys_ind; }
                if(pc < end_tok) pc++; continue;
            }
        }
        pc = saved_pc; tok = &g_tokens[pc];

        if(tok->type == TOKEN_IDENTIFIER && pc + 1 < end_tok && g_tokens[pc+1].type == TOKEN_LPAREN){
            if (handle_function_call(path, &pc, end_tok, argc, argv) != 0) return 1;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }

        if(tok->type == TOKEN_IDENTIFIER && pc + 1 < end_tok && g_tokens[pc+1].type == TOKEN_ASSIGN){
            char var_name[128]; strcpy(var_name, tok->value); pc += 2; char value[512] = {0};
            int is_math = 0; double result = 0;

            if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER && pc + 1 < end_tok && g_tokens[pc+1].type == TOKEN_LPAREN){
                 // Execute function call for assignment
                 if (handle_function_call(path, &pc, end_tok, argc, argv) != 0) return 1;
                 strcpy(value, g_return_value);
                 g_should_return = 0;
            } else if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER && (strcmp(g_tokens[pc].value, "def_file") == 0 || strcmp(g_tokens[pc].value, "relate_file") == 0 || strcmp(g_tokens[pc].value, "fsal") == 0)){
                char func[64]; strcpy(func, g_tokens[pc].value); pc++;
                if(strcmp(func, "fsal") == 0 && pc < end_tok && g_tokens[pc].type == TOKEN_DOT){
                    pc++; if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER) { strcpy(func, g_tokens[pc].value); pc++; }
                }
                if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                    pc++;
                    if(g_tokens[pc].type == TOKEN_STRING || g_tokens[pc].type == TOKEN_IDENTIFIER){
                        const char* v = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                        if(v) strcpy(value, v);
                        pc++;
                    }
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                }
                if(strcmp(func, "def_file") == 0){
                    FILE* f = fopen(value, "w"); if(f) fclose(f);
                }
            } else if(pc + 2 < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER && g_tokens[pc+1].type == TOKEN_DOT && strcmp(g_tokens[pc+2].value, "get") == 0){
                char fsal_var[128]; strcpy(fsal_var, g_tokens[pc].value); pc += 3;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                    pc++; char section[128] = {0}, key[128] = {0};
                    if(pc < end_tok && (g_tokens[pc].type == TOKEN_STRING || g_tokens[pc].type == TOKEN_IDENTIFIER)){
                        const char* s = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                        if(s) strcpy(section, s); pc++;
                    }
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_COMMA) pc++;
                    if(pc < end_tok && (g_tokens[pc].type == TOKEN_STRING || g_tokens[pc].type == TOKEN_IDENTIFIER)){
                        const char* k = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                        if(k) strcpy(key, k); pc++;
                    }
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                    const char* path = get_variable_value(fsal_var);
                    if(path) get_fsal_value(path, section, key, value, sizeof(value));
                }
            } else {
                double result = 0;
                int is_math = 0;
                while(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON){
                    if(pc + 1 < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER && g_tokens[pc+1].type == TOKEN_LPAREN){
                        execute_code_block(path, pc, end_tok, argc, argv);
                        strcat(value, g_return_value);
                        g_should_return = 0;
                        while(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON && g_tokens[pc].type != TOKEN_PLUS && g_tokens[pc].type != TOKEN_MINUS) pc++;
                    } else {
                        const char* v = NULL;
                        if(g_tokens[pc].type == TOKEN_IDENTIFIER){
                            const char* vn = g_tokens[pc].value;
                            v = resolve_identifier(path, &pc, end_tok);
                            if(!v){
                                char err[256]; snprintf(err, sizeof(err), "E0013: use of undefined variable '%s'", vn);
                                welt_diagnostic(path, g_tokens[pc].line, g_tokens[pc].col, "", err);
                                return 1;
                            }
                        } else {
                            v = g_tokens[pc].value;
                        }
                        if(v){
                            if(g_tokens[pc].type == TOKEN_NUMBER || (g_tokens[pc].type == TOKEN_IDENTIFIER && isdigit(v[0]))){
                                if(!is_math) { result = atof(v); is_math = 1; }
                                else {
                                    if(pc > 0 && g_tokens[pc-1].type == TOKEN_PLUS) result += atof(v);
                                    else if(pc > 0 && g_tokens[pc-1].type == TOKEN_MINUS) result -= atof(v);
                                }
                            } else {
                                strcat(value, v);
                            }
                        }
                        pc++;
                    }
                }
                if(is_math && value[0] == '\0') snprintf(value, sizeof(value), "%g", result);
                if(value[0] == '\0' && !is_math) strcpy(value, g_return_value);
            }
            if(!get_variable(var_name)){
                char err[256]; snprintf(err, sizeof(err), "E0013: use of undefined variable '%s'", var_name);
                welt_diagnostic(path, g_tokens[pc-2].line, g_tokens[pc-2].col, "", err);
                return 1;
            }
            set_variable(var_name, value, TYPE_UNDEFINED, path, tok->line, tok->col);
            if(pc < end_tok) pc++; continue;
        }
        
        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "print") == 0){
            pc++;
            if(g_tokens[pc].type == TOKEN_LPAREN){
                pc++; char fmt[512] = {0};
                if(g_tokens[pc].type == TOKEN_STRING){ strcpy(fmt, g_tokens[pc].value); pc++; }
                char args[16][256]; int arg_count = 0;
                while(pc < end_tok && g_tokens[pc].type != TOKEN_RPAREN){
                    if(g_tokens[pc].type == TOKEN_COMMA) { pc++; continue; }
                    const char* val = NULL;
                    if(g_tokens[pc].type == TOKEN_IDENTIFIER){
                        const char* var_name = g_tokens[pc].value;
                        val = resolve_identifier(path, &pc, end_tok);
                        if(!val){
                            char err[256]; snprintf(err, sizeof(err), "E0013: use of undefined variable '%s'", var_name);
                            welt_diagnostic(path, g_tokens[pc].line, g_tokens[pc].col, "", err);
                            return 1;
                        }
                    } else {
                        val = g_tokens[pc].value;
                    }
                    if(arg_count < 16) strcpy(args[arg_count++], val ? val : "(null)");
                    pc++;
                }
                if(g_tokens[pc].type == TOKEN_RPAREN) pc++;
                if(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON){
                    welt_diagnostic(path, g_tokens[pc-1].line, g_tokens[pc-1].col, "", "E0009: expected semicolon after print"); return 1;
                }
                if(arg_count > 0){
                    char final_msg[2048] = {0}; char* pfmt = fmt; int cur_arg = 0;
                    while(*pfmt){
                        if(*pfmt == '{' && *(pfmt+1) == '}' && *(pfmt+2) == '@'){ if(cur_arg < arg_count) strcat(final_msg, args[cur_arg++]); pfmt += 3; }
                        else if(*pfmt == '{' && *(pfmt+1) == '}'){ if(cur_arg < arg_count) strcat(final_msg, args[cur_arg++]); pfmt += 2; }
                        else { int L = strlen(final_msg); final_msg[L] = *pfmt; final_msg[L+1] = '\0'; pfmt++; }
                    }
                    printf("%s\n", final_msg);
                } else printf("%s\n", fmt);
                if(pc < end_tok) pc++; continue;
            }
        }

        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "execute_C") == 0){
            pc++;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                pc++; char code[2048] = {0};
                if(pc < end_tok && (g_tokens[pc].type == TOKEN_STRING || g_tokens[pc].type == TOKEN_IDENTIFIER)){
                    const char* v = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                    if(v) strcpy(code, v); pc++;
                }
                if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                printf("[C-Library] Executing: %s\n", code);
            }
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }

        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "if") == 0){
            pc++; 
            int has_bracket = (g_tokens[pc].type == TOKEN_LBRACKET); 
            int has_paren = (g_tokens[pc].type == TOKEN_LPAREN);
            if(has_bracket || has_paren) pc++;
            
            char cond_val[256] = {0};
            int cond_result = 0;
            
            // Enhanced expression evaluation (==, !=, <, >, <=, >=)
            if(pc + 2 < end_tok && (g_tokens[pc+1].type == TOKEN_EQ || g_tokens[pc+1].type == TOKEN_NE || g_tokens[pc+1].type == TOKEN_LT || g_tokens[pc+1].type == TOKEN_GT || g_tokens[pc+1].type == TOKEN_LE || g_tokens[pc+1].type == TOKEN_GE || g_tokens[pc+1].type == TOKEN_STRICT_EQ || g_tokens[pc+1].type == TOKEN_LT_GT)){
                const char* left = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? resolve_identifier(path, &pc, end_tok) : g_tokens[pc].value;
                pc++;
                WeltTokenType op = g_tokens[pc].type;
                pc++;
                const char* right = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? resolve_identifier(path, &pc, end_tok) : g_tokens[pc].value;
                if(left && right) cond_result = evaluate_comparison(left, right, op);
                pc++;
            } else if(pc < end_tok){
                const char* v = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? resolve_identifier(path, &pc, end_tok) : g_tokens[pc].value;
                if(v){
                    strcpy(cond_val, v);
                    cond_result = (strcmp(v, "true") == 0 || (isdigit(v[0]) && atoi(v) != 0) || (v[0] != '\0' && strcmp(v, "false") != 0 && strcmp(v, "0") != 0));
                }
                pc++;
            }
            
            if(has_bracket && g_tokens[pc].type == TOKEN_RBRACKET) pc++;
            if(has_paren && g_tokens[pc].type == TOKEN_RPAREN) pc++;
            
            while(pc < end_tok && g_tokens[pc].type != TOKEN_LBRACE) pc++;
            if(pc < end_tok){
                pc++; int block_start = pc, depth = 1;
                while(pc < end_tok && depth > 0){
                    if(g_tokens[pc].type == TOKEN_LBRACE) depth++;
                    else if(g_tokens[pc].type == TOKEN_RBRACE) depth--;
                    if(depth > 0) pc++;
                }
                int block_end = pc; pc++;
                
                // Check if it's a match-like if (first token after { is a literal or def)
                if(block_start < block_end && (g_tokens[block_start].type == TOKEN_STRING || g_tokens[block_start].type == TOKEN_NUMBER || (g_tokens[block_start].type == TOKEN_KEYWORD && strcmp(g_tokens[block_start].value, "def")==0))){
                    int mpc = block_start, matched = 0;
                    while(mpc < block_end){
                        if(g_tokens[mpc].type == TOKEN_RBRACE) break;
                        char key[256]; strcpy(key, g_tokens[mpc].value);
                        int is_def = (g_tokens[mpc].type == TOKEN_KEYWORD && strcmp(g_tokens[mpc].value, "def")==0);
                        mpc++;
                        if(mpc < block_end && g_tokens[mpc].type == TOKEN_ARROW){
                            mpc++;
                            if(mpc < block_end && g_tokens[mpc].type == TOKEN_LBRACKET){
                                mpc++; int b_start = mpc, b_depth = 1;
                                while(mpc < block_end && b_depth > 0){
                                    if(g_tokens[mpc].type == TOKEN_LBRACKET) b_depth++;
                                    else if(g_tokens[mpc].type == TOKEN_RBRACKET) b_depth--;
                                    if(b_depth > 0) mpc++;
                                }
                                int b_end = mpc; mpc++;
                                if(!matched && (is_def || strcmp(key, cond_val) == 0)){
                                    execute_code_block(path, b_start, b_end, argc, argv);
                                    matched = 1;
                                }
                            }
                        }
                        if(mpc < block_end && g_tokens[mpc].type == TOKEN_COMMA) mpc++;
                    }
                } else {
                    if(cond_result) execute_code_block(path, block_start, block_end, argc, argv);
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_KEYWORD && strcmp(g_tokens[pc].value, "else") == 0){
                        pc++; if(g_tokens[pc].type == TOKEN_LBRACE){
                            pc++; int e_start = pc, e_depth = 1;
                            while(pc < end_tok && e_depth > 0){
                                if(g_tokens[pc].type == TOKEN_LBRACE) e_depth++;
                                else if(g_tokens[pc].type == TOKEN_RBRACE) e_depth--;
                                if(e_depth > 0) pc++;
                            }
                            int e_end = pc; pc++;
                            if(!cond_result) execute_code_block(path, e_start, e_end, argc, argv);
                        } else {
                             int e_start = pc; while(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON) pc++;
                             if(!cond_result) execute_code_block(path, e_start, pc, argc, argv);
                             if(pc < end_tok) pc++;
                        }
                    }
                }
                continue;
            }
        }
        
        if(tok->type == TOKEN_KEYWORD && strcmp(tok->value, "for") == 0){
            pc++; if(g_tokens[pc].type == TOKEN_LBRACKET){
                pc++; int init_start = pc; while(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON) pc++;
                int init_end = pc; pc++; int cond_start = pc; while(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON) pc++;
                int cond_end = pc; pc++; int incr_start = pc; while(pc < end_tok && g_tokens[pc].type != TOKEN_RBRACKET) pc++;
                int incr_end = pc; pc++; while(pc < end_tok && g_tokens[pc].type != TOKEN_LBRACE) pc++;
                pc++; int body_start = pc, depth = 1;
                while(pc < end_tok && depth > 0){
                    if(g_tokens[pc].type == TOKEN_LBRACE) depth++;
                    else if(g_tokens[pc].type == TOKEN_RBRACE) depth--;
                    if(depth > 0) pc++;
                }
                int body_end = pc; pc++; execute_code_block(path, init_start, init_end, argc, argv);
                while(!g_should_return){
                    int cond = 0; 
                    if(cond_start + 2 < cond_end && (g_tokens[cond_start+1].type == TOKEN_EQ || g_tokens[cond_start+1].type == TOKEN_NE || g_tokens[cond_start+1].type == TOKEN_LT || g_tokens[cond_start+1].type == TOKEN_GT || g_tokens[cond_start+1].type == TOKEN_LE || g_tokens[cond_start+1].type == TOKEN_GE || g_tokens[cond_start+1].type == TOKEN_STRICT_EQ || g_tokens[cond_start+1].type == TOKEN_LT_GT)){
                        const char* left = (g_tokens[cond_start].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[cond_start].value) : g_tokens[cond_start].value;
                        WeltTokenType op = g_tokens[cond_start+1].type;
                        const char* right = (g_tokens[cond_start+2].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[cond_start+2].value) : g_tokens[cond_start+2].value;
                        if(left && right) cond = evaluate_comparison(left, right, op);
                    } else if(cond_start < cond_end) {
                        const char* v = (g_tokens[cond_start].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[cond_start].value) : g_tokens[cond_start].value;
                        if(v) cond = (strcmp(v, "true") == 0 || (isdigit(v[0]) && atoi(v) != 0) || (v[0] != '\0' && strcmp(v, "false") != 0 && strcmp(v, "0") != 0));
                    }
                    if(!cond) break;
                    execute_code_block(path, body_start, body_end, argc, argv);
                    execute_code_block(path, incr_start, incr_end, argc, argv);
                }
                continue;
            }
        }

        if(tok->type == TOKEN_IDENTIFIER && pc + 1 < end_tok && g_tokens[pc+1].type == TOKEN_DOT){
            char var_name[128]; strcpy(var_name, tok->value);
            WeltVariable* var = get_variable(var_name);
            if(var){
                pc += 2;
                if(pc < end_tok && g_tokens[pc].type == TOKEN_IDENTIFIER){
                    char method[64]; strcpy(method, g_tokens[pc].value); pc++;
                    if(pc < end_tok && g_tokens[pc].type == TOKEN_LPAREN){
                        pc++; char args[4][256]; int arg_count = 0;
                        while(pc < end_tok && g_tokens[pc].type != TOKEN_RPAREN){
                            if(g_tokens[pc].type == TOKEN_COMMA) { pc++; continue; }
                            const char* val = (g_tokens[pc].type == TOKEN_IDENTIFIER) ? get_variable_value(g_tokens[pc].value) : g_tokens[pc].value;
                            if(val && arg_count < 4) strcpy(args[arg_count++], val);
                            pc++;
                        }
                        if(pc < end_tok && g_tokens[pc].type == TOKEN_RPAREN) pc++;
                        
                        if(var->type == TYPE_FILE){
                            if(strcmp(method, "write") == 0 && arg_count >= 1){ FILE* f = fopen(var->value, "w"); if(f){ fprintf(f, "%s", args[0]); fclose(f); } }
                            else if(strcmp(method, "read") == 0){
                                FILE* f = fopen(var->value, "r");
                                if(f){
                                    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                                    if(sz > 0){
                                        char* b = malloc(sz + 1); fread(b, 1, sz, f); b[sz] = '\0';
                                        set_variable("__last_ret", b, TYPE_STRING, path, 0, 0); free(b);
                                    } else set_variable("__last_ret", "", TYPE_STRING, path, 0, 0);
                                    fclose(f);
                                } else set_variable("__last_ret", "", TYPE_STRING, path, 0, 0);
                            }
                            else if(strcmp(method, "del") == 0){ remove(var->value); }
                            else if(strcmp(method, "mov") == 0 && arg_count >= 1){ if(arg_count == 1) { rename(var->value, args[0]); strcpy(var->value, args[0]); } else if(arg_count >= 2) { rename(args[0], args[1]); if(strcmp(var->value, args[0])==0) strcpy(var->value, args[1]); } }
                        } else {
                            if(strcmp(method, "has") == 0 && arg_count >= 1){
                                // Mock return value logic
                                set_variable("__last_ret", strstr(var->value, args[0]) ? "true" : "false", TYPE_BOOL, path, tok->line, tok->col);
                            } else if(strcmp(method, "length_of_variable") == 0){
                                char buf[32]; snprintf(buf, sizeof(buf), "%zu", strlen(var->value));
                                set_variable("__last_ret", buf, TYPE_INTEGER, path, tok->line, tok->col);
                            } else if(strcmp(method, "bytes") == 0){
                                set_variable("__last_ret", var->value, TYPE_STRING, path, tok->line, tok->col);
                            }
                        }
                    }
                }
            }
            while(pc < end_tok && g_tokens[pc].type != TOKEN_SEMICOLON) pc++;
            if(pc < end_tok) pc++; continue;
        }
        
        if(tok->type == TOKEN_IDENTIFIER && pc + 1 < end_tok && g_tokens[pc+1].type == TOKEN_INC){
            const char* v = get_variable_value(tok->value);
            if(v){ char buf[32]; snprintf(buf, sizeof(buf), "%d", atoi(v) + 1); set_variable(tok->value, buf, TYPE_INTEGER, path, tok->line, tok->col); }
            pc += 2;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }
        
        if(tok->type == TOKEN_IDENTIFIER && pc + 1 < end_tok && g_tokens[pc+1].type == TOKEN_DEC){
            const char* v = get_variable_value(tok->value);
            if(v){ char buf[32]; snprintf(buf, sizeof(buf), "%d", atoi(v) - 1); set_variable(tok->value, buf, TYPE_INTEGER, path, tok->line, tok->col); }
            pc += 2;
            if(pc < end_tok && g_tokens[pc].type == TOKEN_SEMICOLON) pc++;
            continue;
        }
        pc++;
    }
    return 0;
}

int interpret(const char* path, const char* src, int argc, const char** argv){
    g_should_return = 0; memset(g_return_value, 0, sizeof(g_return_value));
    g_current_expected_ret_type = TYPE_UNDEFINED;
    if(tokenize(path, src) != 0) return 1;
    int has_sa_arg = 0, body_start = -1, body_end = -1;
    if(find_main_tokens(&body_start, &body_end) == 0){
        for(int i = 0; i < body_start; i++){
             if(g_tokens[i].type == TOKEN_KEYWORD && strcmp(g_tokens[i].value, "fn") == 0){
                if(i + 1 < g_token_count && g_tokens[i+1].type == TOKEN_KEYWORD && strcmp(g_tokens[i+1].value, "main") == 0){
                    int p = i + 2; while(p < body_start && g_tokens[p].type != TOKEN_RPAREN){
                        if(g_tokens[p].type == TOKEN_IDENTIFIER && strcmp(g_tokens[p].value, "sa_arg") == 0) {
                            has_sa_arg = 1;
                            p++;
                            if(p < body_start && g_tokens[p].type == TOKEN_LBRACKET) {
                                p++;
                                if(p < body_start && g_tokens[p].type == TOKEN_RBRACKET) {
                                    p++;
                                    if(p < body_start && g_tokens[p].type == TOKEN_LT) {
                                        p++;
                                        while(p < body_start && g_tokens[p].type != TOKEN_GT) p++;
                                        if(p < body_start && g_tokens[p].type == TOKEN_GT) p++;
                                    }
                                }
                            }
                            continue;
                        }
                        p++;
                    } break;
                }
             }
        }
        populate_argv(argc, argv, has_sa_arg);
        return execute_code_block(path, body_start, body_end, argc, argv);
    } else {
        populate_argv(argc, argv, 0);
        return execute_code_block(path, 0, g_token_count, argc, argv);
    }
}
