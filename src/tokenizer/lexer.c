#include "internal.h"
#include <ctype.h>

int is_keyword(const char* word){
    const char* keywords[] = {
        "fn", "main", "if", "else", "for", "while", "print", "ss_input",
        "struct", "structure", "const", "return", "true", "false", "def", "multiline",
        "Extension", "EndRuntime", "EndRuntimeOutput", "include",
        "integer", "float", "string", "bool", "ss", "class", "table", "array",
        "sys_ind", "bit", "global_extentions", "imp", "lib", "File", "impdef",
        "enum", "fwww_at", "sa_free", "has", "addToTable", "addToArray",
        "def_fsal", "execute_C", "Throw", "control-prog", "wait", "stop", "back", "retype", "crp"
    };
    int count = sizeof(keywords) / sizeof(keywords[0]);
    for(int i = 0; i < count; i++){
        if(strcmp(word, keywords[i]) == 0) return 1;
    }
    return 0;
}

int tokenize(const char* path, const char* src){
    g_token_count = 0;
    g_current_src = (char*)src;
    const char* p = src;
    int line = 1, col = 1;
    
    while(*p && g_token_count < MAX_TOKENS){
        if(*p == ' ' || *p == '\t'){ p++; col++; continue; }
        if(*p == '\n'){ line++; col = 1; p++; continue; }
        if(*p == '\r'){ p++; continue; }
        
        if(*p == '/' && *(p+1) == '/'){
            while(*p && *p != '\n') p++;
            continue;
        }
        
        if(*p == '/' && *(p+1) == '*'){
            p += 2;
            while(*p && !(*p == '*' && *(p+1) == '/')){
                if(*p == '\n') line++;
                p++;
            }
            if(*p == '*') p += 2;
            continue;
        }
        
        Token* tok = &g_tokens[g_token_count];
        tok->line = line;
        tok->col = col;
        
        if(*p == '('){ tok->type = TOKEN_LPAREN; strcpy(tok->value, "("); p++; col++; }
        else if(*p == ')'){ tok->type = TOKEN_RPAREN; strcpy(tok->value, ")"); p++; col++; }
        else if(*p == '{'){ tok->type = TOKEN_LBRACE; strcpy(tok->value, "{"); p++; col++; }
        else if(*p == '}'){ tok->type = TOKEN_RBRACE; strcpy(tok->value, "}"); p++; col++; }
        else if(*p == '['){ tok->type = TOKEN_LBRACKET; strcpy(tok->value, "["); p++; col++; }
        else if(*p == ']'){ tok->type = TOKEN_RBRACKET; strcpy(tok->value, "]"); p++; col++; }
        else if(*p == ';'){ tok->type = TOKEN_SEMICOLON; strcpy(tok->value, ";"); p++; col++; }
        else if(*p == ':'){ tok->type = TOKEN_COLON; strcpy(tok->value, ":"); p++; col++; }
        else if(*p == ','){ tok->type = TOKEN_COMMA; strcpy(tok->value, ","); p++; col++; }
        else if(*p == '.'){ tok->type = TOKEN_DOT; strcpy(tok->value, "."); p++; col++; }
        else if(*p == '?'){ tok->type = TOKEN_QUESTION; strcpy(tok->value, "?"); p++; col++; }
        else if(*p == '=' && *(p+1) == '=' && *(p+2) == '='){ tok->type = TOKEN_STRICT_EQ; strcpy(tok->value, "==="); p+=3; col+=3; }
        else if(*p == '=' && *(p+1) == '='){ tok->type = TOKEN_EQ; strcpy(tok->value, "=="); p+=2; col+=2; }
        else if(*p == '!' && *(p+1) == '='){ tok->type = TOKEN_NE; strcpy(tok->value, "!="); p+=2; col+=2; }
        else if(*p == '<' && *(p+1) == '='){ tok->type = TOKEN_LE; strcpy(tok->value, "<="); p+=2; col+=2; }
        else if(*p == '>' && *(p+1) == '='){ tok->type = TOKEN_GE; strcpy(tok->value, ">="); p+=2; col+=2; }
        else if(*p == '<' && *(p+1) == '>'){ tok->type = TOKEN_LT_GT; strcpy(tok->value, "<>"); p+=2; col+=2; }
        else if(*p == '&' && *(p+1) == '&'){ tok->type = TOKEN_AND; strcpy(tok->value, "&&"); p+=2; col+=2; }
        else if(*p == '|' && *(p+1) == '|'){ tok->type = TOKEN_OR; strcpy(tok->value, "||"); p+=2; col+=2; }
        else if(*p == '<'){ tok->type = TOKEN_LT; strcpy(tok->value, "<"); p++; col++; }
        else if(*p == '>'){ tok->type = TOKEN_GT; strcpy(tok->value, ">"); p++; col++; }
        else if(*p == '='){ tok->type = TOKEN_ASSIGN; strcpy(tok->value, "="); p++; col++; }
        else if(*p == '+'){
            if(*(p+1) == '+'){ tok->type = TOKEN_INC; strcpy(tok->value, "++"); p+=2; col+=2; }
            else { tok->type = TOKEN_PLUS; strcpy(tok->value, "+"); p++; col++; }
        }
        else if(*p == '-'){
            if(*(p+1) == '-'){ tok->type = TOKEN_DEC; strcpy(tok->value, "--"); p+=2; col+=2; }
            else if(*(p+1) == '>'){ tok->type = TOKEN_ARROW; strcpy(tok->value, "->"); p+=2; col+=2; }
            else { tok->type = TOKEN_MINUS; strcpy(tok->value, "-"); p++; col++; }
        }
        else if(*p == '*'){ tok->type = TOKEN_STAR; strcpy(tok->value, "*"); p++; col++; }
        else if(*p == '/'){ tok->type = TOKEN_SLASH; strcpy(tok->value, "/"); p++; col++; }
        else if(*p == '%'){ tok->type = TOKEN_PERCENT; strcpy(tok->value, "%"); p++; col++; }
        else if(*p == '!'){ tok->type = TOKEN_NOT; strcpy(tok->value, "!"); p++; col++; }
        else if(*p == '&'){ tok->type = TOKEN_AMPERSAND; strcpy(tok->value, "&"); p++; col++; }
        else if(*p == '|'){ tok->type = TOKEN_PIPE; strcpy(tok->value, "|"); p++; col++; }
        else if(*p == '^'){ tok->type = TOKEN_CARET; strcpy(tok->value, "^"); p++; col++; }
        else if(*p == '~'){ tok->type = TOKEN_TILDE; strcpy(tok->value, "~"); p++; col++; }
        else if(*p == '"'){
            tok->type = TOKEN_STRING; p++; col++; int i = 0;
            while(*p && i < 510){
                if(*p == '\\' && *(p+1) == '"'){ tok->value[i++] = '"'; p += 2; col += 2; }
                else if(*p == '"') break;
                else {
                    tok->value[i++] = *p; if(*p == '\n') line++; p++; col++;
                }
            }
            tok->value[i] = '\0'; if(*p == '"'){ p++; col++; }
        }
        else if(isdigit(*p) || (*p == '<' && *(p+1) == 'h' && *(p+2) == 'e' && *(p+3) == 'x' && *(p+4) == '>')){
            if(*p == '<'){ tok->type = TOKEN_HEX; p += 5; col += 5; int i = 0;
                while(*p && *p != '>' && i < 510){ tok->value[i++] = *p; p++; col++; }
                if(*p == '>'){ p++; col++; } tok->value[i] = '\0';
            } else {
                tok->type = TOKEN_NUMBER; int i = 0;
                while(*p && (isdigit(*p) || *p == '.') && i < 510){ tok->value[i++] = *p; p++; col++; }
                tok->value[i] = '\0';
            }
        }
        else if(isalpha(*p) || *p == '_'){
            int i = 0;
            while(*p && (isalnum(*p) || *p == '_') && i < 510){
                tok->value[i++] = *p; p++; col++;
            }
            tok->value[i] = '\0';
            if(is_keyword(tok->value)) tok->type = TOKEN_KEYWORD;
            else tok->type = TOKEN_IDENTIFIER;
        }
        else {
            tok->type = TOKEN_INVALID; tok->value[0] = *p; tok->value[1] = '\0'; p++; col++;
        }
        g_token_count++;
    }
    
    if(g_token_count < MAX_TOKENS){
        g_tokens[g_token_count].type = TOKEN_EOF; g_tokens[g_token_count].value[0] = '\0'; g_token_count++;
    }
    return 0;
}

