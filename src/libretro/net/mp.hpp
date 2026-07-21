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
#pragma once
#include <cstdint>
#include <queue>
#include <optional>
#include <vector>
#include <libretro.h>

namespace MelonDsDs {
// timestamp, aid, and isReply, respectively.
constexpr size_t HeaderSize = sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint8_t);
constexpr size_t MaxPacketPayloadSize = 2048;

class Packet {
public:
    enum Type {
        Reply, Cmd, Other
    };

    explicit Packet(const void *data, size_t len, uint64_t timestamp, uint8_t aid, Packet::Type type);

    [[nodiscard]] uint64_t Timestamp() const noexcept {
        return _timestamp;
    };
    [[nodiscard]] uint8_t Aid() const noexcept {
        return _aid;
    };
    [[nodiscard]] Packet::Type PacketType() const noexcept {
        return _type;
    }
    [[nodiscard]] const void *Data() const noexcept {
        return _data.data();
    };
    [[nodiscard]] size_t Length() const noexcept {
        return _data.size();
    };

    std::vector<uint8_t> ToBuf() const;
private:
    uint64_t _timestamp;
    uint8_t _aid;
    Packet::Type _type;
    std::vector<uint8_t> _data;
};

std::optional<Packet> ParsePacket(const void *buf, size_t len);

class MpState {
public:
    void PacketReceived(const void *buf, size_t len, uint16_t client_id) noexcept;
    void SetSendFn(retro_netpacket_send_t sendFn) noexcept;
    void SetPollFn(retro_netpacket_poll_receive_t pollFn) noexcept;
    void Reset() noexcept;
    bool IsReady() const noexcept;
    void SendPacket(const Packet &p) noexcept;
    std::optional<Packet> NextPacket() noexcept;
    std::optional<Packet> NextPacketBlock() noexcept;
private:
    bool _warnedHighLatency = false;
    int _timeoutCount = 0;
    retro_netpacket_send_t _sendFn = nullptr;
    retro_netpacket_poll_receive_t _pollFn = nullptr;
    std::optional<uint16_t> _hostId;
    std::queue<Packet> receivedPackets;
};
}
