#pragma once

#include <stddef.h>
#include <stdio.h>

#include "ffb_build_stamp.h"

// Shared firmware identity for base / rim / pack manifest.
// Stamp macros come from ffb_build_stamp.h (pack script) or defaults below.

#ifndef FFB_RELEASE_TAG
#define FFB_RELEASE_TAG "dev"
#endif

#ifndef FFB_BUILD_UTC
// Fallback for plain `pio run` without pack stamp (local compile time).
#define FFB_BUILD_UTC __DATE__ " " __TIME__
#endif

namespace FfbVersion {

static constexpr size_t kIdMax = 48;

// "release build_utc" e.g. "v0.2.0 2026-09-11T10:47:00Z" or "dev Sep 11 2026 12:00:00"
inline void formatId(char* out, size_t n) {
    if (!out || n == 0)
        return;
    snprintf(out, n, "%s %s", FFB_RELEASE_TAG, FFB_BUILD_UTC);
    out[n - 1] = '\0';
}

inline const char* releaseTag() {
    return FFB_RELEASE_TAG;
}
inline const char* buildUtc() {
    return FFB_BUILD_UTC;
}

} // namespace FfbVersion
