// dirent_compat.h — minimal dirent for Windows/OG Xbox (C linkage)
#pragma once

#if defined(_WIN32) || defined(_XBOX) || defined(XBOX)

#if defined(_XBOX) || defined(XBOX)
#include <xtl.h>
#else
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>

#ifndef NAME_MAX
#define NAME_MAX 260
#endif

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct dirent {
        char  d_name[NAME_MAX];
        DWORD d_type;             // FILE_ATTRIBUTE_* flags
    } dirent;

    typedef struct DIR {
        HANDLE           hFind;
        WIN32_FIND_DATAA ffd;
        char             pattern[NAME_MAX]; // "path\*"
        int              first_read;
    } DIR;

    static void _dir_make_pattern(char* out, const char* path) {
        size_t n = strlen(path);
        strncpy(out, path, NAME_MAX - 3);
        out[NAME_MAX - 3] = '\0';
        if (n > 0) {
            char c = out[strlen(out) - 1];
            if (c != '\\' && c != '/') strncat(out, "\\", NAME_MAX - strlen(out) - 1);
        }
        strncat(out, "*", NAME_MAX - strlen(out) - 1);
    }

    static DIR* opendir(const char* path) {
        DIR* d = (DIR*)malloc(sizeof(DIR));
        if (!d) return NULL;
        memset(d, 0, sizeof(*d));

        _dir_make_pattern(d->pattern, path);       // builds "path\*"
        d->hFind = FindFirstFileA(d->pattern, &d->ffd);
        if (d->hFind == INVALID_HANDLE_VALUE) {
            DWORD e = GetLastError();
            SDL_Log("opendir: FindFirstFileA failed for '%s' (pattern '%s') GLE=%lu",
                path, d->pattern, (unsigned long)e);
            free(d);
            return NULL;
        }
        d->first_read = 1;
        return d;
    }


    static struct dirent* readdir(DIR* d) {
        static struct dirent de; // not thread-safe
        if (!d) return NULL;

        if (d->first_read) {
            d->first_read = 0;
        }
        else {
            if (!FindNextFileA(d->hFind, &d->ffd)) return NULL;
        }

        if (strcmp(d->ffd.cFileName, ".") == 0 || strcmp(d->ffd.cFileName, "..") == 0)
            return readdir(d);

        strncpy(de.d_name, d->ffd.cFileName, NAME_MAX - 1);
        de.d_name[NAME_MAX - 1] = '\0';
        de.d_type = d->ffd.dwFileAttributes;
        return &de;
    }

    static void rewinddir(DIR* d) {
        if (!d) return;
        if (d->hFind && d->hFind != INVALID_HANDLE_VALUE) {
            FindClose(d->hFind);
        }
        d->hFind = FindFirstFileA(d->pattern, &d->ffd);
        d->first_read = (d->hFind != INVALID_HANDLE_VALUE) ? 1 : 0;
    }

    static int closedir(DIR* d) {
        if (!d) return -1;
        if (d->hFind && d->hFind != INVALID_HANDLE_VALUE) FindClose(d->hFind);
        free(d);
        return 0;
    }

#ifdef __cplusplus
}
#endif

#else
#include <dirent.h>
#endif
