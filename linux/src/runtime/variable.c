#include "internal.h"

static WeltVariable g_variables[MAX_VARIABLES];
static int g_var_count = 0;
static WeltTypeAlias g_type_aliases[32];
static int g_alias_count = 0;

void add_type_alias(const char* original, const char* alias){
    if(g_alias_count < 32){
        strcpy(g_type_aliases[g_alias_count].original, original);
        strcpy(g_type_aliases[g_alias_count].alias, alias);
        g_alias_count++;
    }
}

int is_alias(const char* word){
    for(int i=0; i<g_alias_count; i++) if(strcmp(word, g_type_aliases[i].alias) == 0) return 1;
    return 0;
}

const char* get_original_type(const char* alias){
    for(int i=0; i<g_alias_count; i++) if(strcmp(alias, g_type_aliases[i].alias) == 0) return g_type_aliases[i].original;
    return alias;
}

WeltVariable* get_variable(const char* name){
    for(int i=0; i<g_var_count; i++){
        if(strcmp(g_variables[i].name, name) == 0){
            return &g_variables[i];
        }
    }
    return NULL;
}

WeltVariable* create_variable(const char* name, WeltType type, const char* path, int line, int col){
    if(g_var_count >= MAX_VARIABLES) return NULL;
    
    WeltVariable* var = &g_variables[g_var_count];
    snprintf(var->name, sizeof(var->name), "%s", name);
    var->type = type;
    var->value = (char*)malloc(256);
    var->value[0] = '\0';
    var->is_const = 0;
    var->is_sys_ind = 0;
    var->line = line;
    var->col = col;
    var->path = path;
    var->refcount = 1;
    
    g_var_count++;
    return var;
}

void set_variable(const char* name, const char* value, WeltType type, const char* path, int line, int col){
    WeltVariable* var = get_variable(name);
    if(var && var->is_const){
        return;
    }
    
    if(!var){
        var = create_variable(name, type, path, line, col);
    }
    
    if(var && value){
        if(var->value) free(var->value);
        var->value = (char*)malloc(strlen(value) + 1);
        strcpy(var->value, value);
    }
}

void free_variable(const char* name){
    for(int i=0; i<g_var_count; i++){
        if(strcmp(g_variables[i].name, name) == 0){
            if(g_variables[i].value) free(g_variables[i].value);
            // Shift remaining variables
            for(int j=i; j<g_var_count-1; j++){
                g_variables[j] = g_variables[j+1];
            }
            g_var_count--;
            return;
        }
    }
}

const char* get_variable_value(const char* name){
    WeltVariable* var = get_variable(name);
    return var ? var->value : NULL;
}

WeltType parse_type(const char* typeStr){
    const char* t = get_original_type(typeStr);
    if(strcmp(t, "string") == 0) return TYPE_STRING;
    if(strcmp(t, "integer") == 0) return TYPE_INTEGER;
    if(strcmp(t, "float") == 0) return TYPE_FLOAT;
    if(strcmp(t, "bool") == 0) return TYPE_BOOL;
    if(strcmp(t, "bit") == 0) return TYPE_BIT;
    if(strcmp(t, "ss") == 0) return TYPE_STRING;
    if(strcmp(t, "File") == 0) return TYPE_FILE;
    if(strcmp(t, "def_fsal") == 0) return TYPE_FSAL;
    if(strcmp(t, "table") == 0) return TYPE_TABLE;
    if(strcmp(t, "array") == 0) return TYPE_ARRAY;
    return TYPE_UNDEFINED;
}

const char* get_type_name(WeltType type){
    switch(type){
        case TYPE_STRING: return "string";
        case TYPE_INTEGER: return "integer";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_BIT: return "bit";
        case TYPE_FILE: return "File";
        case TYPE_FSAL: return "FSAL";
        case TYPE_TABLE: return "table";
        case TYPE_ARRAY: return "array";
        case TYPE_POINTER: return "pointer";
        case TYPE_UNDEFINED: return "undefined";
        default: return "unknown";
    }
}
