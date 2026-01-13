#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <direct.h>
#include <stdint.h>

#include "platform_win.h"
#include "zipwrap_ps.h"
#include "fsnet.h"
#include "config.h"
#include "welt.h"
#include "ui.h"

static void appdata_fsaldir(char* out, size_t cap){
    const char* up = getenv("USERPROFILE");
    if(up) {
        strncpy(out, up, cap-1);
        out[cap-1] = '\0';
    } else {
        char app[512]; 
        if(pw_get_appdata(app,sizeof(app))!=0){ 
            strncpy(out, ".", cap-1); 
            out[cap-1] = '\0';
            return; 
        }
        snprintf(out, cap, "%s\\FSAL", app);
    }
}

static int is_welt_unpacked() {
    char root[512]; appdata_fsaldir(root, sizeof(root));
    char bin[512], inweld[512], cweld[512];
    pw_join(root, ".fsal\\bin", bin, sizeof(bin));
    pw_join(bin, "inweld.cmd", inweld, sizeof(inweld));
    pw_join(bin, "cweld.cmd", cweld, sizeof(cweld));
    return pw_file_exists(inweld) && pw_file_exists(cweld);
}

static void usage(){
    printf("%s%sFSAL CLI%s\n", FSAL_COLOR_BOLD, FSAL_COLOR_CYAN, FSAL_COLOR_RESET);
    printf("Usage:\n");
    printf("  fsal unpack <repo-url|welt>\n");
    if(is_welt_unpacked()) {
        printf("  fsal welt inweld <file.wt>\n");
        printf("  fsal welt cweld <file.wt> [-into out.exe]\n");
    }
}

static int ensure_runtime_layout(char* rootOut, size_t cap){
    char root[512]; appdata_fsaldir(root,sizeof(root));
    if(pw_ensure_dir(root)!=0) return -1;
    char p[512];
    pw_join(root, ".fsal", p, sizeof(p)); pw_ensure_dir(p);
    pw_join(root, ".fsal\\bin", p, sizeof(p)); pw_ensure_dir(p);
    pw_join(root, ".fsal\\welt", p, sizeof(p)); pw_ensure_dir(p);
    if(rootOut) snprintf(rootOut,cap,"%s", root);
    return 0;
}

static int register_command_shim(const char* cmd, const char* targetExe, const char* extraArgs){
    char root[512]; appdata_fsaldir(root,sizeof(root));
    char bin[512]; pw_join(root, ".fsal\\bin", bin, sizeof(bin));
    if(pw_ensure_dir(bin)!=0) return -1; 
    char shimFile[512]; char tmp[256]; snprintf(tmp,sizeof(tmp),"%s.cmd", cmd);
    pw_join(bin, tmp, shimFile, sizeof(shimFile));
    char body[4096];
    if(extraArgs && extraArgs[0]) snprintf(body,sizeof(body), "@echo off\r\nsetlocal enabledelayedexpansion\r\n\"%s\" %s %%*\r\nendlocal\r\n", targetExe, extraArgs);
    else snprintf(body,sizeof(body), "@echo off\r\nsetlocal enabledelayedexpansion\r\n\"%s\" %%*\r\nendlocal\r\n", targetExe);
    int rc = pw_write_text(shimFile, body);
    if(rc!=0) return rc;
    pw_add_to_user_path(bin, NULL, 0);
    return 0;
}

static int find_config_in_extracted(const char* dir, char* cfgPath, size_t cfgCap, char* pkgRoot, size_t pkgCap){
    WIN32_FIND_DATAA fd; char pat[512]; snprintf(pat,sizeof(pat),"%s\\*", dir);
    HANDLE h = FindFirstFileA(pat,&fd); if(h==INVALID_HANDLE_VALUE) return -1;
    int rc=-1;
    do {
        if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            if(strcmp(fd.cFileName,".")==0||strcmp(fd.cFileName,"..")==0) continue;
            char root[512]; snprintf(root,sizeof(root),"%s\\%s", dir, fd.cFileName);
            char cf[512]; snprintf(cf,sizeof(cf),"%s\\.fsal\\config.fsal", root);
            if(pw_file_exists(cf)){
                snprintf(cfgPath,cfgCap,"%s", cf);
                snprintf(pkgRoot,pkgCap,"%s", root);
                rc=0; break;
            }
        }
    } while(FindNextFileA(h,&fd));
    FindClose(h);
    return rc;
}

