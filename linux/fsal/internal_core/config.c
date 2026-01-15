#include "config.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

static const char* trim(const char* s){
    while(*s && isspace((unsigned char)*s)) s++;
    return s;
}

int parse_fsal_config(const char* text, FsalConfig* outCfg, char* err, int errCap){
    if(!text||!outCfg){ if(err){snprintf(err,errCap,"bad args");} return -1; }
    memset(outCfg,0,sizeof(*outCfg));
    int in_section=0;
    const char* p=text;
    char line[512];
    while(*p){
        size_t i=0; while(*p && *p!='\n' && i<sizeof(line)-1){ line[i++]=*p++; }
        if(*p=='\n') p++; line[i]='\0';
        const char* s=trim(line);
        if(s[0]=='#' || s[0]==';' || s[0]=='\0') continue;
        if(s[0]=='['){ if(strncmp(s,"[configure]",11)==0) in_section=1; else in_section=0; continue; }
        if(!in_section) continue;
        const char* eq=strchr(s,'='); if(!eq) continue;
        char key[64]; char val[256];
        size_t klen= (size_t)(eq - s);
        if(klen >= sizeof(key)) klen = sizeof(key)-1;
        strncpy(key, s, klen); key[klen]='\0';
        const char* rhs = trim(eq+1);
        size_t vlen = strlen(rhs);
        const char* colon = strchr(rhs, ':');
        if(colon && colon > rhs) {
            const char* test = colon + 1;
            int is_type = 0;
            while(*test && (isalpha(*test) || *test == '_')) test++;
            if(*test == '\0' || isspace(*test)) is_type = 1;
            if(is_type) vlen = (size_t)(colon - rhs);
        }
        if(vlen >= sizeof(val)) vlen = sizeof(val)-1;
        strncpy(val, rhs, vlen); val[vlen]='\0';
        // strip quotes if present
        if(val[0]=='"'){
            size_t L=strlen(val);
            if(L>1 && val[L-1]=='"'){ val[L-1]='\0'; memmove(val, val+1, L-1); }
        }
        // normalize key
        for(char* t=key; *t; ++t) if(*t==' ') *t='\0';
        if(strcmp(key,"name")==0){ strncpy(outCfg->name,val,sizeof(outCfg->name)-1); }
        else if(strcmp(key,"cmduse")==0){ strncpy(outCfg->cmduse,val,sizeof(outCfg->cmduse)-1); }
        else if(strcmp(key,"version")==0){ outCfg->version=atof(val); }
    }
    if(outCfg->name[0]==0 || outCfg->cmduse[0]==0 || outCfg->version<=0){
        if(err) snprintf(err,errCap,"missing keys in config");
        return -2;
    }
    return 0;
}

int parse_fsal_deps(const char* text, FsalDep* outDeps, int maxDeps, char* err, int errCap) {
    if(!text||!outDeps){ if(err){snprintf(err,errCap,"bad args");} return -1; }
    int count = 0;
    const char* p = text;
    int in_list = 0;
    
    char line[512];
    while(*p && count < maxDeps) {
        size_t i=0; while(*p && *p!='\n' && i<sizeof(line)-1){ line[i++]=*p++; }
        if(*p=='\n') p++; line[i]='\0';
        const char* s = trim(line);
        if(s[0]=='#' || s[0]==';' || s[0]=='\0') continue;
        
        if(s[0]=='[') {
            if(strncmp(s, "[listdep]", 9) == 0 || strncmp(s, "[dependencies]", 14) == 0) in_list = 1;
            else in_list = 0;
            continue;
        }
        
        if(!in_list) continue;
        
        // Look for "Ndep = [", "Ndep_d = [", "N_d = [", etc.
        if(strstr(s, "=") && strstr(s, "[")) {
            FsalDep* d = &outDeps[count];
            memset(d, 0, sizeof(*d));
            
            // Read until closing bracket "]"
            while(*p && count < maxDeps) {
                i=0; while(*p && *p!='\n' && i<sizeof(line)-1){ line[i++]=*p++; }
                if(*p=='\n') p++; line[i]='\0';
                s = trim(line);
                if(s[0] == ']') { count++; break; }
                
                const char* eq = strchr(s, '=');
                if(!eq) continue;
                
                char key[64], val[256];
                size_t klen = (size_t)(eq - s);
                if(klen >= sizeof(key)) klen = sizeof(key)-1;
                strncpy(key, s, klen); key[klen]='\0';
                for(char* t=key; *t; ++t) if(isspace(*t)) *t='\0';
                
                const char* rhs = trim(eq+1);
                size_t vlen = strlen(rhs);
                const char* colon = strchr(rhs, ':');
                if(colon && colon > rhs) {
                    const char* test = colon + 1;
                    int is_type = 0;
                    while(*test && (isalpha(*test) || *test == '_')) test++;
                    if(*test == '\0' || isspace(*test)) is_type = 1;
                    if(is_type) vlen = (size_t)(colon - rhs);
                }
                if(vlen >= sizeof(val)) vlen = sizeof(val)-1;
                strncpy(val, rhs, vlen); val[vlen]='\0';
                
                if(val[0]=='"') {
                    size_t L=strlen(val);
                    if(L>1 && val[L-1]=='"') { val[L-1]='\0'; memmove(val, val+1, L-1); }
                }
                
                if(strcmp(key, "name") == 0) strncpy(d->name, val, sizeof(d->name)-1);
                else if(strcmp(key, "description") == 0) strncpy(d->description, val, sizeof(d->description)-1);
                else if(strcmp(key, "version") == 0) d->version = atoi(val);
                else if(strcmp(key, "git") == 0) strncpy(d->git, val, sizeof(d->git)-1);
            }
        }
    }
    return count;
}
