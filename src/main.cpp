// StructureLimitRemover - standalone native patch for Minecraft Bedrock
// (libminecraftpe.so 26.40 / 2.64.0, arm64-v8a).
//
// No preloader / LeviLauncher SDK dependency: this library does its own
// signature scanning (reads /proc/self/maps), verifies the original bytes and
// patches the executable memory with mprotect + memcpy.  The patch runs from
// an ELF .init_array constructor, so it executes as soon as the .so is loaded
// (dlopen by LeviLauncher, LD_PRELOAD, JNI, ...).

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

namespace {

constexpr uint8_t kLimit = 0x3E7; /* 999 */

struct PatchSite {
    const char *name;
    const char *signature; /* space-separated hex, "??" wildcards */
    size_t patchOffset;    /* byte offset of the 4-byte patch inside the match */
    uint8_t original[4];
    uint8_t patch[4];
};

/*
 * Site 1: sub_10056C48 - shared "max structure size" helper used by the
 * structure block UI save, /structure command, script structure manager and
 * server-side validation.  maxX/maxY/maxZ are all raised from 64 to 999.
 *
 * Site 2: sub_100572C8 - StructureEditorData::setSize clamps sizeX/sizeZ to
 * [1, 64] on the client and server.  The clamp value and both comparisons are
 * raised to 999.
 */
const PatchSite kPatchSites[] = {
    {"maxx",
     "?? 3C 00 13 ?? 08 80 52 ?? 08 80 52 ?? A1 20 4B ?? 81 08 AA C0 03 5F D6",
     4,
     {0x09, 0x08, 0x80, 0x52},
     {0xE9, 0x7C, 0x80, 0x52}},

    {"maxz",
     "?? 3C 00 13 ?? 08 80 52 ?? 08 80 52 ?? A1 20 4B ?? 81 08 AA C0 03 5F D6",
     8,
     {0x01, 0x08, 0x80, 0x52},
     {0xE1, 0x7C, 0x80, 0x52}},

    {"maxy",
     "?? 3C 00 13 ?? 08 80 52 ?? 08 80 52 ?? A1 20 4B ?? 81 08 AA C0 03 5F D6",
     12,
     {0x08, 0xA1, 0x20, 0x4B},
     {0xE8, 0x7C, 0x80, 0x52}},

    {"clamp_const",
     "?? 3C 00 13 ?? 08 80 52 ?? 88 40 B9 ?? 10 20 1E "
     "?? 01 08 0B ?? ?? 50 29 ?? 01 08 4B ?? A1 22 4B "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A",
     4,
     {0x0C, 0x08, 0x80, 0x52},
     {0xEC, 0x7C, 0x80, 0x52}},

    {"clamp_cmp_x",
     "?? 3C 00 13 ?? 08 80 52 ?? 88 40 B9 ?? 10 20 1E "
     "?? 01 08 0B ?? ?? 50 29 ?? 01 08 4B ?? A1 22 4B "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A",
     40,
     {0x3F, 0x01, 0x01, 0x71},
     {0x3F, 0x9D, 0x0F, 0x71}},

    {"clamp_cmp_z",
     "?? 3C 00 13 ?? 08 80 52 ?? 88 40 B9 ?? 10 20 1E "
     "?? 01 08 0B ?? ?? 50 29 ?? 01 08 4B ?? A1 22 4B "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A",
     56,
     {0xBF, 0x01, 0x01, 0x71},
     {0xBF, 0x9D, 0x0F, 0x71}},
};

struct PatternByte {
    uint8_t value;
    uint8_t mask;
};

struct Region {
    uintptr_t start;
    uintptr_t end;
};

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool parsePattern(const char *signature, PatternByte *out, size_t maxLen,
                  size_t *outLen) {
    size_t n = 0;
    const char *p = signature;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;

        const char *start = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        const size_t tokLen = static_cast<size_t>(p - start);
        if (n >= maxLen) return false;

        PatternByte &byte = out[n];
        if ((tokLen == 1 && start[0] == '?') ||
            (tokLen == 2 && start[0] == '?' && start[1] == '?')) {
            byte = {0, 0};
        } else if (tokLen == 2) {
            uint8_t value = 0;
            uint8_t mask = 0;
            for (size_t i = 0; i < 2; ++i) {
                const int shift = static_cast<int>((1 - i) * 4);
                const char ch = start[i];
                if (ch == '?') continue;
                const int digit = hexValue(ch);
                if (digit < 0) return false;
                value |= static_cast<uint8_t>(digit << shift);
                mask |= static_cast<uint8_t>(0xF << shift);
            }
            byte = {value, mask};
        } else {
            return false;
        }
        ++n;
    }
    *outLen = n;
    return n > 0;
}

