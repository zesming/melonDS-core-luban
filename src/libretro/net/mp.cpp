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
#include "environment.hpp"
#include <ctime>
#include <libretro.h>
#include <new>
#include <retro_assert.h>
using namespace MelonDsDs;

// How many successive timeouts before
// the player gets notified they are not supposed to use a VPN.
constexpr int SUCCESSIVE_TIMEOUTS_WARNING = 6;
constexpr long RECV_TIMEOUT_MS = 25;

bool MpState::IsReady() const noexcept {
    return _sendFn != nullptr && _pollFn != nullptr;
}

void MpState::SetSendFn(retro_netpacket_send_t sendFn) noexcept {
    if (sendFn != nullptr) {
        retro::set_warn_message("LAN Multiplayer will NOT work using VPNs or tunnels such as Hamachi!");
    }
    _sendFn = sendFn;
}

void MpState::SetPollFn(retro_netpacket_poll_receive_t pollFn) noexcept {
    _pollFn = pollFn;
}

void MpState::Reset() noexcept {
    _sendFn = nullptr;
    _pollFn = nullptr;
    _hostId.reset();
    _warnedHighLatency = false;
    _timeoutCount = 0;
    while (!receivedPackets.empty()) {
        receivedPackets.pop();
    }
}

void MpState::PacketReceived(const void *buf, size_t len, uint16_t client_id) noexcept {
    retro_assert(IsReady());
    try {
        std::optional<Packet> packet = ParsePacket(buf, len);
        if (!packet.has_value()) {
            return;
        }
        if(packet->PacketType() == Packet::Type::Cmd) {
            _hostId = client_id;
            //retro::debug("Host client id is {}", client_id);
        }
        receivedPackets.push(std::move(packet).value());
    } catch (const std::bad_alloc &) {
        return;
    }
}

std::optional<Packet> MpState::NextPacket() noexcept {
    retro_assert(IsReady());
    if(receivedPackets.empty()) {
        _sendFn(RETRO_NETPACKET_FLUSH_HINT, NULL, 0, RETRO_NETPACKET_BROADCAST);
        _pollFn();
    }
    if(receivedPackets.empty()) {
        return std::nullopt;
    } else {
        _timeoutCount = 0;
        Packet p = receivedPackets.front();
        receivedPackets.pop();
        return p;
    }
}

std::optional<Packet> MpState::NextPacketBlock() noexcept {
    retro_assert(IsReady());
    if (receivedPackets.empty()) {
        for(std::clock_t start = std::clock(); std::clock() < (start + (RECV_TIMEOUT_MS * CLOCKS_PER_SEC / 1000));) {
            _sendFn(RETRO_NETPACKET_FLUSH_HINT, NULL, 0, RETRO_NETPACKET_BROADCAST);
            _pollFn();
            if(!receivedPackets.empty()) {
                return NextPacket();
            }
        }
    } else {
        return NextPacket();
    }
    _timeoutCount++;
    if (_timeoutCount >= SUCCESSIVE_TIMEOUTS_WARNING && !_warnedHighLatency) {
        retro::set_warn_message("LAN Multiplayer will NOT work using VPNs or tunnels such as Hamachi!");
        _warnedHighLatency = true;
    }
    retro::debug("Timeout while waiting for packet");
    return std::nullopt;
}

void MpState::SendPacket(const Packet &p) noexcept {
    retro_assert(IsReady());
    uint16_t dest = RETRO_NETPACKET_BROADCAST;
    if(p.PacketType() == Packet::Type::Cmd) {
        _hostId = std::nullopt;
    }
    if(p.PacketType() == Packet::Type::Reply && _hostId.has_value()) {
        dest = _hostId.value();
    }
    _sendFn(RETRO_NETPACKET_UNSEQUENCED | RETRO_NETPACKET_UNRELIABLE | RETRO_NETPACKET_FLUSH_HINT, p.ToBuf().data(), p.Length() + HeaderSize, dest);
}


