/*
Copyright (C) 2015-2019 Parallel Realities

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/*
 * Cross-platform createSaveFolder for OG Xbox + Windows
 * Keeps original logging and app.saveDir behavior.
 */

#include "win32Init.h"          /* brings in MAX_FILENAME_LENGTH, STRNCPY, app, etc. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "SDL.h"

#include <stdarg.h>

 /* Safe snprintf replacement that works on OG Xbox / MSVC CRTs */
static int x_snprintf(char* dst, size_t n, const char* fmt, ...)
{
    if (!dst || n == 0) return 0;
    va_list ap;
    va_start(ap, fmt);
#if defined(_XBOX) || defined(XBOX) || defined(_MSC_VER)
    /* _vsnprintf writes at most n bytes and may not NUL-terminate */
    int r = _vsnprintf(dst, n, fmt, ap);
#else
    int r = vsnprintf(dst, n, fmt, ap);
#endif
    va_end(ap);
    /* Ensure NUL-termination */
    dst[n - 1] = '\0';
    return r;
}



#if defined(_XBOX) || defined(XBOX)
 /* -------- OG Xbox (RXDK) -------- */
#include <xtl.h>     /* CreateDirectoryA/GetLastError */
#include <direct.h>  /* _mkdir fallback */


static void ensure_trailing_slash(char* s, size_t cap)
{
    size_t n = strlen(s);
    if (n && s[n - 1] != '\\' && n + 1 < cap) {
        s[n] = '\\';
        s[n + 1] = '\0';
    }
}

static int try_make_dir(const char* path)
{
    if (!CreateDirectoryA(path, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            /* fallback to CRT */
            if (_mkdir(path) != 0 && errno != EEXIST) {
                return 0;
            }
        }
    }
    return 1;
}

static int is_writable_dir(const char* dir)
{
    char probe[MAX_FILENAME_LENGTH];
    x_snprintf(probe, sizeof(probe), "%s%s", dir, "__probe__.tmp");
    FILE* f = fopen(probe, "wb");
    if (!f) return 0;
    fwrite("x", 1, 1, f);
    fclose(f);
    remove(probe);
    return 1;
}

void createSaveFolder(void)
{
    /* Adjust TITLEID/UDATA path to your project if you want persistent saves */
    const char* candidates[] = {
        "T:\\waterCloset",                   /* dev-friendly temp partition */
        "E:\\UDATA\\AAAA0001\\00000001",     /* change to your TITLEID */
        "E:\\WaterCloset",
        NULL
    };

    app.saveDir[0] = '\0';

    for (int i = 0; candidates[i]; ++i) {
        char path[MAX_FILENAME_LENGTH];
        /* single leaf folder -> just create it; parents exist in these cases */
        strncpy(path, candidates[i], sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';

        if (!try_make_dir(path)) {
            SDL_Log("Failed to create save dir '%s' (GLE=%lu), trying next...",
                path, (unsigned long)GetLastError());
            continue;
        }

        ensure_trailing_slash(path, sizeof(path));

        if (!is_writable_dir(path)) {
            SDL_Log("Save dir not writable: '%s', trying next...", path);
            continue;
        }

        STRNCPY(app.saveDir, path, MAX_FILENAME_LENGTH);
        SDL_Log("Save dir = %s", app.saveDir);
        return;
    }

    /* Final fallback: leave empty => use current working dir */
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
        "All save dir candidates failed; will save to current dir.");
    app.saveDir[0] = '\0';
}

#endif /* _XBOX || XBOX */