#include "mp.hpp"
#include "environment.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <vector>

namespace retro {
void fmt_log(retro_log_level, fmt::string_view, fmt::format_args) noexcept {
}

bool set_warn_message(const char *) {
    return true;
}
}

namespace {
using MelonDsDs::HeaderSize;
using MelonDsDs::MpState;
using MelonDsDs::Packet;

int failures = 0;
MpState *hostState = nullptr;
MpState *clientState = nullptr;
uint16_t lastHostDestination = RETRO_NETPACKET_BROADCAST;
uint16_t lastClientDestination = RETRO_NETPACKET_BROADCAST;

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        failures++;
    }
}

void NoopPoll() {
}

void HostSend(int, const void *data, size_t length, uint16_t clientId) {
    lastHostDestination = clientId;
    if (clientState != nullptr && data != nullptr && length > 0) {
        clientState->PacketReceived(data, length, 0);
    }
}

void ClientSend(int, const void *data, size_t length, uint16_t clientId) {
    lastClientDestination = clientId;
    if (hostState != nullptr && data != nullptr && length > 0) {
        hostState->PacketReceived(data, length, 1);
    }
}

void CaptureClientSend(int, const void *, size_t, uint16_t clientId) {
    lastClientDestination = clientId;
}

void Configure(MpState &state, retro_netpacket_send_t send) {
    state.SetSendFn(send);
    state.SetPollFn(NoopPoll);
}

void Stop(MpState &state) {
    state.Reset();
}

void RequirePayload(const Packet &packet, const std::vector<uint8_t> &expected, const char *message) {
    const auto *actual = static_cast<const uint8_t *>(packet.Data());
    Require(packet.Length() == expected.size(), message);
    if (packet.Length() == expected.size()) {
        Require(std::equal(expected.begin(), expected.end(), actual), message);
    }
}

void TestCommandReplyExchange() {
    MpState host;
    MpState client;
    hostState = &host;
    clientState = &client;
    Configure(host, HostSend);
    Configure(client, ClientSend);

    const std::vector<uint8_t> commandPayload {0x10, 0x20, 0x30};
    host.SendPacket(Packet(commandPayload.data(), commandPayload.size(), 100, 0, Packet::Type::Cmd));

    Require(lastHostDestination == RETRO_NETPACKET_BROADCAST, "command was not broadcast");
    const std::optional<Packet> command = client.NextPacket();
    Require(command.has_value(), "client did not receive command");
    if (command.has_value()) {
        Require(command->PacketType() == Packet::Type::Cmd, "client received wrong packet type");
        RequirePayload(*command, commandPayload, "client command payload mismatch");
    }

    const std::vector<uint8_t> replyPayload {0xA0, 0xB0};
    client.SendPacket(Packet(replyPayload.data(), replyPayload.size(), 101, 1, Packet::Type::Reply));

    Require(lastClientDestination == 0, "reply was not directed to the command sender");
    const std::optional<Packet> reply = host.NextPacket();
    Require(reply.has_value(), "host did not receive reply");
    if (reply.has_value()) {
        Require(reply->PacketType() == Packet::Type::Reply, "host received wrong packet type");
        RequirePayload(*reply, replyPayload, "host reply payload mismatch");
    }

    Stop(client);
    Stop(host);
    hostState = nullptr;
    clientState = nullptr;
}

void TestSessionRestartClearsPeerRouting() {
    MpState client;
    Configure(client, CaptureClientSend);

    const std::array<uint8_t, 1> oldCommandPayload {0x44};
    const Packet oldCommand(oldCommandPayload.data(), oldCommandPayload.size(), 200, 0, Packet::Type::Cmd);
    const std::vector<uint8_t> encoded = oldCommand.ToBuf();
    client.PacketReceived(encoded.data(), encoded.size(), 7);
    Require(client.NextPacket().has_value(), "old session command was not received");

    Stop(client);
    Configure(client, CaptureClientSend);

    lastClientDestination = RETRO_NETPACKET_BROADCAST;
    const std::array<uint8_t, 1> replyPayload {0x55};
    client.SendPacket(Packet(replyPayload.data(), replyPayload.size(), 201, 1, Packet::Type::Reply));
    Require(lastClientDestination == RETRO_NETPACKET_BROADCAST,
            "new session reply reused the previous session client id");

    Stop(client);
}

void TestSessionRestartDropsQueuedPackets() {
    MpState client;
    Configure(client, CaptureClientSend);

    const std::array<uint8_t, 1> oldPayload {0x66};
    const Packet oldPacket(oldPayload.data(), oldPayload.size(), 300, 0, Packet::Type::Other);
    const std::vector<uint8_t> encoded = oldPacket.ToBuf();
    client.PacketReceived(encoded.data(), encoded.size(), 7);

    Stop(client);
    Configure(client, CaptureClientSend);

    Require(!client.NextPacket().has_value(), "new session consumed a queued packet from the previous session");

    Stop(client);
}
}

int main() {
    TestCommandReplyExchange();
    TestSessionRestartClearsPeerRouting();
    TestSessionRestartDropsQueuedPackets();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
