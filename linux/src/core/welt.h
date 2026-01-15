#ifndef WELT_H
#define WELT_H

#ifdef __cplusplus
extern "C" {
#endif

int welt_inweld(const char* filePath, int argc, const char** argv);
int welt_cweld(const char* filePath, const char* outExeOpt);
int welt_run_code(const char* name, const char* code, int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif