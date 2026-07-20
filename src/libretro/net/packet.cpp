/*
    Copyright 2024 Bernardo Gomes Negri

    melonDS DS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS DS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS DS. If not, see http://www.gnu.org/licenses/.
*/
#include "mp.hpp"

#include <cstring>
#include <retro_endianness.h>

using namespace MelonDsDs;

namespace {
uint64_t SwapToNetwork(uint64_t value) noexcept {
    return swap_if_little64(value);
}
}

std::optional<Packet> MelonDsDs::ParsePacket(const void *buf, size_t len) {
    if (buf == nullptr || len < HeaderSize || len - HeaderSize > MaxPacketPayloadSize) {
        return std::nullopt;
    }

    const auto *bytes = static_cast<const uint8_t *>(buf);
    uint64_t netTimestamp = 0;
    std::memcpy(&netTimestamp, bytes, sizeof(netTimestamp));

    Packet::Type packetType;
    switch (bytes[9]) {
        case 0:
            packetType = Packet::Type::Other;
            break;
        case 1:
            if (bytes[8] == 0 || bytes[8] >= 16) {
                return std::nullopt;
            }
            packetType = Packet::Type::Reply;
            break;
        case 2:
            packetType = Packet::Type::Cmd;
            break;
        default:
            return std::nullopt;
    }

    return Packet(
        bytes + HeaderSize,
        len - HeaderSize,
        SwapToNetwork(netTimestamp),
        bytes[8],
        packetType);
}

Packet::Packet(const void *data, size_t len, uint64_t timestamp, uint8_t aid, Packet::Type type) :
    _timestamp(timestamp),
    _aid(aid),
    _type(type),
    _data(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + len) {
}

std::vector<uint8_t> Packet::ToBuf() const {
    std::vector<uint8_t> ret;
    ret.reserve(HeaderSize + Length());

    uint64_t netTimestamp = SwapToNetwork(_timestamp);
    const auto *timestampBytes = reinterpret_cast<const uint8_t *>(&netTimestamp);
    ret.insert(ret.end(), timestampBytes, timestampBytes + sizeof(netTimestamp));
    ret.push_back(_aid);

    switch (_type) {
        case Other:
            ret.push_back(0);
            break;
        case Reply:
            ret.push_back(1);
            break;
        case Cmd:
            ret.push_back(2);
            break;
    }
    ret.insert(ret.end(), _data.begin(), _data.end());
    return ret;
}
