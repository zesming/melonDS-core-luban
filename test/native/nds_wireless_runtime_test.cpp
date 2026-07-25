#include "nds_wireless_runtime.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
int failures = 0;

void Require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

int OpenSender(uint32_t address = INADDR_LOOPBACK)
{
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(address);
    if (bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int OpenReceiver(uint16_t port)
{
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

std::vector<uint8_t> ValidMpPacket(size_t length = 10)
{
    std::vector<uint8_t> packet(length, 0);
    packet[9] = 0;
    return packet;
}

uint16_t FindFreeLoopbackPort()
{
    const int fd = OpenSender();
    if (fd < 0) return 0;
    sockaddr_in addr{};
    socklen_t length = sizeof(addr);
    const bool ok = getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &length) == 0;
    close(fd);
    return ok ? ntohs(addr.sin_port) : 0;
}

bool SendTo(int fd, uint16_t port, const std::vector<uint8_t> &packet)
{
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons(port);
    return sendto(fd, packet.data(), packet.size(), 0,
                  reinterpret_cast<const sockaddr *>(&destination), sizeof(destination)) ==
           static_cast<ssize_t>(packet.size());
}

struct ReceiveState {
    size_t calls = 0;
    size_t bytes = 0;
    std::vector<uint8_t> packetIds;
};

struct FakeDatagram {
    sockaddr_in source{};
    std::vector<uint8_t> payload;
};

struct RuntimeTestIo {
    int sendError = 0;
    int receiveError = 0;
    size_t sendCalls = 0;
    size_t receiveCalls = 0;
    size_t enumerateCalls = 0;
    sockaddr_in lastDestination{};
    std::vector<uint32_t> localAddresses;
    std::vector<FakeDatagram> receiveQueue;
    size_t nextDatagram = 0;
};

ssize_t FakeSendTo(int, const void *, size_t length, int, const sockaddr *destination, socklen_t, void *opaque)
{
    auto *io = static_cast<RuntimeTestIo *>(opaque);
    ++io->sendCalls;
    io->lastDestination = *reinterpret_cast<const sockaddr_in *>(destination);
    if (io->sendError != 0) {
        errno = io->sendError;
        return -1;
    }
    return static_cast<ssize_t>(length);
}

ssize_t FakeReceiveFrom(int, void *data, size_t length, int, sockaddr *source, socklen_t *sourceLength, void *opaque)
{
    auto *io = static_cast<RuntimeTestIo *>(opaque);
    ++io->receiveCalls;
    if (io->receiveError != 0 || io->nextDatagram == io->receiveQueue.size()) {
        errno = io->receiveError != 0 ? io->receiveError : EAGAIN;
        return -1;
    }
    const FakeDatagram &datagram = io->receiveQueue[io->nextDatagram++];
    if (datagram.payload.size() > length || !source || !sourceLength || *sourceLength < sizeof(sockaddr_in)) {
        errno = EMSGSIZE;
        return -1;
    }
    memcpy(data, datagram.payload.data(), datagram.payload.size());
    *reinterpret_cast<sockaddr_in *>(source) = datagram.source;
    *sourceLength = sizeof(sockaddr_in);
    return static_cast<ssize_t>(datagram.payload.size());
}

size_t FakeEnumerateLocalIpv4(uint32_t *addresses, size_t capacity, void *opaque)
{
    auto *io = static_cast<RuntimeTestIo *>(opaque);
    ++io->enumerateCalls;
    const size_t count = io->localAddresses.size() < capacity ? io->localAddresses.size() : capacity;
    for (size_t index = 0; index < count; ++index) {
        addresses[index] = io->localAddresses[index];
    }
    return count;
}

void SetRuntimeTestHooks(RuntimeTestIo *io)
{
    LubanNdsWirelessRuntimeTestHooks hooks{};
    hooks.sendTo = FakeSendTo;
    hooks.receiveFrom = FakeReceiveFrom;
    hooks.enumerateLocalIpv4 = FakeEnumerateLocalIpv4;
    hooks.userData = io;
    lubanNdsWirelessSetRuntimeTestHooks(&hooks);
}

void ClearRuntimeTestHooks()
{
    lubanNdsWirelessSetRuntimeTestHooks(nullptr);
}

sockaddr_in Endpoint(uint32_t address, uint16_t port)
{
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = htonl(address);
    endpoint.sin_port = htons(port);
    return endpoint;
}

void Receive(const void *data, size_t length, const char *, void *opaque)
{
    auto *state = static_cast<ReceiveState *>(opaque);
    ++state->calls;
    state->bytes += length;
    if (data && length > 0) {
        state->packetIds.push_back(static_cast<const uint8_t *>(data)[0]);
    }
}

void TestConfiguredUnicastAndEmptyPacketRejection()
{
    const uint16_t port = FindFreeLoopbackPort();
    Require(port != 0, "could not allocate a loopback port");
    if (port == 0) return;
    LubanNdsWirelessConfig config{};
    config.enabled = true;
    config.unicast = true;
    config.port = port;
    std::strcpy(config.peerHost, "127.0.0.1");
    lubanNdsWirelessConfigure(&config);
    Require(lubanNdsWirelessOpenSocket(), "unicast socket did not open");
    lubanNdsWirelessSendPacket(nullptr, 0);
    LubanNdsWirelessRuntimeState state{};
    Require(lubanNdsWirelessGetRuntimeState(&state), "runtime state unavailable");
    Require(state.txPackets == 0, "empty packet was transmitted");
    lubanNdsWirelessStop();
}

void TestLoopbackHostClientExchangeAndRestart()
{
    const uint16_t port = FindFreeLoopbackPort();
    Require(port != 0, "could not allocate a loopback port");
    if (port == 0) return;

    LubanNdsWirelessConfig hostConfig{};
    hostConfig.enabled = true;
    hostConfig.port = port;
    lubanNdsWirelessConfigure(&hostConfig);
    Require(lubanNdsWirelessOpenSocket(), "loopback host socket did not open");

    std::vector<uint8_t> waitingPacket = ValidMpPacket();
    waitingPacket[0] = 0x31;
    lubanNdsWirelessSendPacket(waitingPacket.data(), waitingPacket.size());
    LubanNdsWirelessRuntimeState state{};
    Require(lubanNdsWirelessGetRuntimeState(&state) && state.txPackets == 1,
            "unlocked host did not broadcast while waiting for client registration");

    const std::vector<uint8_t> clientPacket = [] {
        std::vector<uint8_t> packet = ValidMpPacket();
        packet[0] = 0x4c;
        return packet;
    }();
    const std::vector<uint8_t> hostPacket = [] {
        std::vector<uint8_t> packet = ValidMpPacket();
        packet[0] = 0x7e;
        return packet;
    }();
    const pid_t client = fork();
    Require(client >= 0, "could not fork loopback client");
    if (client == 0) {
        LubanNdsWirelessConfig clientConfig{};
        clientConfig.enabled = true;
        clientConfig.unicast = true;
        clientConfig.port = port;
        std::strcpy(clientConfig.peerHost, "127.0.0.1");
        lubanNdsWirelessConfigure(&clientConfig);
        if (!lubanNdsWirelessOpenSocket()) _exit(2);
        ReceiveState received;
        for (size_t attempt = 0; attempt < 2000 && received.calls == 0; ++attempt) {
            lubanNdsWirelessPollPackets(Receive, &received);
            usleep(1000);
        }
        if (received.calls == 1 && received.packetIds[0] == hostPacket[0]) {
            lubanNdsWirelessSendPacket(clientPacket.data(), clientPacket.size());
        }
        lubanNdsWirelessStop();
        _exit(received.calls == 1 && received.packetIds[0] == hostPacket[0] ? 0 : 3);
    }
    if (client < 0) {
        lubanNdsWirelessStop();
        return;
    }

    ReceiveState received;
    for (size_t attempt = 0; attempt < 1000; ++attempt) {
        lubanNdsWirelessPollPackets(Receive, &received);
        lubanNdsWirelessGetRuntimeState(&state);
        if (state.lastPeer[0] != '\0') break;
        usleep(1000);
    }
    Require(std::string(state.lastPeer) == "127.0.0.1",
            "receive-first loopback client did not register with the host");
    Require(received.calls == 0, "client registration was forwarded as an MP payload");

    const int wrongPeer = OpenSender();
    Require(wrongPeer >= 0, "could not open wrong-peer sender");
    if (wrongPeer >= 0) {
        std::vector<uint8_t> wrongPacket = ValidMpPacket();
        wrongPacket[0] = 0x6d;
        Require(SendTo(wrongPeer, port, wrongPacket), "wrong-peer packet was not sent");
        for (size_t attempt = 0; attempt < 10; ++attempt) {
            lubanNdsWirelessPollPackets(Receive, &received);
            usleep(1000);
        }
        Require(received.calls == 0, "locked host accepted a packet from a different endpoint");
        close(wrongPeer);
    }

    lubanNdsWirelessSendPacket(hostPacket.data(), hostPacket.size());
    for (size_t attempt = 0; attempt < 1000 && received.calls == 0; ++attempt) {
        lubanNdsWirelessPollPackets(Receive, &received);
        usleep(1000);
    }
    Require(received.calls == 1 && received.packetIds[0] == clientPacket[0],
            "host did not receive the client payload after receive-first registration");
    int clientStatus = 0;
    Require(waitpid(client, &clientStatus, 0) == client, "could not wait for loopback client");
    Require(WIFEXITED(clientStatus) && WEXITSTATUS(clientStatus) == 0,
            "loopback client did not receive the host payload");

    lubanNdsWirelessStop();
    const int releasedSocket = OpenReceiver(port);
    Require(releasedSocket >= 0, "stop did not release the host UDP port");
    if (releasedSocket >= 0) close(releasedSocket);

    lubanNdsWirelessConfigure(&hostConfig);
    Require(lubanNdsWirelessOpenSocket(), "host socket did not reopen after stop");
    lubanNdsWirelessStop();
}

void TestClientAndHostRestartRecovery()
{
    const uint16_t port = FindFreeLoopbackPort();
    Require(port != 0, "could not allocate a loopback port");
    if (port == 0) return;

    LubanNdsWirelessConfig hostConfig{};
    hostConfig.enabled = true;
    hostConfig.port = port;
    lubanNdsWirelessConfigure(&hostConfig);
    Require(lubanNdsWirelessOpenSocket(), "restart-recovery host socket did not open");

    const std::vector<uint8_t> hostPackets[] = {
        [] { auto packet = ValidMpPacket(); packet[0] = 0x71; return packet; }(),
        [] { auto packet = ValidMpPacket(); packet[0] = 0x72; return packet; }(),
        [] { auto packet = ValidMpPacket(); packet[0] = 0x73; return packet; }(),
    };
    const std::vector<uint8_t> clientPackets[] = {
        [] { auto packet = ValidMpPacket(); packet[0] = 0x41; return packet; }(),
        [] { auto packet = ValidMpPacket(); packet[0] = 0x42; return packet; }(),
        [] { auto packet = ValidMpPacket(); packet[0] = 0x43; return packet; }(),
    };
    int phasePipe[2] = {};
    const int pipeResult = pipe(phasePipe);
    Require(pipeResult == 0, "could not create restart-recovery phase pipe");
    if (pipeResult != 0) {
        lubanNdsWirelessStop();
        return;
    }

    const pid_t client = fork();
    Require(client >= 0, "could not fork restart-recovery client");
    if (client == 0) {
        close(phasePipe[0]);
        LubanNdsWirelessConfig clientConfig{};
        clientConfig.enabled = true;
        clientConfig.unicast = true;
        clientConfig.port = port;
        std::strcpy(clientConfig.peerHost, "127.0.0.1");
        auto exchange = [&](const std::vector<uint8_t> &hostPacket, const std::vector<uint8_t> &clientPacket) {
            ReceiveState received;
            for (size_t attempt = 0; attempt < 2000 && received.calls == 0; ++attempt) {
                lubanNdsWirelessPollPackets(Receive, &received);
                usleep(1000);
            }
            if (received.calls != 1 || received.packetIds[0] != hostPacket[0]) return false;
            lubanNdsWirelessSendPacket(clientPacket.data(), clientPacket.size());
            return true;
        };
        lubanNdsWirelessConfigure(&clientConfig);
        if (!lubanNdsWirelessOpenSocket() || write(phasePipe[1], "A", 1) != 1 ||
            !exchange(hostPackets[0], clientPackets[0])) {
            _exit(2);
        }
        lubanNdsWirelessStop();
        lubanNdsWirelessConfigure(&clientConfig);
        if (!lubanNdsWirelessOpenSocket() || write(phasePipe[1], "B", 1) != 1 ||
            !exchange(hostPackets[1], clientPackets[1]) || !exchange(hostPackets[2], clientPackets[2])) {
            _exit(3);
        }
        lubanNdsWirelessStop();
        close(phasePipe[1]);
        _exit(0);
    }
    if (client < 0) {
        close(phasePipe[0]);
        close(phasePipe[1]);
        lubanNdsWirelessStop();
        return;
    }
    close(phasePipe[1]);

    auto waitForPeer = [&] {
        LubanNdsWirelessRuntimeState state{};
        ReceiveState ignored;
        for (size_t attempt = 0; attempt < 1000; ++attempt) {
            lubanNdsWirelessPollPackets(Receive, &ignored);
            lubanNdsWirelessGetRuntimeState(&state);
            if (state.lastPeer[0] != '\0') return true;
            usleep(1000);
        }
        return false;
    };
    ReceiveState received;
    auto exchange = [&](size_t index) {
        lubanNdsWirelessSendPacket(hostPackets[index].data(), hostPackets[index].size());
        for (size_t attempt = 0; attempt < 1000 && received.calls <= index; ++attempt) {
            lubanNdsWirelessPollPackets(Receive, &received);
            usleep(1000);
        }
        return received.calls == index + 1 && received.packetIds[index] == clientPackets[index][0];
    };

    char phase = '\0';
    Require(read(phasePipe[0], &phase, 1) == 1 && phase == 'A', "initial client did not open");
    Require(waitForPeer(), "host did not receive initial client registration");
    Require(exchange(0), "initial host-first exchange failed");

    phase = '\0';
    Require(read(phasePipe[0], &phase, 1) == 1 && phase == 'B', "restarted client did not open");
    for (size_t attempt = 0; attempt < 500; ++attempt) {
        lubanNdsWirelessPollPackets(Receive, &received);
        usleep(1000);
    }
    const bool clientRestarted = exchange(1);
    Require(clientRestarted, "host did not recover after client-only restart");
    if (!clientRestarted) {
        int clientStatus = 0;
        waitpid(client, &clientStatus, 0);
        close(phasePipe[0]);
        lubanNdsWirelessStop();
        return;
    }

    lubanNdsWirelessStop();
    lubanNdsWirelessConfigure(&hostConfig);
    Require(lubanNdsWirelessOpenSocket(), "host socket did not reopen during client-held restart");
    Require(waitForPeer(), "reopened host did not recover from periodic client registration");
    Require(exchange(2), "host-only restart did not recover the host-first exchange");

    int clientStatus = 0;
    Require(waitpid(client, &clientStatus, 0) == client, "could not wait for restart-recovery client");
    Require(WIFEXITED(clientStatus) && WEXITSTATUS(clientStatus) == 0,
            "restart-recovery client did not receive all host payloads");
    close(phasePipe[0]);
    lubanNdsWirelessStop();
}

void TestHostBroadcastsBeforeLockAndLocksValidPeer()
{
    const uint16_t port = FindFreeLoopbackPort();
    Require(port != 0, "could not allocate a loopback port");
    if (port == 0) return;
    LubanNdsWirelessConfig config{};
    config.enabled = true;
    config.port = port;
    lubanNdsWirelessConfigure(&config);
    Require(lubanNdsWirelessOpenSocket(), "host socket did not open");
    const int invalidSender = OpenSender();
    const int sender = OpenSender();
    Require(invalidSender >= 0 && sender >= 0, "sender did not open");
    if (invalidSender < 0 || sender < 0) return;
    const std::vector<uint8_t> packet = ValidMpPacket();
    lubanNdsWirelessSendPacket(packet.data(), packet.size());
    LubanNdsWirelessRuntimeState state{};
    Require(lubanNdsWirelessGetRuntimeState(&state) && state.txPackets == 1,
            "unlocked host did not broadcast its MP packet");

    Require(SendTo(invalidSender, port, {0x00}), "invalid packet was not sent");
    ReceiveState received;
    lubanNdsWirelessPollPackets(Receive, &received);
    Require(received.calls == 0, "invalid packet locked the host peer");
    Require(SendTo(sender, port, packet), "valid packet was not sent");
    for (size_t attempt = 0; attempt < 10 && received.calls == 0; ++attempt) {
        lubanNdsWirelessPollPackets(Receive, &received);
        usleep(1000);
    }
    Require(received.calls == 1, "valid packet did not lock the host peer");

    lubanNdsWirelessSendPacket(packet.data(), packet.size());
    Require(lubanNdsWirelessGetRuntimeState(&state) && state.txPackets == 2,
            "locked host did not send its MP packet");
    timeval timeout{};
    timeout.tv_sec = 1;
    setsockopt(sender, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    std::vector<uint8_t> receivedPacket(packet.size());
    Require(recv(sender, receivedPacket.data(), receivedPacket.size(), 0) ==
                static_cast<ssize_t>(receivedPacket.size()) && receivedPacket == packet,
            "locked host did not unicast to the first valid peer");
    close(invalidSender);
    close(sender);
    lubanNdsWirelessStop();
}

void TestTransientSocketErrorsPreserveLastErrorUntilSuccess()
{
    const int errors[] = {EAGAIN, EWOULDBLOCK, ENOBUFS, EINTR};
    const std::vector<uint8_t> packet = ValidMpPacket();
    for (int error : errors) {
        const uint16_t port = FindFreeLoopbackPort();
        Require(port != 0, "could not allocate a loopback port");
        if (port == 0) return;
        RuntimeTestIo io;
        io.sendError = error;
        SetRuntimeTestHooks(&io);
        LubanNdsWirelessConfig config{};
        config.enabled = true;
        config.port = port;
        lubanNdsWirelessConfigure(&config);
        Require(lubanNdsWirelessOpenSocket(), "socket did not open for transient send test");
        io.sendError = EIO;
        lubanNdsWirelessSendPacket(packet.data(), packet.size());
        LubanNdsWirelessRuntimeState state{};
        Require(lubanNdsWirelessGetRuntimeState(&state) && state.txPackets == 0 && state.lastError[0] != '\0',
                "real send error was not recorded");
        const std::string sendError = state.lastError;
        io.sendError = error;
        lubanNdsWirelessSendPacket(packet.data(), packet.size());
        Require(lubanNdsWirelessGetRuntimeState(&state), "runtime state unavailable after transient send");
        Require(io.sendCalls == 2, "transient send error retried instead of making one syscall");
        Require(state.txPackets == 0 && std::string(state.lastError) == sendError,
                "transient send error cleared the prior real error");
        io.sendError = 0;
        lubanNdsWirelessSendPacket(packet.data(), packet.size());
        Require(lubanNdsWirelessGetRuntimeState(&state) && io.sendCalls == 3 && state.txPackets == 1 &&
                    state.lastError[0] == '\0',
                "successful send did not clear the prior real error");
        lubanNdsWirelessStop();

        io = RuntimeTestIo{};
        SetRuntimeTestHooks(&io);
        lubanNdsWirelessConfigure(&config);
        Require(lubanNdsWirelessOpenSocket(), "socket did not open for transient receive test");
        io.receiveError = EIO;
        Require(!lubanNdsWirelessPollPackets(Receive, nullptr), "real receive error reported a packet");
        Require(lubanNdsWirelessGetRuntimeState(&state) && state.rxPackets == 0 && state.lastError[0] != '\0',
                "real receive error was not recorded");
        const std::string receiveError = state.lastError;
        io.receiveError = error;
        Require(!lubanNdsWirelessPollPackets(Receive, nullptr), "transient receive error reported a packet");
        Require(lubanNdsWirelessGetRuntimeState(&state), "runtime state unavailable after transient receive");
        Require(io.receiveCalls == 2, "transient receive error retried instead of making one syscall");
        Require(state.rxPackets == 0 && std::string(state.lastError) == receiveError,
                "transient receive error cleared the prior real error");
        io.receiveError = 0;
        io.receiveQueue = {{Endpoint(INADDR_LOOPBACK, port), packet}};
        ReceiveState received;
        Require(lubanNdsWirelessPollPackets(Receive, &received), "successful receive was not observed");
        Require(lubanNdsWirelessGetRuntimeState(&state) && io.receiveCalls == 4 && received.calls == 1 &&
                    state.rxPackets == 1 && state.lastError[0] == '\0',
                "successful receive did not clear the prior real error");
        lubanNdsWirelessStop();
    }
    ClearRuntimeTestHooks();
}

void TestLocalAddressCacheIgnoresSelfAndLocksRemotePeer()
{
    const uint16_t port = FindFreeLoopbackPort();
    Require(port != 0, "could not allocate a loopback port");
    if (port == 0) return;
    RuntimeTestIo io;
    io.localAddresses = {htonl(INADDR_LOOPBACK)};
    const uint16_t remotePort = static_cast<uint16_t>(port == 65535 ? 65534 : port + 1);
    const sockaddr_in remote = Endpoint(INADDR_LOOPBACK + 1, remotePort);
    io.receiveQueue = {
        {Endpoint(INADDR_LOOPBACK, port), ValidMpPacket()},
        {remote, ValidMpPacket()},
        {Endpoint(INADDR_LOOPBACK + 2, remotePort), ValidMpPacket()},
        {Endpoint(INADDR_LOOPBACK + 2, remotePort), {'L', 'U', 'B', 'A', 'N', '-', 'N', 'D', 'S', '1'}},
    };
    SetRuntimeTestHooks(&io);

    LubanNdsWirelessConfig config{};
    config.enabled = true;
    config.port = port;
    lubanNdsWirelessConfigure(&config);
    Require(lubanNdsWirelessOpenSocket(), "host socket did not open for local-address cache test");
    Require(io.enumerateCalls == 1, "local IPv4 addresses were not cached when opening socket");

    const std::vector<uint8_t> packet = ValidMpPacket();
    lubanNdsWirelessSendPacket(packet.data(), packet.size());
    Require(io.sendCalls == 1 && io.lastDestination.sin_addr.s_addr == htonl(INADDR_BROADCAST),
            "unlocked host did not broadcast its MP packet");

    ReceiveState received;
    Require(lubanNdsWirelessPollPackets(Receive, &received), "remote MP packet did not lock the host peer");
    Require(received.calls == 1 && io.enumerateCalls == 1,
            "self packet, a second peer packet, or a different-IP registration was accepted, or local IPv4 enumeration ran in the receive hot path");

    lubanNdsWirelessSendPacket(packet.data(), packet.size());
    Require(io.sendCalls == 2 && io.lastDestination.sin_addr.s_addr == remote.sin_addr.s_addr &&
                io.lastDestination.sin_port == remote.sin_port,
            "locked host did not unicast to the remote ephemeral-port MP peer");

    lubanNdsWirelessCloseSocket();
    io.localAddresses = {htonl(INADDR_LOOPBACK + 1)};
    io.receiveQueue = {{Endpoint(INADDR_LOOPBACK + 1, port), ValidMpPacket()}};
    io.nextDatagram = 0;
    Require(lubanNdsWirelessOpenSocket(), "host socket did not reopen for local-address cache test");
    Require(io.enumerateCalls == 2, "socket reopen did not refresh the local IPv4 cache");
    received = ReceiveState{};
    Require(!lubanNdsWirelessPollPackets(Receive, &received) && received.calls == 0,
            "socket close did not clear the previous peer and local-address cache");
    lubanNdsWirelessStop();
    ClearRuntimeTestHooks();
}

void TestPollPacketAndByteBudgets()
{
    const uint16_t port = FindFreeLoopbackPort();
    Require(port != 0, "could not allocate a loopback port");
    if (port == 0) return;
    RuntimeTestIo io;
    io.localAddresses = {htonl(INADDR_LOOPBACK)};
    const sockaddr_in remote = Endpoint(INADDR_LOOPBACK + 1, port);
    for (uint8_t index = 0; index < 65; ++index) {
        std::vector<uint8_t> packet = ValidMpPacket();
        packet[0] = index;
        io.receiveQueue.push_back({remote, std::move(packet)});
    }
    SetRuntimeTestHooks(&io);
    LubanNdsWirelessConfig config{};
    config.enabled = true;
    config.port = port;
    lubanNdsWirelessConfigure(&config);
    Require(lubanNdsWirelessOpenSocket(), "host socket did not open");
    ReceiveState received;
    Require(lubanNdsWirelessPollPackets(Receive, &received), "first packet-budget poll did not observe packets");
    Require(io.receiveCalls == 64 && received.calls == 64 && received.packetIds.size() == 64,
            "per-poll packet limit did not make exactly 64 receive calls");
    for (uint8_t index = 0; index < 64 && index < received.packetIds.size(); ++index) {
        Require(received.packetIds[index] == index, "packet budget did not preserve the first 64 queued packets");
    }
    Require(lubanNdsWirelessPollPackets(Receive, &received), "packet after the packet budget was not delivered");
    Require(io.receiveCalls == 66 && received.calls == 65 && received.packetIds.back() == 64,
            "next poll did not continue after the packet budget");

    const std::vector<uint8_t> maximumPacket = ValidMpPacket(10 + 2048);
    io = RuntimeTestIo{};
    io.localAddresses = {htonl(INADDR_LOOPBACK)};
    for (uint8_t index = 0; index < 17; ++index) {
        std::vector<uint8_t> packet = maximumPacket;
        packet[0] = index;
        io.receiveQueue.push_back({remote, std::move(packet)});
    }
    SetRuntimeTestHooks(&io);
    lubanNdsWirelessCloseSocket();
    Require(lubanNdsWirelessOpenSocket(), "host socket did not reopen for byte budget test");
    received = ReceiveState{};
    Require(lubanNdsWirelessPollPackets(Receive, &received), "byte-budget poll did not observe packets");
    Require(io.receiveCalls == 16 && received.calls == 16 && received.bytes == maximumPacket.size() * 16,
            "byte budget did not deliver the packet that crossed the 32 KiB boundary");
    Require(received.packetIds.size() == 16 && received.packetIds.back() == 15,
            "byte budget did not preserve the crossing packet");
    Require(lubanNdsWirelessPollPackets(Receive, &received), "next byte-budget poll did not observe the remaining packet");
    Require(io.receiveCalls == 18 && received.calls == 17 && received.packetIds.back() == 16,
            "next poll did not continue after the byte budget");
    lubanNdsWirelessStop();
    ClearRuntimeTestHooks();
}
} // namespace

int main()
{
    TestConfiguredUnicastAndEmptyPacketRejection();
    TestLoopbackHostClientExchangeAndRestart();
    TestClientAndHostRestartRecovery();
    TestHostBroadcastsBeforeLockAndLocksValidPeer();
    TestTransientSocketErrorsPreserveLastErrorUntilSuccess();
    TestLocalAddressCacheIgnoresSelfAndLocksRemotePeer();
    TestPollPacketAndByteBudgets();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
