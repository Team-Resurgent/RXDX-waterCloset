/*
Copyright (C) 2019 Parallel Realities

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "io.h"
#include "../build_defs.h"
#include "../path_util.h"      /* normalize_separators_inplace, join_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "SDL.h"

/* Use our dirent shim on Xbox/other if needed */
#include "../dirent_compat.h"

/* -------- I/O tracing toggle -------- */
#ifndef IO_TRACE
#define IO_TRACE 0  /* set to 1 to enable */
#endif

#if IO_TRACE
#define IO_LOG(fmt, ...) SDL_Log("[IO] " fmt, __VA_ARGS__)
#else
#define IO_LOG(fmt, ...) do{}while(0)
#endif

/* ------------------------------------------------------------------------- */
/* Safe snprintf wrapper (always NUL-terminates)                             */
static int x_snprintf(char* dst, size_t n, const char* fmt, ...)
{
    if (!dst || n == 0) return 0;
    va_list ap;
    va_start(ap, fmt);
    int r = _vsnprintf(dst, n, fmt, ap);
    va_end(ap);
    dst[n - 1] = '\0';
    return r;
}

/* ------------------------------------------------------------------------- */
/* Existence helpers                                                         */

/* true for files OR directories */
static int pathExistsPortable(const char* p)
{
    if (!p || !*p) return 0;
    struct stat st;
    return _stat(p, &st) == 0;
}

/* file-only (kept for any call sites that need it) */
static int fileExistsPortable(const char* p)
{
    if (!p || !*p) return 0;
    FILE* f = fopen(p, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* optional: expose to other code that expects "file" check with normalization */
int fileExists(const char* filename)
{
    if (!filename) return 0;
    char fixed[MAX_FILENAME_LENGTH];
    x_snprintf(fixed, sizeof(fixed), "%s", filename);
    normalize_separators_inplace(fixed);
    return fileExistsPortable(fixed);
}

static int stringComparator(const void* a, const void* b)
{
    const char* const* s1 = (const char* const*)a;
    const char* const* s2 = (const char* const*)b;
    return strcmp(*s1, *s2);
}

/* ------------------------------------------------------------------------- */
/* Returns a path to use for opening 'filename'. Tries multiple locations.   */
/* Never exit(); logs and returns NULL if not found.                         */
/* ALWAYS return a persistent buffer (never a stack pointer) on success.     */
const char* getFileLocation(const char* filename)
{
    static char resolved[MAX_FILENAME_LENGTH];  /* <- return this on success */
    static char pathA[MAX_FILENAME_LENGTH];
    static char pathB[MAX_FILENAME_LENGTH];
    static char pathC[MAX_FILENAME_LENGTH];
    static char pathD[MAX_FILENAME_LENGTH];

    if (!filename) return NULL;

    IO_LOG("lookup '%s'", filename);

    /* 0) Try as-is (normalized) */
    x_snprintf(resolved, sizeof(resolved), "%s", filename);
    normalize_separators_inplace(resolved);
    IO_LOG("  try as-is          : %s", resolved);
    if (pathExistsPortable(resolved)) {
        IO_LOG("  FOUND as-is");
        return resolved;  /* persistent */
    }

    /* 1) DATA_DIR/filename */
    join_path(pathA, sizeof(pathA), DATA_DIR, filename);
    IO_LOG("  try DATA_DIR       : %s", pathA);
    if (pathExistsPortable(pathA)) {
        IO_LOG("  FOUND DATA_DIR");
        x_snprintf(resolved, sizeof(resolved), "%s", pathA);
        return resolved;
    }

    /* 2) DATA_DIR/data/filename */
    {
        char leaf[MAX_FILENAME_LENGTH];
        x_snprintf(leaf, sizeof(leaf), "data/%s", filename);
        normalize_separators_inplace(leaf);
        join_path(pathB, sizeof(pathB), DATA_DIR, leaf);
        IO_LOG("  try DATA_DIR+data  : %s", pathB);
        if (pathExistsPortable(pathB)) {
            IO_LOG("  FOUND DATA_DIR+data");
            x_snprintf(resolved, sizeof(resolved), "%s", pathB);
            return resolved;
        }
    }

    /* 3) Strip leading data/ and try again under DATA_DIR */
    {
        const char* f = filename;
        if (!strncmp(f, "data/", 5) || !strncmp(f, "data\\", 5)) f += 5;
        join_path(pathC, sizeof(pathC), DATA_DIR, f);
        IO_LOG("  try DATA_DIR+strip : %s", pathC);
        if (pathExistsPortable(pathC)) {
            IO_LOG("  FOUND DATA_DIR+strip");
            x_snprintf(resolved, sizeof(resolved), "%s", pathC);
            return resolved;
        }
    }

    /* 4) Last shot: DATA_DIR + filename with normalized separators */
    x_snprintf(pathD, sizeof(pathD), "%s/%s", DATA_DIR, filename);
    normalize_separators_inplace(pathD);
    IO_LOG("  try DATA_DIR+norm  : %s", pathD);
    if (pathExistsPortable(pathD)) {
        IO_LOG("  FOUND DATA_DIR+norm");
        x_snprintf(resolved, sizeof(resolved), "%s", pathD);
        return resolved;
    }

    /* Not found — log and return NULL */
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
        "[IO] Asset NOT FOUND for '%s'. Tried:\n"
        "  '%s'\n  '%s'\n  '%s'\n  '%s'",
        filename, resolved, pathA, pathB, pathC);
    return NULL;
}

/* ------------------------------------------------------------------------- */
char* readFile(const char* filename)
{
    if (!filename) return NULL;

    const char* path = getFileLocation(filename);
    if (!path) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "[IO] readFile: could not resolve '%s'", filename);
        return NULL;
    }

    IO_LOG("read '%s' -> '%s'", filename, path);

    char* buffer = NULL;
    unsigned long length = 0;
    FILE* file = fopen(path, "rb");

    if (!file) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "[IO] fopen failed for '%s'", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    length = (unsigned long)ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fclose(file);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[IO] malloc(%lu) failed for '%s'", length + 1, path);
        return NULL;
    }

    size_t rd = (length > 0) ? fread(buffer, 1, length, file) : 0;
    buffer[length] = '\0';
    fclose(file);

    IO_LOG("read ok: %lu bytes (%zu actually read)", length, rd);
    return buffer;
}

