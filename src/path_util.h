#pragma once
// path_util.h
#pragma once

#if defined(_XBOX) || defined(XBOX) || defined(_WIN32)
#define DIR_SEP_CHAR '\\'
#else
#define DIR_SEP_CHAR '/'
#endif

static inline void normalize_separators_inplace(char* s) {
    if (!s) return;
#if defined(_XBOX) || defined(XBOX) || defined(_WIN32)
    // On Xbox/Windows convert '/' -> '\'
    for (char* p = s; *p; ++p) if (*p == '/') *p = '\\';
#else
    // On POSIX convert '\' -> '/'
    for (char* p = s; *p; ++p) if (*p == '\\') *p = '/';
#endif
}

// Safe join using the platform separator (optional helper)
static inline void join_path(char* out, size_t outsz, const char* base, const char* leaf) {
    if (!out || outsz == 0) return;
    out[0] = '\0';
    if (base && *base) {
        size_t n = 0;
        while (base[n] && n + 1 < outsz) { out[n] = base[n]; n++; }
        out[n] = '\0';
        if (n && out[n - 1] != '/' && out[n - 1] != '\\') {
            if (n + 1 < outsz) { out[n++] = DIR_SEP_CHAR; out[n] = '\0'; }
        }
    }
    if (leaf && *leaf) {
        while (*leaf == '/' || *leaf == '\\') leaf++;
        size_t n = 0; while (out[n]) n++;
        while (*leaf && n + 1 < outsz) { out[n++] = *leaf++; }
        out[n] = '\0';
    }
    normalize_separators_inplace(out);
}
