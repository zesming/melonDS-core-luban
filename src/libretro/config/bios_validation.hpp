/*
    Copyright 2026 LUBAN contributors
*/

#ifndef MELONDSDS_CONFIG_BIOS_VALIDATION_HPP
#define MELONDSDS_CONFIG_BIOS_VALIDATION_HPP

#include <CRC32.h>
#include <MemConstants.h>

#include "../std/span.hpp"
#include "types.hpp"

namespace MelonDsDs {
    inline bool IsKnownNativeNdsBios(BiosType type, std::span<const uint8_t> buffer) noexcept {
        uint32_t expectedCrc = 0;
        switch (type) {
            case BiosType::Arm7:
                expectedCrc = melonDS::ARM7BIOSCRC32;
                break;
            case BiosType::Arm9:
                expectedCrc = melonDS::ARM9BIOSCRC32;
                break;
            case BiosType::Arm7i:
            case BiosType::Arm9i:
                // DSi is not a supported product mode; keep its existing loader semantics.
                return true;
        }
        return melonDS::CRC32(buffer.data(), static_cast<int>(buffer.size())) == expectedCrc;
    }
}

#endif // MELONDSDS_CONFIG_BIOS_VALIDATION_HPP
