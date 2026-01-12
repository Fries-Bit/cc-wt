#include "internal.h"
#ifdef _WIN32
#include <windows.h>
#endif

int read_welt_file(const char* p, char** out, size_t* sz){
    FILE* f=fopen(p,"rb"); 
    if(!f) return -1; 
    fseek(f,0,SEEK_END); 
    long L=ftell(f); 
    fseek(f,0,SEEK_SET);
    char* m=(char*)malloc(L+1); 
    if(!m){ fclose(f); return -2; }
    if((long)fread(m,1,L,f)!=L){ fclose(f); free(m); return -3; }
    fclose(f); 
    m[L]='\0'; 
    if(out)*out=m; 
    if(sz)*sz=(size_t)L; 
    else free(m); 
    return 0;
}

int welt_run_code(const char* name, const char* code, int argc, const char** argv){
    return interpret(name, code, argc, argv);
}

int welt_inweld(const char* filePath, int argc, const char** argv){
    char* code=0; size_t sz=0; 
    if(read_welt_file(filePath,&code,&sz)!=0){ fprintf(stderr,"welt: cannot read %s\n", filePath); return 1; }
    int rc = lexer_check(filePath, code);
    if(rc==0) interpret(filePath, code, argc, argv); 
    free(code);
    return rc==0?0:1;
}

int welt_cweld(const char* filePath, const char* outExeOpt){
    char* code=0; size_t sz=0; 
    if(read_welt_file(filePath,&code,&sz)!=0){ fprintf(stderr,"cannot read %s\n", filePath); return 1; }
    int rc = lexer_check(filePath, code);
    if(rc!=0){ free(code); return 2; }
    const char* out = outExeOpt? outExeOpt : "welt_prog.exe";
    char selfPath[1024];
#ifdef _WIN32
    GetModuleFileNameA(NULL, selfPath, sizeof(selfPath));
#else
    strcpy(selfPath, "fsal.exe");
#endif
    FILE* src = fopen(selfPath, "rb");
    if(!src){ fprintf(stderr, "welt: cannot open self %s\n", selfPath); free(code); return 3; }
    FILE* dst = fopen(out, "wb");
    if(!dst){ fprintf(stderr, "welt: cannot create output %s\n", out); fclose(src); free(code); return 4; }
    char buffer[8192]; size_t n;
    while((n = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, n, dst);
    fclose(src);
    uint32_t codeSize = (uint32_t)sz;
    uint32_t magic = 0x544C5750;
    fwrite(code, 1, sz, dst);
    fwrite(&codeSize, 4, 1, dst);
    fwrite(&magic, 4, 1, dst);
    fclose(dst);
    free(code);
    return 0;
}