static void dl_progress(size_t current, size_t total, void* userdata) {
    ui_loading_bar(current, total, (const char*)userdata);
}

static int cmd_unpack(int argc, char** argv){
    if(argc<3){ ui_print_error("unpack: missing repo url or 'welt'"); return 1; }
    char root[512]; if(ensure_runtime_layout(root,sizeof(root))!=0){ ui_print_error("runtime layout failed"); return 2; }
    if(strcmp(argv[2],"welt")==0){
        char self[512]; GetModuleFileNameA(NULL, self, sizeof(self));
        if(register_command_shim("inweld", self, "welt inweld")!=0) return 3;
        if(register_command_shim("cweld", self, "welt cweld")!=0) return 3;
        ui_print_step("Unpacked", "welt");
        return 0;
    }
    ui_print_step("Unpacking", argv[2]);
    char tmpdir[512]; pw_join(root, "_tmp", tmpdir, sizeof(tmpdir)); pw_ensure_dir(tmpdir);
    char zip[512]; pw_join(tmpdir, "repo.zip", zip, sizeof(zip));
    if(fs_download_github_zip(argv[2], zip, dl_progress, "Downloading")!=0){ 
        ui_print_error("Failed to download repository"); return 4; 
    }
    char exdir[512]; pw_join(tmpdir, "extract", exdir, sizeof(exdir)); pw_ensure_dir(exdir);
    if(pw_expand_archive(zip, exdir, NULL, 0)!=0){ 
        ui_print_error("Failed to extract repository"); return 5; 
    }
    char cfg[512], pkgRoot[512]; if(find_config_in_extracted(exdir,cfg,sizeof(cfg),pkgRoot,sizeof(pkgRoot))!=0){
        ui_print_error(".fsal/config.fsal not found in repo"); return 6;
    }
    char* cfgText=0; size_t sz=0; if(pw_read_text(cfg,&cfgText,&sz)!=0){ ui_print_error("cannot read config"); return 7; }
    FsalConfig fc; char perr[128]; if(parse_fsal_config(cfgText,&fc,perr,sizeof(perr))!=0){ ui_print_error(perr); free(cfgText); return 8; }
    free(cfgText);
    char fsalDir[512]; pw_join(root, ".fsal", fsalDir, sizeof(fsalDir));
    char dest[512]; pw_join(fsalDir, fc.name, dest, sizeof(dest)); pw_ensure_dir(dest);
    char cmd[1024]; snprintf(cmd,sizeof(cmd),"robocopy \"%s\" \"%s\" /MIR /NFL /NDL /NJH /NJS /nc /ns /np", pkgRoot, dest);
    pw_shell(cmd,NULL,0);
    char providedCmd[512]; snprintf(providedCmd,sizeof(providedCmd),"%s\\.fsal\\%s", dest, fc.cmduse);
    if(pw_file_exists(providedCmd)) register_command_shim(fc.cmduse, providedCmd, NULL);
    else {
        char runwt[512]; snprintf(runwt,sizeof(runwt),"%s\\run.wt", dest);
        if(pw_file_exists(runwt)){
            char self[512]; GetModuleFileNameA(NULL, self, sizeof(self));
            char args[1024]; snprintf(args,sizeof(args),"welt inweld \"%s\"", runwt);
            register_command_shim(fc.cmduse, self, args);
        }
    }
    ui_print_step("Unpacked", fc.name);
    return 0;
}

