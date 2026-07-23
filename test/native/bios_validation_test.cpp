#include <array>
#include <cassert>

#include "bios_validation.hpp"

int main() {
    std::array<uint8_t, melonDS::ARM7BIOSSize> wrongArm7 {};
    std::array<uint8_t, melonDS::ARM9BIOSSize> wrongArm9 {};

    // Same-size garbage must not be reported as a native BIOS pair.
    assert(!MelonDsDs::IsKnownNativeNdsBios(MelonDsDs::BiosType::Arm7, wrongArm7));
    assert(!MelonDsDs::IsKnownNativeNdsBios(MelonDsDs::BiosType::Arm9, wrongArm9));
    return 0;
}
