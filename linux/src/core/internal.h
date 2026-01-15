#ifndef WELT_INTERNAL_H
#define WELT_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "welt.h"

#define MAX_VARIABLES 512
#define MAX_ARRAY_ITEMS 1024
#define MAX_TABLE_ITEMS 1024
#define MAX_TOKENS 8192

typedef enum {
    TYPE_UNDEFINED,
    TYPE_STRING,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_ARRAY,
    TYPE_TABLE,
    TYPE_POINTER,
    TYPE_BIT,
    TYPE_FILE,
    TYPE_FSAL
} WeltType;

typedef struct {
    char alias[64];
    char original[64];
} WeltTypeAlias;

typedef struct {
    char name[128];
    WeltType return_type;
    int body_start;
    int body_end;
} WeltFunction;

typedef struct {
    char name[128];
    char* value;
    WeltType type;
    int is_const;
    int is_sys_ind;
    int line;
    int col;
    const char* path;
    int refcount;
} WeltVariable;

typedef enum {
    TOKEN_EOF, TOKEN_KEYWORD, TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_HEX, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET, TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_COMMA,
    TOKEN_DOT, TOKEN_ARROW, TOKEN_PLUS, TOKEN_INC, TOKEN_MINUS, TOKEN_DEC,
    TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT, TOKEN_ASSIGN, TOKEN_EQ,
    TOKEN_STRICT_EQ, TOKEN_NE, TOKEN_LT, TOKEN_LE, TOKEN_GT, TOKEN_GE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT, TOKEN_QUESTION, TOKEN_AMPERSAND,
    TOKEN_PIPE, TOKEN_CARET, TOKEN_TILDE, TOKEN_LT_GT, TOKEN_INVALID
} WeltTokenType;

typedef struct {
    WeltTokenType type;
    char value[512];
    int line;
    int col;
} Token;

// Globals (defined in welt_core.c)
extern Token g_tokens[MAX_TOKENS];
extern int g_token_count;
extern char* g_current_src;
extern int g_should_return;

// Diagnostic
void welt_diagnostic(const char* path, int line, int col, const char* codeLine, const char* msg);

// Lexer
int tokenize(const char* path, const char* src);
int tokenize_append(const char* path, const char* src);
int lexer_check(const char* path, const char* src);
int is_keyword(const char* word);

// Variables
WeltVariable* get_variable(const char* name);
void set_variable(const char* name, const char* value, WeltType type, const char* path, int line, int col);
void free_variable(const char* name);
WeltVariable* create_variable(const char* name, WeltType type, const char* path, int line, int col);
const char* get_variable_value(const char* name);
WeltType parse_type(const char* typeStr);
const char* get_type_name(WeltType type);
void add_type_alias(const char* original, const char* alias);
int is_alias(const char* word);
const char* get_original_type(const char* alias);

// Interpreter
int interpret(const char* path, const char* src, int argc, const char** argv);
int execute_code_block(const char* path, int start_tok, int end_tok, int argc, const char** argv);
int read_welt_file(const char* p, char** out, size_t* sz);
int get_fsal_dir(char* out, size_t cap);

#endif
