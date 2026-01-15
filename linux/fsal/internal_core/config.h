#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[128];
    char cmduse[128];
    double version;
} FsalConfig;

typedef struct {
    char name[128];
    char description[256];
    int version;
    char git[256];
} FsalDep;

int parse_fsal_config(const char* text, FsalConfig* outCfg, char* err, int errCap);
int parse_fsal_deps(const char* text, FsalDep* outDeps, int maxDeps, char* err, int errCap);

#ifdef __cplusplus
}
#endif

#endif