bool collectRegions(const char *moduleName, Region *regions, size_t maxRegions,
                    size_t *regionCount) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return false;

    size_t count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, moduleName)) continue;
        unsigned long start = 0;
        unsigned long end = 0;
        char perms[5] = {0};
        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) != 3) continue;
        if (end <= start || perms[0] != 'r') continue;
        if (count >= maxRegions) break;
        regions[count++] = {static_cast<uintptr_t>(start),
                            static_cast<uintptr_t>(end)};
    }
    fclose(fp);
    *regionCount = count;
    return count > 0;
}

bool matchAt(const uint8_t *data, size_t dataLen, const PatternByte *pattern,
             size_t patternLen) {
    if (dataLen < patternLen) return false;
    for (size_t i = 0; i < patternLen; ++i) {
        if ((data[i] & pattern[i].mask) != pattern[i].value) return false;
    }
    return true;
}

bool findPattern(const Region &region, const PatternByte *pattern,
                 size_t patternLen, uintptr_t *out) {
    const uint8_t *base = reinterpret_cast<const uint8_t *>(region.start);
    const size_t regionLen = region.end - region.start;
    if (regionLen < patternLen) return false;

    size_t anchor = 0;
    while (anchor < patternLen && pattern[anchor].mask != 0xFF) ++anchor;
    if (anchor == patternLen) anchor = 0;
    const uint8_t anchorByte = pattern[anchor].value;

    for (size_t i = 0; i + patternLen <= regionLen; ++i) {
        if (base[i + anchor] != anchorByte) continue;
        if (matchAt(base + i, regionLen - i, pattern, patternLen)) {
            *out = region.start + i;
            return true;
        }
    }
    return false;
}

bool writePatch(uintptr_t address, const uint8_t data[4]) {
    const long pageSize = sysconf(_SC_PAGESIZE);
    const uintptr_t pageMask = static_cast<uintptr_t>(pageSize) - 1;
    const uintptr_t pageStart = address & ~pageMask;
    const size_t span =
        ((address + 4 + pageMask) & ~pageMask) - pageStart;

    if (mprotect(reinterpret_cast<void *>(pageStart), span,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return false;
    }
    memcpy(reinterpret_cast<void *>(address), data, 4);
    __builtin___clear_cache(reinterpret_cast<char *>(address),
                            reinterpret_cast<char *>(address + 4));
    mprotect(reinterpret_cast<void *>(pageStart), span,
             PROT_READ | PROT_EXEC);
    return true;
}

void applyPatches() {
    Region regions[32];
    size_t regionCount = 0;
    if (!collectRegions("libminecraftpe.so", regions, 32, &regionCount)) {
        return;
    }

    for (const auto &site : kPatchSites) {
        PatternByte pattern[64];
        size_t patternLen = 0;
        if (!parsePattern(site.signature, pattern, 64, &patternLen)) {
            continue;
        }

        uintptr_t found = 0;
        for (size_t i = 0; i < regionCount; ++i) {
            if (findPattern(regions[i], pattern, patternLen, &found)) break;
        }
        if (found == 0) continue;

        const uintptr_t patchAddr = found + site.patchOffset;
        if (memcmp(reinterpret_cast<void *>(patchAddr), site.patch, 4) == 0) {
            continue; /* already patched */
        }
        if (memcmp(reinterpret_cast<void *>(patchAddr), site.original, 4) !=
            0) {
            continue; /* version mismatch, skip safely */
        }
        writePatch(patchAddr, site.patch);
    }
}

} // namespace

__attribute__((constructor)) static void StructureLimitRemoverInit() {
    applyPatches();
}