int lexer_check(const char* path, const char* src){
    int brace=0, paren=0, bracket=0; 
    int line=1, col=1; 
    const char* p=src; 
    const char* lineStart=src;
    
    while(*p){
        if(*p=='\n'){ line++; col=1; lineStart=p+1; p++; continue; }
        if(*p=='{'){ brace++; }
        else if(*p=='}'){ brace--; if(brace<0){ welt_diagnostic(path,line,col,"", "E0001: unmatched closing brace"); return -1; } }
        else if(*p=='('){ paren++; }
        else if(*p==')'){ paren--; if(paren<0){ welt_diagnostic(path,line,col,"", "E0002: unmatched closing parenthesis"); return -1; } }
        else if(*p=='['){ bracket++; }
        else if(*p==']'){ bracket--; if(bracket<0){ welt_diagnostic(path,line,col,"", "E0003: unmatched closing bracket"); return -1; } }
        else if(*p=='"'){
            p++; col++;
            while(*p){
                if(*p == '\\' && *(p+1) == '"'){ p += 2; col += 2; }
                else if(*p == '"') break;
                else {
                    if(*p == '\n'){ line++; col=1; lineStart=p+1; }
                    p++; col++;
                }
            }
        }
        
        if(*p=='/' && *(p+1)=='/'){ while(*p && *p!='\n') p++; continue; }
        if(*p=='/' && *(p+1)=='*'){
            p+=2; while(*p && !(*p=='*' && *(p+1)=='/')){ 
                if(*p=='\n'){ line++; col=1; lineStart=p+1; } p++; col++; 
            }
            if(*p){ p+=2; col+=2; } continue;
        }
        p++; col++;
    }
    
    if(brace!=0){ welt_diagnostic(path,line,col,(lineStart?lineStart:""), "E0004: unclosed braces"); return -2; }
    if(paren!=0){ welt_diagnostic(path,line,col,(lineStart?lineStart:""), "E0005: unclosed parentheses"); return -2; }
    if(bracket!=0){ welt_diagnostic(path,line,col,(lineStart?lineStart:""), "E0006: unclosed brackets"); return -2; }
    
    return 0;
}