/* ------------------------------------------------------------------------- */
int writeFile(const char* filename, const char* data)
{
    if (!filename) return 0;

    /* Normalize a local copy for separators */
    char fixed[MAX_FILENAME_LENGTH];
    x_snprintf(fixed, sizeof(fixed), "%s", filename);
    normalize_separators_inplace(fixed);

    FILE* file = fopen(fixed, "wb");
    if (file)
    {
        fprintf(file, "%s\n", data ? data : "");
        fclose(file);
        return 1;
    }
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
        "[IO] writeFile: fopen failed for '%s'", fixed);
    return 0;
}

/* ------------------------------------------------------------------------- */
char** getFileList(const char* dir, int* count)
{
    DIR* d;
    int i = 0;
    struct dirent* ent;
    char** filenames = NULL;

    if (count) *count = 0;
    if (!dir) return NULL;

    /* Resolve directory path via getFileLocation (handles DATA_DIR, etc.) */
    const char* dirPath = getFileLocation(dir);
    if (!dirPath) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "[IO] getFileList: could not resolve dir '%s'", dir);
        return NULL;
    }

    d = opendir(dirPath);
    if (!d) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "[IO] opendir failed for '%s'", dirPath);
        return NULL;
    }

    /* First pass: count entries (skip hidden dot entries) */
    while ((ent = readdir(d)) != NULL)
    {
        if (ent->d_name[0] != '.') i++;
    }

    if (i > 0)
    {
        filenames = (char**)malloc(sizeof(char*) * i);
        if (!filenames) {
            closedir(d);
            return NULL;
        }
        memset(filenames, 0, sizeof(char*) * i);

        /* Rewind and collect names */
        rewinddir(d);

        i = 0;
        while ((ent = readdir(d)) != NULL)
        {
            if (ent->d_name[0] != '.')
            {
                filenames[i] = (char*)malloc(MAX_FILENAME_LENGTH);
                if (!filenames[i]) {
                    for (int k = 0; k < i; k++) free(filenames[k]);
                    free(filenames);
                    closedir(d);
                    return NULL;
                }
                memset(filenames[i], 0, MAX_FILENAME_LENGTH);
                STRNCPY(filenames[i], ent->d_name, MAX_FILENAME_LENGTH);
                i++;
            }
        }
    }

    closedir(d);

    if (count) *count = i;

    if (filenames && i > 1)
        qsort(filenames, i, sizeof(char*), stringComparator);

    return filenames;
}
