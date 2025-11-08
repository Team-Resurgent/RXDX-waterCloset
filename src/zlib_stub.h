#pragma once
#include <stdio.h>

#define Z_OK 0

typedef FILE* gzFile;

static inline gzFile gzopen(const char* path, const char* mode) { return fopen(path, mode); }
static inline int    gzread(gzFile f, void* buf, unsigned len) { return (int)fread(buf, 1, len, f); }
static inline int    gzclose(gzFile f) { return fclose(f); }
static inline int    gzeof(gzFile f) { return feof(f); }
static inline const char* gzerror(gzFile f, int* errnum) { if (errnum) *errnum = 0; return "zlib_stub"; }
#pragma once