static int dispatch_welt(int argc, char** argv){
    if(argc<2){ usage(); return 1; }
    if(strcmp(argv[1],"welt")==0){
        if(!is_welt_unpacked()){
            ui_print_error("Welt is not unpacked. Run 'fsal unpack welt' first.");
            return 1;
        }
        if(argc<3){ fprintf(stderr,"welt: missing subcommand (inweld|cweld)\n"); return 1; }
        if(strcmp(argv[2],"inweld")==0){ if(argc<4){ fprintf(stderr,"inweld: need file.wt\n"); return 1;} return welt_inweld(argv[3], argc-4, (const char**)(argv+4)); }
        if(strcmp(argv[2],"cweld")==0){ if(argc<4){ fprintf(stderr,"cweld: need file.wt\n"); return 1;} const char* out=NULL; if(argc>=6 && strcmp(argv[4],"-into")==0) out=argv[5]; return welt_cweld(argv[3], out); }
        fprintf(stderr,"unknown welt subcommand\n"); return 1;
    }
    return -1;
}

static int check_and_run_embedded(int argc, char** argv){
    char selfPath[1024];
    if(GetModuleFileNameA(NULL, selfPath, sizeof(selfPath)) == 0) return -1;
    FILE* self = fopen(selfPath, "rb");
    if(!self) return -1;
    fseek(self, 0, SEEK_END); long fileSize = ftell(self); fseek(self, 0, SEEK_SET);
    if(fileSize < 8){ fclose(self); return -1; }
    fseek(self, fileSize - 8, SEEK_SET); uint32_t codeSize=0, magic=0;
    if(fread(&codeSize, 4, 1, self)!=1 || fread(&magic, 4, 1, self)!=1){ fclose(self); return -1; }
    if(magic != 0x544C5750){ fclose(self); return -1; }
    if(codeSize > 1024*1024 || codeSize == 0){ fclose(self); return -1; }
    fseek(self, fileSize - 8 - codeSize, SEEK_SET);
    char* code = (char*)malloc(codeSize+1);
    if(!code || fread(code, 1, codeSize, self)!=codeSize){ if(code) free(code); fclose(self); return -1; }
    code[codeSize]='\0'; fclose(self);
    int ret = welt_run_code("embedded.wt", code, argc-1, (const char**)(argv+1));
    free(code); return ret;
}

static int installer_main(const char* exe) {
    ui_print_step("Installing", "FSAL Runtime");
    const char* up = getenv("USERPROFILE");
    char defDir[512];
    if (up) snprintf(defDir, sizeof(defDir), "%s", up);
    else {
        char app[512]; 
        if (pw_get_appdata(app, sizeof(app)) != 0) { strcpy(app, "."); }
        snprintf(defDir, sizeof(defDir), "%s\\FSAL", app);
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
    char dstFsal[512]; pw_join(dir, "fsal.exe", dstFsal, sizeof(dstFsal));
    ui_print_step("Setting up", "fsal.exe");
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "powershell -NoProfile -Command \"Copy-Item -Force -LiteralPath '%s' -Destination '%s'\"", exe, dstFsal);
    if (pw_shell(cmd, NULL, 0) != 0) {
        ui_print_error("Failed to copy fsal.exe");
        return 2;
    }
    char bin[512]; pw_join(dir, ".fsal\\bin", bin, sizeof(bin));
    pw_ensure_dir(bin);
    char err[256];
    if (pw_add_to_user_path(bin, err, sizeof(err)) != 0) {
        ui_print_warning("Failed to add to PATH");
    }
    char shim[512]; pw_join(bin, "fsal.cmd", shim, sizeof(shim));
    char body[1024]; snprintf(body, sizeof(body), "@echo off\r\n\"%s\" %%*\r\n", dstFsal);
    pw_write_text(shim, body);
    return 0;
}

int main(int argc, char** argv){
    if(argc == 1) {
        char exe[512];
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        return installer_main(exe);
    }
    int embedded = check_and_run_embedded(argc, argv);
    if(embedded!=-1) return embedded;
    if(argc<2){ usage(); return 0; }
    char root[512]; if(ensure_runtime_layout(root,sizeof(root))!=0){ ui_print_error("init failed"); return 1; }
    if(strcmp(argv[1],"unpack")==0){ return cmd_unpack(argc, argv); }
    int w = dispatch_welt(argc, argv); if(w!=-1) return w;
    usage(); return 0;
}
