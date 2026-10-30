#include "AtomicFile.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOUSER
#include <windows.h>

bool AtomicReplaceFile(const char* temporaryPath, const char* destinationPath) {
    return MoveFileExA(temporaryPath, destinationPath,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
#else
#include <cstdio>

bool AtomicReplaceFile(const char* temporaryPath, const char* destinationPath) {
    return std::rename(temporaryPath, destinationPath) == 0;
}
#endif
