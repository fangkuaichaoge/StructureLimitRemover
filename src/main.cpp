#include <array>
#include <cstdint>
#include <cstring>

#include "pl/Gloss.h"
#include "pl/Mod.hpp"
#include "pl/memory/Patch.hpp"
#include "pl/memory/Signature.hpp"

/*
 * StructureLimitRemover - remove the 64x64x64 structure block size limit.
 *
 * Target: libminecraftpe.so (26.40 / 2.64.0), imagebase 0.
 *
 * The size limit is enforced by the tiny shared helper sub_10056C48, which is
 * called by every structure save / import / command path:
 *
 *   - structure block UI "save" (export) handler   sub_E778744
 *   - /structure command save                      sub_E42E360
 *   - script structure manager create/place        sub_E77BA9C
 *   - server-side StructureBlockActor validation   sub_10029260 (via sub_C1C4220)
 *
 * sub_10056C48 (6 instructions):
 *
 *   10056C48  SXTH            W8, W1
 *   10056C4C  MOV             W9, #0x40          ; maxX = 64
 *   10056C50  MOV             W1, #0x40          ; maxZ = 64
 *   10056C54  SUB             W8, W8, W0,SXTH    ; maxY = a2 - a1 (normally 64)
 *   10056C58  ORR             X0, X9, X8,LSL#32
 *   10056C5C  RET
 *
 * It returns (maxY << 32) | maxX in X0 and maxZ in W1.  We patch the three
 * constants so it returns (999, 999, 999):
 *
 *   MOV W9, #0x40          -> MOV W9, #0x3E7   (999)
 *   MOV W1, #0x40          -> MOV W1, #0x3E7   (999)
 *   SUB W8, W8, W0,SXTH    -> MOV W8, #0x3E7   (999)
 *
 * Signatures use "??" wildcards for the register-encoding bytes (low byte of
 * each instruction), which is the usual Android-modding style and tolerates
 * minor register allocation differences between builds.  The patched words
 * themselves are still verified against the exact original bytes of THIS
 * version before writing, so a mismatched build fails safely.
 *
 * Second site: sub_100572C8 (StructureEditorData::setSize / block actor size
 * setter), used by the CLIENT structure-block screen when the size fields are
 * applied (callers sub_82496F4 / sub_8249D9C / sub_8250B2C) and by the server
 * block actor (sub_C1C1D3C).  It clamps sizeX and sizeZ to [1, 64] (Y is not
 * clamped, which is why a 258-tall structure showed as "64 258 64"):
 *
 *   100572D4  MOV  W12, #0x40      ; clamp value 64
 *   100572F8  CMP  W9,  #0x40      ; sizeX > 64 ?
 *   10057308  CMP  W13, #0x40      ; sizeZ > 64 ?
 *
 * We raise all three to 999 so the editor accepts sizes up to 999.
 */

namespace {

struct PatchSite {
    const char *name;
    const char *signature;
    size_t patchOffset; /* byte offset of the 4-byte patch inside the match */
    std::array<uint8_t, 4> original;
    std::array<uint8_t, 4> patch;
};

const PatchSite kPatchSites[] = {
    /* maxX: MOV W9, #0x40 -> MOV W9, #0x3E7 */
    {"structlimit_maxx",
     "?? 3C 00 13 ?? 08 80 52 ?? 08 80 52 ?? A1 20 4B ?? 81 08 AA C0 03 5F D6",
     4,
     {0x09, 0x08, 0x80, 0x52},
     {0xE9, 0x7C, 0x80, 0x52}},

    /* maxZ: MOV W1, #0x40 -> MOV W1, #0x3E7 */
    {"structlimit_maxz",
     "?? 3C 00 13 ?? 08 80 52 ?? 08 80 52 ?? A1 20 4B ?? 81 08 AA C0 03 5F D6",
     8,
     {0x01, 0x08, 0x80, 0x52},
     {0xE1, 0x7C, 0x80, 0x52}},

    /* maxY: SUB W8, W8, W0,SXTH -> MOV W8, #0x3E7 */
    {"structlimit_maxy",
     "?? 3C 00 13 ?? 08 80 52 ?? 08 80 52 ?? A1 20 4B ?? 81 08 AA C0 03 5F D6",
     12,
     {0x08, 0xA1, 0x20, 0x4B},
     {0xE8, 0x7C, 0x80, 0x52}},

    /* editor size clamp constant: MOV W12, #0x40 -> MOV W12, #0x3E7 */
    {"structlimit_clamp_const",
     "?? 3C 00 13 ?? 08 80 52 ?? 88 40 B9 ?? 10 20 1E "
     "?? 01 08 0B ?? ?? 50 29 ?? 01 08 4B ?? A1 22 4B "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A",
     4,
     {0x0C, 0x08, 0x80, 0x52},
     {0xEC, 0x7C, 0x80, 0x52}},

    /* editor size clamp X: CMP W9, #0x40 -> CMP W9, #0x3E7 */
    {"structlimit_clamp_cmp_x",
     "?? 3C 00 13 ?? 08 80 52 ?? 88 40 B9 ?? 10 20 1E "
     "?? 01 08 0B ?? ?? 50 29 ?? 01 08 4B ?? A1 22 4B "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A",
     40,
     {0x3F, 0x01, 0x01, 0x71},
     {0x3F, 0x9D, 0x0F, 0x71}},

    /* editor size clamp Z: CMP W13, #0x40 -> CMP W13, #0x3E7 */
    {"structlimit_clamp_cmp_z",
     "?? 3C 00 13 ?? 08 80 52 ?? 88 40 B9 ?? 10 20 1E "
     "?? 01 08 0B ?? ?? 50 29 ?? 01 08 4B ?? A1 22 4B "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A "
     "?? 01 08 6B ?? B1 88 1A ?? 01 01 71 ?? B1 8C 1A",
     56,
     {0xBF, 0x01, 0x01, 0x71},
     {0xBF, 0x9D, 0x0F, 0x71}},
};

} // namespace

class StructureLimitRemoverMod {
public:
    bool load() {
        GlossInit(true);
        return patchStructureSizeSites();
    }

private:
    bool patchStructureSizeSites() {
        bool allOk = true;

        for (const auto &site : kPatchSites) {
            uintptr_t addr = pl::memory::resolveSignature(site.signature,
                                                          "libminecraftpe.so");
            if (addr == 0) {
                allOk = false;
                continue;
            }

            const uintptr_t patchAddr = addr + site.patchOffset;
            const auto current = pl::memory::readBytes(patchAddr, 4);
            if (current.size() != 4) {
                allOk = false;
                continue;
            }

            if (std::memcmp(current.data(), site.patch.data(), 4) == 0) {
                continue;
            }
            if (std::memcmp(current.data(), site.original.data(), 4) != 0) {
                allOk = false;
                continue;
            }

            if (!pl::memory::writeBytes(patchAddr, site.patch, site.name)) {
                allOk = false;
                continue;
            }
        }
        return allOk;
    }
};

static StructureLimitRemoverMod mod;
PL_REGISTER_MOD(StructureLimitRemoverMod, mod)
