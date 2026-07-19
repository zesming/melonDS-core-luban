/*
    Copyright 2023 Jesse Talavera-Greenberg

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

#include "libretro.hpp"

// NOT UNUSED; GPU.h doesn't #include OpenGL, so I do it here.
// This must come before <GPU.h>!
#include "PlatformOGLPrivate.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <span>

#include <compat/strl.h>
#include <file/file_path.h>
#include <libretro.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>

#undef isnan
#include <fmt/format.h>

#include "config/config.hpp"
#include "core/core.hpp"
#include "environment.hpp"
#include "exceptions.hpp"
#include "info.hpp"
#include "retro/task_queue.hpp"
#include "sram.hpp"
#include "tracy.hpp"
#include "version.hpp"

using namespace melonDS;
using std::make_optional;
using std::optional;
using std::nullopt;
using std::span;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::make_unique;
using retro::task::TaskSpec;

namespace MelonDsDs {
    // Aligned with CoreState to prevent undefined behavior
    alignas(CoreState) static std::array<std::byte, sizeof(CoreState)> CoreStateBuffer;
    CoreState& Core = *reinterpret_cast<CoreState*>(CoreStateBuffer.data());
}

namespace {
std::atomic<int32_t> g_mpBeginCalls{0};
std::atomic<int32_t> g_mpEndCalls{0};
std::atomic<int32_t> g_mpSendCalls{0};
std::atomic<int32_t> g_mpReceiveCalls{0};
std::atomic<int32_t> g_mpChannelSetCalls{0};
std::atomic<int32_t> g_mpTxStartCalls{0};
std::atomic<int32_t> g_mpTxFrameCalls{0};
std::atomic<int32_t> g_mpRxCheckCalls{0};
std::atomic<int32_t> g_mpClientSyncCalls{0};
std::atomic<int32_t> g_mpChannelChangeCalls{0};
std::atomic<int32_t> g_mpLastChannel{0};
std::atomic<int32_t> g_mpFireTxCalls{0};
std::atomic<int32_t> g_mpFireTxBlockedRxDisabledCalls{0};
std::atomic<int32_t> g_mpFireTxNoRequestCalls{0};
std::atomic<int32_t> g_mpLastRxCnt{0};
std::atomic<int32_t> g_mpLastTxReq{0};
std::atomic<int32_t> g_mpLastTxBusy{0};
std::atomic<int32_t> g_mpLastTxSlotMask{0};
std::atomic<int32_t> g_mpLastTxStartMask{0};
std::atomic<int32_t> g_mpRxCntWriteCalls{0};
std::atomic<int32_t> g_mpRxCntEnableWriteCalls{0};
std::atomic<int32_t> g_mpLastRxCntWrite{0};
std::atomic<int32_t> g_mpLastRxCntLatched{0};
std::atomic<int32_t> g_mpLastWifiRegWriteAddr{0};
std::atomic<int32_t> g_mpLastWifiRegWriteValue{0};
std::atomic<int32_t> g_mpLastWifiRegWriteCpu{-1};
std::atomic<int32_t> g_mpLastWifiRegWritePc{0};
std::atomic<int32_t> g_mpWifiRegReadCalls{0};
std::atomic<int32_t> g_mpLastWifiRegReadAddr{0};
std::atomic<int32_t> g_mpLastWifiRegReadValue{0};
std::atomic<int32_t> g_mpLastWifiRegReadCpu{-1};
std::atomic<int32_t> g_mpLastWifiRegReadPc{0};
std::atomic<int32_t> g_mpTxReqSetWriteCalls{0};
std::atomic<int32_t> g_mpLastTxReqSetWrite{0};
std::atomic<int32_t> g_mpLastTxReqSetWriteCpu{-1};
std::atomic<int32_t> g_mpLastTxReqSetWritePc{0};
std::atomic<int32_t> g_mpLastTxReqSetSlotLoc1{0};
std::atomic<int32_t> g_mpLastTxReqSetSlotCmd{0};
std::atomic<int32_t> g_mpLastTxReqSetSlotLoc2{0};
std::atomic<int32_t> g_mpLastTxReqSetSlotLoc3{0};
std::atomic<int32_t> g_mpTxSlotWriteCalls{0};
std::atomic<int32_t> g_mpLastTxSlotWriteAddr{0};
std::atomic<int32_t> g_mpLastTxSlotWriteValue{0};
std::atomic<int32_t> g_mpLastTxSlotWriteCpu{-1};
std::atomic<int32_t> g_mpLastTxSlotWritePc{0};
std::atomic<int32_t> g_mpLastTxSlotLoc1{0};
std::atomic<int32_t> g_mpLastTxSlotCmd{0};
std::atomic<int32_t> g_mpLastTxSlotLoc2{0};
std::atomic<int32_t> g_mpLastTxSlotLoc3{0};
std::atomic<int32_t> g_mpTxSlotCmdWriteCalls{0};
std::atomic<int32_t> g_mpLastTxSlotCmdRawWrite{0};
std::atomic<int32_t> g_mpLastTxSlotCmdLatched{0};
std::atomic<int32_t> g_mpLastTxSlotCmdCounter{0};
std::atomic<int32_t> g_mpTxSlotCmdBlockedByCounterCalls{0};
std::atomic<int32_t> g_mpModeResetWriteCalls{0};
std::atomic<int32_t> g_mpLastModeResetWrite{0};
std::atomic<int32_t> g_mpLastModeReset{0};
std::atomic<int32_t> g_mpModeWepWriteCalls{0};
std::atomic<int32_t> g_mpLastModeWep{0};
std::atomic<int32_t> g_mpPowerStateWriteCalls{0};
std::atomic<int32_t> g_mpPowerStateIgnoredWriteCalls{0};
std::atomic<int32_t> g_mpLastPowerStateWrite{0};
std::atomic<int32_t> g_mpLastPowerState{0};
std::atomic<int32_t> g_mpLastRxBufBegin{0};
std::atomic<int32_t> g_mpLastRxBufEnd{0};
std::atomic<int32_t> g_mpRxBufWriteAddrWriteCalls{0};
std::atomic<int32_t> g_mpLastRxBufWriteAddr{0};
std::atomic<int32_t> g_mpTxBufWriteAddrWriteCalls{0};
std::atomic<int32_t> g_mpLastTxBufWriteAddr{0};
std::atomic<int32_t> g_mpTxBufCountWriteCalls{0};
std::atomic<int32_t> g_mpLastTxBufCount{0};
std::atomic<int32_t> g_mpTxBufDataWriteCalls{0};
std::atomic<int32_t> g_mpLastTxBufDataWriteAddr{0};
std::atomic<int32_t> g_mpLastTxBufDataWriteValue{0};
std::atomic<int32_t> g_mpLastTxBufDataWriteNextAddr{0};
std::atomic<int32_t> g_mpLastTxBufDataWriteRemaining{0};
std::atomic<int32_t> g_mpLastRfRaw1{0};
std::atomic<int32_t> g_mpLastRfRaw2{0};
std::atomic<int32_t> g_mpLastRfIndex1{0};
std::atomic<int32_t> g_mpLastRfIndex2{0};
std::atomic<int32_t> g_mpRfValidChannelSnapshots{0};
std::atomic<int32_t> g_mpRfRaw1MatchChannel{0};
std::atomic<int32_t> g_mpRfRaw2MatchChannel{0};
std::atomic<int32_t> g_mpRfClosestChannel{0};
std::atomic<int32_t> g_mpRfClosestRaw1{0};
std::atomic<int32_t> g_mpRfClosestRaw2{0};
std::mutex g_mpWifiRegWriteTraceMutex;
char g_mpWifiRegWriteTraceTail[256] = "";
}

static void appendMpWifiRegWriteTrace(int32_t addr, int32_t value, int32_t cpu, int32_t pc) {
    char entry[32];
    std::snprintf(entry,
                  sizeof(entry),
                  "%03X=%04X@%d:%08X",
                  addr & 0xFFF,
                  value & 0xFFFF,
                  cpu,
                  static_cast<uint32_t>(pc));

    std::lock_guard<std::mutex> lock(g_mpWifiRegWriteTraceMutex);
    char candidate[sizeof(g_mpWifiRegWriteTraceTail) + sizeof(entry)];
    if (g_mpWifiRegWriteTraceTail[0] != '\0') {
        std::snprintf(candidate, sizeof(candidate), "%s;%s", g_mpWifiRegWriteTraceTail, entry);
    } else {
        strlcpy(candidate, entry, sizeof(candidate));
    }

    const size_t candidateLength = std::strlen(candidate);
    const char *tail = candidate;
    if (candidateLength >= sizeof(g_mpWifiRegWriteTraceTail)) {
        tail = candidate + candidateLength - (sizeof(g_mpWifiRegWriteTraceTail) - 1);
        const char *entryStart = std::strchr(tail, ';');
        if (entryStart && entryStart[1] != '\0') {
            tail = entryStart + 1;
        }
    }
    strlcpy(g_mpWifiRegWriteTraceTail, tail, sizeof(g_mpWifiRegWriteTraceTail));
}

extern "C" void melondsds_record_mp_begin_call(void) {
    g_mpBeginCalls.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_end_call(void) {
    g_mpEndCalls.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_channel_set_call(void) {
    g_mpChannelSetCalls.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_tx_start_call(void) {
    g_mpTxStartCalls.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_tx_frame_call(void) {
    g_mpTxFrameCalls.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_rx_check_call(void) {
    g_mpRxCheckCalls.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_client_sync_call(void) {
    g_mpClientSyncCalls.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_channel_change_call(int32_t channel) {
    g_mpChannelChangeCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastChannel.store(channel, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_fire_tx_gate(int32_t rxCnt,
                                                 int32_t txReq,
                                                 int32_t txBusy,
                                                 int32_t txSlotMask,
                                                 int32_t txStartMask,
                                                 int32_t txSlotLoc1,
                                                 int32_t txSlotCmd,
                                                 int32_t txSlotLoc2,
                                                 int32_t txSlotLoc3) {
    g_mpFireTxCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastRxCnt.store(rxCnt, std::memory_order_relaxed);
    g_mpLastTxReq.store(txReq, std::memory_order_relaxed);
    g_mpLastTxBusy.store(txBusy, std::memory_order_relaxed);
    g_mpLastTxSlotMask.store(txSlotMask, std::memory_order_relaxed);
    g_mpLastTxStartMask.store(txStartMask, std::memory_order_relaxed);
    g_mpLastTxSlotLoc1.store(txSlotLoc1, std::memory_order_relaxed);
    g_mpLastTxSlotCmd.store(txSlotCmd, std::memory_order_relaxed);
    g_mpLastTxSlotLoc2.store(txSlotLoc2, std::memory_order_relaxed);
    g_mpLastTxSlotLoc3.store(txSlotLoc3, std::memory_order_relaxed);
    if (txStartMask == 0) {
        g_mpFireTxNoRequestCalls.fetch_add(1, std::memory_order_relaxed);
    } else if ((rxCnt & 0x8000) == 0) {
        g_mpFireTxBlockedRxDisabledCalls.fetch_add(1, std::memory_order_relaxed);
    }
}

extern "C" void melondsds_record_mp_rx_cnt_write(int32_t raw, int32_t latched) {
    g_mpRxCntWriteCalls.fetch_add(1, std::memory_order_relaxed);
    if ((raw & 0x8000) != 0) {
        g_mpRxCntEnableWriteCalls.fetch_add(1, std::memory_order_relaxed);
    }
    g_mpLastRxCntWrite.store(raw, std::memory_order_relaxed);
    g_mpLastRxCntLatched.store(latched, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_wifi_register_write(int32_t addr,
                                                        int32_t value,
                                                        int32_t cpu,
                                                        int32_t pc) {
    g_mpLastWifiRegWriteAddr.store(addr, std::memory_order_relaxed);
    g_mpLastWifiRegWriteValue.store(value, std::memory_order_relaxed);
    g_mpLastWifiRegWriteCpu.store(cpu, std::memory_order_relaxed);
    g_mpLastWifiRegWritePc.store(pc, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_wifi_register_write_trace(int32_t addr,
                                                              int32_t value,
                                                              int32_t cpu,
                                                              int32_t pc) {
    appendMpWifiRegWriteTrace(addr, value, cpu, pc);
}

extern "C" void melondsds_record_mp_wifi_register_read(int32_t addr,
                                                       int32_t value,
                                                       int32_t cpu,
                                                       int32_t pc) {
    g_mpWifiRegReadCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastWifiRegReadAddr.store(addr, std::memory_order_relaxed);
    g_mpLastWifiRegReadValue.store(value, std::memory_order_relaxed);
    g_mpLastWifiRegReadCpu.store(cpu, std::memory_order_relaxed);
    g_mpLastWifiRegReadPc.store(pc, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_tx_req_set_write(int32_t value,
                                                     int32_t cpu,
                                                     int32_t pc,
                                                     int32_t txSlotLoc1,
                                                     int32_t txSlotCmd,
                                                     int32_t txSlotLoc2,
                                                     int32_t txSlotLoc3) {
    g_mpTxReqSetWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastTxReqSetWrite.store(value, std::memory_order_relaxed);
    g_mpLastTxReqSetWriteCpu.store(cpu, std::memory_order_relaxed);
    g_mpLastTxReqSetWritePc.store(pc, std::memory_order_relaxed);
    g_mpLastTxReqSetSlotLoc1.store(txSlotLoc1, std::memory_order_relaxed);
    g_mpLastTxReqSetSlotCmd.store(txSlotCmd, std::memory_order_relaxed);
    g_mpLastTxReqSetSlotLoc2.store(txSlotLoc2, std::memory_order_relaxed);
    g_mpLastTxReqSetSlotLoc3.store(txSlotLoc3, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_tx_slot_write(int32_t addr,
                                                  int32_t value,
                                                  int32_t cpu,
                                                  int32_t pc) {
    g_mpTxSlotWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastTxSlotWriteAddr.store(addr, std::memory_order_relaxed);
    g_mpLastTxSlotWriteValue.store(value, std::memory_order_relaxed);
    g_mpLastTxSlotWriteCpu.store(cpu, std::memory_order_relaxed);
    g_mpLastTxSlotWritePc.store(pc, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_tx_slot_cmd_write(int32_t raw,
                                                      int32_t latched,
                                                      int32_t cmdCounter,
                                                      int32_t blockedByCmdCounter) {
    g_mpTxSlotCmdWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastTxSlotCmdRawWrite.store(raw, std::memory_order_relaxed);
    g_mpLastTxSlotCmdLatched.store(latched, std::memory_order_relaxed);
    g_mpLastTxSlotCmdCounter.store(cmdCounter, std::memory_order_relaxed);
    if (blockedByCmdCounter != 0) {
        g_mpTxSlotCmdBlockedByCounterCalls.fetch_add(1, std::memory_order_relaxed);
    }
}

extern "C" void melondsds_record_mp_setup_snapshot(int32_t modeReset,
                                                   int32_t modeWep,
                                                   int32_t powerState,
                                                   int32_t rxBufBegin,
                                                   int32_t rxBufEnd,
                                                   int32_t rxBufWriteAddr,
                                                   int32_t txBufWriteAddr,
                                                   int32_t txBufCount) {
    g_mpLastModeReset.store(modeReset, std::memory_order_relaxed);
    g_mpLastModeWep.store(modeWep, std::memory_order_relaxed);
    g_mpLastPowerState.store(powerState, std::memory_order_relaxed);
    g_mpLastRxBufBegin.store(rxBufBegin, std::memory_order_relaxed);
    g_mpLastRxBufEnd.store(rxBufEnd, std::memory_order_relaxed);
    g_mpLastRxBufWriteAddr.store(rxBufWriteAddr, std::memory_order_relaxed);
    g_mpLastTxBufWriteAddr.store(txBufWriteAddr, std::memory_order_relaxed);
    g_mpLastTxBufCount.store(txBufCount, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_mode_reset_write(int32_t raw, int32_t latched) {
    g_mpModeResetWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastModeResetWrite.store(raw, std::memory_order_relaxed);
    g_mpLastModeReset.store(latched, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_mode_wep_write(int32_t value) {
    g_mpModeWepWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastModeWep.store(value, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_power_state_write(int32_t raw, int32_t latched, int32_t ignored) {
    g_mpPowerStateWriteCalls.fetch_add(1, std::memory_order_relaxed);
    if (ignored) {
        g_mpPowerStateIgnoredWriteCalls.fetch_add(1, std::memory_order_relaxed);
    }
    g_mpLastPowerStateWrite.store(raw, std::memory_order_relaxed);
    g_mpLastPowerState.store(latched, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_rx_buf_write_addr_write(int32_t value) {
    g_mpRxBufWriteAddrWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastRxBufWriteAddr.store(value, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_tx_buf_write_addr_write(int32_t value) {
    g_mpTxBufWriteAddrWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastTxBufWriteAddr.store(value, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_tx_buf_count_write(int32_t value) {
    g_mpTxBufCountWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastTxBufCount.store(value, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_tx_buf_data_write(int32_t addr,
                                                      int32_t value,
                                                      int32_t nextAddr,
                                                      int32_t remainingCount) {
    g_mpTxBufDataWriteCalls.fetch_add(1, std::memory_order_relaxed);
    g_mpLastTxBufDataWriteAddr.store(addr, std::memory_order_relaxed);
    g_mpLastTxBufDataWriteValue.store(value, std::memory_order_relaxed);
    g_mpLastTxBufDataWriteNextAddr.store(nextAddr, std::memory_order_relaxed);
    g_mpLastTxBufDataWriteRemaining.store(remainingCount, std::memory_order_relaxed);
}

extern "C" void melondsds_record_mp_rf_channel_snapshot(int32_t raw1,
                                                        int32_t raw2,
                                                        int32_t index1,
                                                        int32_t index2,
                                                        int32_t valid,
                                                        int32_t raw1MatchChannel,
                                                        int32_t raw2MatchChannel,
                                                        int32_t closestChannel,
                                                        int32_t closestRaw1,
                                                        int32_t closestRaw2) {
    g_mpLastRfRaw1.store(raw1, std::memory_order_relaxed);
    g_mpLastRfRaw2.store(raw2, std::memory_order_relaxed);
    g_mpLastRfIndex1.store(index1, std::memory_order_relaxed);
    g_mpLastRfIndex2.store(index2, std::memory_order_relaxed);
    g_mpRfRaw1MatchChannel.store(raw1MatchChannel, std::memory_order_relaxed);
    g_mpRfRaw2MatchChannel.store(raw2MatchChannel, std::memory_order_relaxed);
    g_mpRfClosestChannel.store(closestChannel, std::memory_order_relaxed);
    g_mpRfClosestRaw1.store(closestRaw1, std::memory_order_relaxed);
    g_mpRfClosestRaw2.store(closestRaw2, std::memory_order_relaxed);
    if (valid) {
        g_mpRfValidChannelSnapshots.fetch_add(1, std::memory_order_relaxed);
    }
}

static void resetMpDiagnostics() {
    g_mpBeginCalls.store(0, std::memory_order_relaxed);
    g_mpEndCalls.store(0, std::memory_order_relaxed);
    g_mpSendCalls.store(0, std::memory_order_relaxed);
    g_mpReceiveCalls.store(0, std::memory_order_relaxed);
    g_mpChannelSetCalls.store(0, std::memory_order_relaxed);
    g_mpTxStartCalls.store(0, std::memory_order_relaxed);
    g_mpTxFrameCalls.store(0, std::memory_order_relaxed);
    g_mpRxCheckCalls.store(0, std::memory_order_relaxed);
    g_mpClientSyncCalls.store(0, std::memory_order_relaxed);
    g_mpChannelChangeCalls.store(0, std::memory_order_relaxed);
    g_mpLastChannel.store(0, std::memory_order_relaxed);
    g_mpFireTxCalls.store(0, std::memory_order_relaxed);
    g_mpFireTxBlockedRxDisabledCalls.store(0, std::memory_order_relaxed);
    g_mpFireTxNoRequestCalls.store(0, std::memory_order_relaxed);
    g_mpLastRxCnt.store(0, std::memory_order_relaxed);
    g_mpLastTxReq.store(0, std::memory_order_relaxed);
    g_mpLastTxBusy.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotMask.store(0, std::memory_order_relaxed);
    g_mpLastTxStartMask.store(0, std::memory_order_relaxed);
    g_mpRxCntWriteCalls.store(0, std::memory_order_relaxed);
    g_mpRxCntEnableWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastRxCntWrite.store(0, std::memory_order_relaxed);
    g_mpLastRxCntLatched.store(0, std::memory_order_relaxed);
    g_mpLastWifiRegWriteAddr.store(0, std::memory_order_relaxed);
    g_mpLastWifiRegWriteValue.store(0, std::memory_order_relaxed);
    g_mpLastWifiRegWriteCpu.store(-1, std::memory_order_relaxed);
    g_mpLastWifiRegWritePc.store(0, std::memory_order_relaxed);
    g_mpWifiRegReadCalls.store(0, std::memory_order_relaxed);
    g_mpLastWifiRegReadAddr.store(0, std::memory_order_relaxed);
    g_mpLastWifiRegReadValue.store(0, std::memory_order_relaxed);
    g_mpLastWifiRegReadCpu.store(-1, std::memory_order_relaxed);
    g_mpLastWifiRegReadPc.store(0, std::memory_order_relaxed);
    g_mpTxReqSetWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastTxReqSetWrite.store(0, std::memory_order_relaxed);
    g_mpLastTxReqSetWriteCpu.store(-1, std::memory_order_relaxed);
    g_mpLastTxReqSetWritePc.store(0, std::memory_order_relaxed);
    g_mpLastTxReqSetSlotLoc1.store(0, std::memory_order_relaxed);
    g_mpLastTxReqSetSlotCmd.store(0, std::memory_order_relaxed);
    g_mpLastTxReqSetSlotLoc2.store(0, std::memory_order_relaxed);
    g_mpLastTxReqSetSlotLoc3.store(0, std::memory_order_relaxed);
    g_mpTxSlotWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotWriteAddr.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotWriteValue.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotWriteCpu.store(-1, std::memory_order_relaxed);
    g_mpLastTxSlotWritePc.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotLoc1.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotCmd.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotLoc2.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotLoc3.store(0, std::memory_order_relaxed);
    g_mpTxSlotCmdWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotCmdRawWrite.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotCmdLatched.store(0, std::memory_order_relaxed);
    g_mpLastTxSlotCmdCounter.store(0, std::memory_order_relaxed);
    g_mpTxSlotCmdBlockedByCounterCalls.store(0, std::memory_order_relaxed);
    g_mpModeResetWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastModeResetWrite.store(0, std::memory_order_relaxed);
    g_mpLastModeReset.store(0, std::memory_order_relaxed);
    g_mpModeWepWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastModeWep.store(0, std::memory_order_relaxed);
    g_mpPowerStateWriteCalls.store(0, std::memory_order_relaxed);
    g_mpPowerStateIgnoredWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastPowerStateWrite.store(0, std::memory_order_relaxed);
    g_mpLastPowerState.store(0, std::memory_order_relaxed);
    g_mpLastRxBufBegin.store(0, std::memory_order_relaxed);
    g_mpLastRxBufEnd.store(0, std::memory_order_relaxed);
    g_mpRxBufWriteAddrWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastRxBufWriteAddr.store(0, std::memory_order_relaxed);
    g_mpTxBufWriteAddrWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastTxBufWriteAddr.store(0, std::memory_order_relaxed);
    g_mpTxBufCountWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastTxBufCount.store(0, std::memory_order_relaxed);
    g_mpTxBufDataWriteCalls.store(0, std::memory_order_relaxed);
    g_mpLastTxBufDataWriteAddr.store(0, std::memory_order_relaxed);
    g_mpLastTxBufDataWriteValue.store(0, std::memory_order_relaxed);
    g_mpLastTxBufDataWriteNextAddr.store(0, std::memory_order_relaxed);
    g_mpLastTxBufDataWriteRemaining.store(0, std::memory_order_relaxed);
    g_mpLastRfRaw1.store(0, std::memory_order_relaxed);
    g_mpLastRfRaw2.store(0, std::memory_order_relaxed);
    g_mpLastRfIndex1.store(0, std::memory_order_relaxed);
    g_mpLastRfIndex2.store(0, std::memory_order_relaxed);
    g_mpRfValidChannelSnapshots.store(0, std::memory_order_relaxed);
    g_mpRfRaw1MatchChannel.store(0, std::memory_order_relaxed);
    g_mpRfRaw2MatchChannel.store(0, std::memory_order_relaxed);
    g_mpRfClosestChannel.store(0, std::memory_order_relaxed);
    g_mpRfClosestRaw1.store(0, std::memory_order_relaxed);
    g_mpRfClosestRaw2.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(g_mpWifiRegWriteTraceMutex);
        g_mpWifiRegWriteTraceTail[0] = '\0';
    }
}

PUBLIC_SYMBOL void retro_init(void) {
#ifdef HAVE_TRACY
    tracy::StartupProfiler();
#endif
    TracySetProgramName(MELONDSDS_VERSION);
    ZoneScopedN(TracyFunction);
    retro::env::init();
    retro::debug("retro_init");
    retro::info("{} {}", MELONDSDS_NAME, MELONDSDS_VERSION);
    retro_assert(!MelonDsDs::Core.IsInitialized());
    resetMpDiagnostics();

    retro::task::init(false, nullptr);

    memset(MelonDsDs::CoreStateBuffer.data(), 0, MelonDsDs::CoreStateBuffer.size());
    new(&MelonDsDs::CoreStateBuffer) MelonDsDs::CoreState(); // placement-new the CoreState
    retro_assert(MelonDsDs::Core.IsInitialized());
}

PUBLIC_SYMBOL bool retro_load_game(const struct retro_game_info *info) {
    ZoneScopedN(TracyFunction);
    if (info) {
        ZoneText(info->path, strlen(info->path));
        retro::debug("retro_load_game(\"{}\", {})", info->path ? info->path : "", info->size);
    }
    else {
        retro::debug("retro_load_game(<no content>)");
    }

    std::span<const retro_game_info> content = info ? std::span(info, 1) : std::span<const retro_game_info>();

    return MelonDsDs::Core.LoadGame(MelonDsDs::MELONDSDS_GAME_TYPE_NDS, content);
}

PUBLIC_SYMBOL void retro_get_system_av_info(struct retro_system_av_info *info) {
    ZoneScopedN(TracyFunction);
    retro::debug(TracyFunction);

    retro_assert(info != nullptr);

    *info = MelonDsDs::Core.GetSystemAvInfo();

    retro::debug("retro_get_system_av_info finished");
}

PUBLIC_SYMBOL void retro_set_controller_port_device(unsigned port, unsigned device) {
    MelonDsDs::Core.GetInputState().SetControllerPortDevice(port, device);
}

PUBLIC_SYMBOL [[gnu::hot]] void retro_run(void) {
    {
        ZoneScopedN(TracyFunction);
        MelonDsDs::Core.Run();
    }
    FrameMark;
}

PUBLIC_SYMBOL void retro_unload_game(void) {
    ZoneScopedN(TracyFunction);
    using MelonDsDs::Core;
    retro::debug("retro_unload_game()");
    // No need to flush SRAM to the buffer, Platform::WriteNDSSave has been doing that for us this whole time
    // No need to flush the homebrew save data either, the CartHomebrew destructor does that

    // The cleanup handlers for each task will flush data to disk if needed
    retro::task::reset();
    retro::task::wait();
    retro::task::deinit();

    Core.UnloadGame();
}

PUBLIC_SYMBOL unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC;
}

PUBLIC_SYMBOL bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
    ZoneScopedN(TracyFunction);
    retro::debug("retro_load_game_special({}, {}, {})", MelonDsDs::get_game_type_name(type), fmt::ptr(info), num);

    return MelonDsDs::Core.LoadGame(type, std::span(info, num));
}

// We deinitialize all these variables just in case the frontend doesn't unload the dynamic library.
// It might be keeping the library around for debugging purposes,
// or it might just be buggy.
PUBLIC_SYMBOL void retro_deinit(void) {
    { // Scoped so that we can capture one last scope before shutting down the profiler
        ZoneScopedN(TracyFunction);
        retro::debug("retro_deinit()");
        retro::task::deinit();
        MelonDsDs::Core.~CoreState(); // placement delete
        memset(MelonDsDs::CoreStateBuffer.data(), 0, MelonDsDs::CoreStateBuffer.size());
        retro_assert(!MelonDsDs::Core.IsInitialized());
        retro::env::deinit();
    }

#ifdef HAVE_TRACY
    tracy::ShutdownProfiler();
#endif
}

PUBLIC_SYMBOL unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

PUBLIC_SYMBOL void retro_get_system_info(struct retro_system_info *info) {
    info->library_name = MELONDSDS_NAME;
    info->block_extract = false;
    info->library_version = MELONDSDS_VERSION;
    info->need_fullpath = false;
    info->valid_extensions = "nds|ids|dsi";
}

PUBLIC_SYMBOL void retro_reset(void) {
    ZoneScopedN(TracyFunction);
    retro::debug("retro_reset()\n");

    try {
        MelonDsDs::Core.Reset();
    }
    catch (const MelonDsDs::opengl_exception& e) {
        retro::error("{}", e.what());
        retro::set_error_message(e.user_message());
        retro::shutdown();
        // TODO: Instead of shutting down, fall back to the software renderer
    }
    catch (const MelonDsDs::emulator_exception& e) {
        retro::error("{}", e.what());
        retro::set_error_message(e.user_message());
        retro::shutdown();
    }
    catch (const std::exception& e) {
        retro::set_error_message(e.what());
        retro::shutdown();
    }
    catch (...) {
        retro::set_error_message("An unknown error has occurred.");
        retro::shutdown();
    }
}

PUBLIC_SYMBOL void retro_cheat_reset(void) {
    ZoneScopedN(TracyFunction);

    MelonDsDs::Core.CheatReset();
}

PUBLIC_SYMBOL void retro_cheat_set(unsigned index, bool enabled, const char *code) {
    // Cheat codes are small programs, so we can't exactly turn them off (that would be undoing them)
    ZoneScopedN(TracyFunction);

    MelonDsDs::Core.CheatSet(index, enabled, code);
}

static const char *memory_type_name(unsigned type)
{
    switch (type) {
        case RETRO_MEMORY_SAVE_RAM:
            return "RETRO_MEMORY_SAVE_RAM";
        case RETRO_MEMORY_RTC:
            return "RETRO_MEMORY_RTC";
        case RETRO_MEMORY_SYSTEM_RAM:
            return "RETRO_MEMORY_SYSTEM_RAM";
        case RETRO_MEMORY_VIDEO_RAM:
            return "RETRO_MEMORY_VIDEO_RAM";
        case MelonDsDs::MELONDSDS_MEMORY_GBA_SAVE_RAM:
            return "MELONDSDS_MEMORY_GBA_SAVE_RAM";
        default:
            return "<unknown>";
    }
}

PUBLIC_SYMBOL size_t retro_serialize_size(void) {
    ZoneScopedN(TracyFunction);

    return MelonDsDs::Core.SerializeSize();
}

PUBLIC_SYMBOL bool retro_serialize(void *data, size_t size) {
    ZoneScopedN(TracyFunction);

    return MelonDsDs::Core.Serialize(std::span(static_cast<std::byte*>(data), size));
}

PUBLIC_SYMBOL bool retro_unserialize(const void *data, size_t size) {
    ZoneScopedN(TracyFunction);
    retro::debug("retro_unserialize({}, {})", data, size);

    return MelonDsDs::Core.Unserialize(std::span(static_cast<const std::byte*>(data), size));
}

PUBLIC_SYMBOL void *retro_get_memory_data(unsigned type) {
    ZoneScopedN(TracyFunction);
    retro::debug("retro_get_memory_data({})\n", memory_type_name(type));

    return MelonDsDs::Core.GetMemoryData(type);
}

PUBLIC_SYMBOL size_t retro_get_memory_size(unsigned type) {
    ZoneScopedN(TracyFunction);

    return MelonDsDs::Core.GetMemorySize(type);
}

extern "C" PUBLIC_SYMBOL uint64_t melondsds_get_nds_save_generation(void) {
    return MelonDsDs::Core.GetNdsSaveGeneration();
}

extern "C" PUBLIC_SYMBOL void melondsds_get_mp_diagnostics(int32_t *beginCalls,
                                                            int32_t *endCalls,
                                                            int32_t *sendCalls,
                                                            int32_t *receiveCalls,
                                                            int32_t *channelSetCalls,
                                                            int32_t *txStartCalls,
                                                            int32_t *txFrameCalls,
                                                            int32_t *rxCheckCalls,
                                                            int32_t *clientSyncCalls,
                                                            int32_t *channelChangeCalls,
                                                            int32_t *lastChannel,
                                                            int32_t *fireTxCalls,
                                                            int32_t *fireTxBlockedRxDisabledCalls,
                                                            int32_t *fireTxNoRequestCalls,
                                                            int32_t *lastRxCnt,
                                                            int32_t *lastTxReq,
                                                            int32_t *lastTxBusy,
                                                            int32_t *lastTxSlotMask,
                                                            int32_t *lastTxStartMask,
                                                            int32_t *rxCntWriteCalls,
                                                            int32_t *rxCntEnableWriteCalls,
                                                            int32_t *lastRxCntWrite,
                                                            int32_t *lastRxCntLatched,
                                                            int32_t *lastWifiRegWriteAddr,
                                                            int32_t *lastWifiRegWriteValue,
                                                            int32_t *lastWifiRegWriteCpu,
                                                            int32_t *lastWifiRegWritePc,
                                                            int32_t *wifiRegReadCalls,
                                                            int32_t *lastWifiRegReadAddr,
                                                            int32_t *lastWifiRegReadValue,
                                                            int32_t *lastWifiRegReadCpu,
                                                            int32_t *lastWifiRegReadPc,
                                                            int32_t *txReqSetWriteCalls,
                                                            int32_t *lastTxReqSetWrite,
                                                            int32_t *lastTxReqSetWriteCpu,
                                                            int32_t *lastTxReqSetWritePc,
                                                            int32_t *lastTxReqSetSlotLoc1,
                                                            int32_t *lastTxReqSetSlotCmd,
                                                            int32_t *lastTxReqSetSlotLoc2,
                                                            int32_t *lastTxReqSetSlotLoc3,
                                                            int32_t *txSlotWriteCalls,
                                                            int32_t *lastTxSlotWriteAddr,
                                                            int32_t *lastTxSlotWriteValue,
                                                            int32_t *lastTxSlotWriteCpu,
                                                            int32_t *lastTxSlotWritePc) {
    if (beginCalls) *beginCalls = g_mpBeginCalls.load(std::memory_order_relaxed);
    if (endCalls) *endCalls = g_mpEndCalls.load(std::memory_order_relaxed);
    if (sendCalls) *sendCalls = g_mpSendCalls.load(std::memory_order_relaxed);
    if (receiveCalls) *receiveCalls = g_mpReceiveCalls.load(std::memory_order_relaxed);
    if (channelSetCalls) *channelSetCalls = g_mpChannelSetCalls.load(std::memory_order_relaxed);
    if (txStartCalls) *txStartCalls = g_mpTxStartCalls.load(std::memory_order_relaxed);
    if (txFrameCalls) *txFrameCalls = g_mpTxFrameCalls.load(std::memory_order_relaxed);
    if (rxCheckCalls) *rxCheckCalls = g_mpRxCheckCalls.load(std::memory_order_relaxed);
    if (clientSyncCalls) *clientSyncCalls = g_mpClientSyncCalls.load(std::memory_order_relaxed);
    if (channelChangeCalls) *channelChangeCalls = g_mpChannelChangeCalls.load(std::memory_order_relaxed);
    if (lastChannel) *lastChannel = g_mpLastChannel.load(std::memory_order_relaxed);
    if (fireTxCalls) *fireTxCalls = g_mpFireTxCalls.load(std::memory_order_relaxed);
    if (fireTxBlockedRxDisabledCalls) {
        *fireTxBlockedRxDisabledCalls = g_mpFireTxBlockedRxDisabledCalls.load(std::memory_order_relaxed);
    }
    if (fireTxNoRequestCalls) *fireTxNoRequestCalls = g_mpFireTxNoRequestCalls.load(std::memory_order_relaxed);
    if (lastRxCnt) *lastRxCnt = g_mpLastRxCnt.load(std::memory_order_relaxed);
    if (lastTxReq) *lastTxReq = g_mpLastTxReq.load(std::memory_order_relaxed);
    if (lastTxBusy) *lastTxBusy = g_mpLastTxBusy.load(std::memory_order_relaxed);
    if (lastTxSlotMask) *lastTxSlotMask = g_mpLastTxSlotMask.load(std::memory_order_relaxed);
    if (lastTxStartMask) *lastTxStartMask = g_mpLastTxStartMask.load(std::memory_order_relaxed);
    if (rxCntWriteCalls) *rxCntWriteCalls = g_mpRxCntWriteCalls.load(std::memory_order_relaxed);
    if (rxCntEnableWriteCalls) {
        *rxCntEnableWriteCalls = g_mpRxCntEnableWriteCalls.load(std::memory_order_relaxed);
    }
    if (lastRxCntWrite) *lastRxCntWrite = g_mpLastRxCntWrite.load(std::memory_order_relaxed);
    if (lastRxCntLatched) *lastRxCntLatched = g_mpLastRxCntLatched.load(std::memory_order_relaxed);
    if (lastWifiRegWriteAddr) *lastWifiRegWriteAddr = g_mpLastWifiRegWriteAddr.load(std::memory_order_relaxed);
    if (lastWifiRegWriteValue) *lastWifiRegWriteValue = g_mpLastWifiRegWriteValue.load(std::memory_order_relaxed);
    if (lastWifiRegWriteCpu) *lastWifiRegWriteCpu = g_mpLastWifiRegWriteCpu.load(std::memory_order_relaxed);
    if (lastWifiRegWritePc) *lastWifiRegWritePc = g_mpLastWifiRegWritePc.load(std::memory_order_relaxed);
    if (wifiRegReadCalls) *wifiRegReadCalls = g_mpWifiRegReadCalls.load(std::memory_order_relaxed);
    if (lastWifiRegReadAddr) *lastWifiRegReadAddr = g_mpLastWifiRegReadAddr.load(std::memory_order_relaxed);
    if (lastWifiRegReadValue) *lastWifiRegReadValue = g_mpLastWifiRegReadValue.load(std::memory_order_relaxed);
    if (lastWifiRegReadCpu) *lastWifiRegReadCpu = g_mpLastWifiRegReadCpu.load(std::memory_order_relaxed);
    if (lastWifiRegReadPc) *lastWifiRegReadPc = g_mpLastWifiRegReadPc.load(std::memory_order_relaxed);
    if (txReqSetWriteCalls) *txReqSetWriteCalls = g_mpTxReqSetWriteCalls.load(std::memory_order_relaxed);
    if (lastTxReqSetWrite) *lastTxReqSetWrite = g_mpLastTxReqSetWrite.load(std::memory_order_relaxed);
    if (lastTxReqSetWriteCpu) {
        *lastTxReqSetWriteCpu = g_mpLastTxReqSetWriteCpu.load(std::memory_order_relaxed);
    }
    if (lastTxReqSetWritePc) *lastTxReqSetWritePc = g_mpLastTxReqSetWritePc.load(std::memory_order_relaxed);
    if (lastTxReqSetSlotLoc1) *lastTxReqSetSlotLoc1 = g_mpLastTxReqSetSlotLoc1.load(std::memory_order_relaxed);
    if (lastTxReqSetSlotCmd) *lastTxReqSetSlotCmd = g_mpLastTxReqSetSlotCmd.load(std::memory_order_relaxed);
    if (lastTxReqSetSlotLoc2) *lastTxReqSetSlotLoc2 = g_mpLastTxReqSetSlotLoc2.load(std::memory_order_relaxed);
    if (lastTxReqSetSlotLoc3) *lastTxReqSetSlotLoc3 = g_mpLastTxReqSetSlotLoc3.load(std::memory_order_relaxed);
    if (txSlotWriteCalls) *txSlotWriteCalls = g_mpTxSlotWriteCalls.load(std::memory_order_relaxed);
    if (lastTxSlotWriteAddr) *lastTxSlotWriteAddr = g_mpLastTxSlotWriteAddr.load(std::memory_order_relaxed);
    if (lastTxSlotWriteValue) *lastTxSlotWriteValue = g_mpLastTxSlotWriteValue.load(std::memory_order_relaxed);
    if (lastTxSlotWriteCpu) *lastTxSlotWriteCpu = g_mpLastTxSlotWriteCpu.load(std::memory_order_relaxed);
    if (lastTxSlotWritePc) *lastTxSlotWritePc = g_mpLastTxSlotWritePc.load(std::memory_order_relaxed);
}

extern "C" PUBLIC_SYMBOL void melondsds_get_mp_setup_diagnostics(int32_t *lastTxSlotLoc1,
                                                                  int32_t *lastTxSlotCmd,
                                                                  int32_t *lastTxSlotLoc2,
                                                                  int32_t *lastTxSlotLoc3,
                                                                  int32_t *txSlotCmdWriteCalls,
                                                                  int32_t *lastTxSlotCmdRawWrite,
                                                                  int32_t *lastTxSlotCmdLatched,
                                                                  int32_t *lastTxSlotCmdCounter,
                                                                  int32_t *txSlotCmdBlockedByCounterCalls,
                                                                  int32_t *modeResetWriteCalls,
                                                                  int32_t *lastModeResetWrite,
                                                                  int32_t *lastModeReset,
                                                                  int32_t *modeWepWriteCalls,
                                                                  int32_t *lastModeWep,
                                                                  int32_t *powerStateWriteCalls,
                                                                  int32_t *powerStateIgnoredWriteCalls,
                                                                  int32_t *lastPowerStateWrite,
                                                                  int32_t *lastPowerState,
                                                                  int32_t *lastRxBufBegin,
                                                                  int32_t *lastRxBufEnd,
                                                                  int32_t *rxBufWriteAddrWriteCalls,
                                                                  int32_t *lastRxBufWriteAddr,
                                                                  int32_t *txBufWriteAddrWriteCalls,
                                                                  int32_t *lastTxBufWriteAddr,
                                                                  int32_t *txBufCountWriteCalls,
                                                                  int32_t *lastTxBufCount,
                                                                  int32_t *txBufDataWriteCalls,
                                                                  int32_t *lastTxBufDataWriteAddr,
                                                                  int32_t *lastTxBufDataWriteValue,
                                                                  int32_t *lastTxBufDataWriteNextAddr,
                                                                  int32_t *lastTxBufDataWriteRemaining,
                                                                  int32_t *lastRfRaw1,
                                                                  int32_t *lastRfRaw2,
                                                                  int32_t *lastRfIndex1,
                                                                  int32_t *lastRfIndex2,
                                                                  int32_t *rfValidChannelSnapshots,
                                                                  int32_t *rfRaw1MatchChannel,
                                                                  int32_t *rfRaw2MatchChannel,
                                                                  int32_t *rfClosestChannel,
                                                                  int32_t *rfClosestRaw1,
                                                                  int32_t *rfClosestRaw2) {
    if (lastTxSlotLoc1) *lastTxSlotLoc1 = g_mpLastTxSlotLoc1.load(std::memory_order_relaxed);
    if (lastTxSlotCmd) *lastTxSlotCmd = g_mpLastTxSlotCmd.load(std::memory_order_relaxed);
    if (lastTxSlotLoc2) *lastTxSlotLoc2 = g_mpLastTxSlotLoc2.load(std::memory_order_relaxed);
    if (lastTxSlotLoc3) *lastTxSlotLoc3 = g_mpLastTxSlotLoc3.load(std::memory_order_relaxed);
    if (txSlotCmdWriteCalls) {
        *txSlotCmdWriteCalls = g_mpTxSlotCmdWriteCalls.load(std::memory_order_relaxed);
    }
    if (lastTxSlotCmdRawWrite) {
        *lastTxSlotCmdRawWrite = g_mpLastTxSlotCmdRawWrite.load(std::memory_order_relaxed);
    }
    if (lastTxSlotCmdLatched) {
        *lastTxSlotCmdLatched = g_mpLastTxSlotCmdLatched.load(std::memory_order_relaxed);
    }
    if (lastTxSlotCmdCounter) {
        *lastTxSlotCmdCounter = g_mpLastTxSlotCmdCounter.load(std::memory_order_relaxed);
    }
    if (txSlotCmdBlockedByCounterCalls) {
        *txSlotCmdBlockedByCounterCalls = g_mpTxSlotCmdBlockedByCounterCalls.load(std::memory_order_relaxed);
    }
    if (modeResetWriteCalls) *modeResetWriteCalls = g_mpModeResetWriteCalls.load(std::memory_order_relaxed);
    if (lastModeResetWrite) *lastModeResetWrite = g_mpLastModeResetWrite.load(std::memory_order_relaxed);
    if (lastModeReset) *lastModeReset = g_mpLastModeReset.load(std::memory_order_relaxed);
    if (modeWepWriteCalls) *modeWepWriteCalls = g_mpModeWepWriteCalls.load(std::memory_order_relaxed);
    if (lastModeWep) *lastModeWep = g_mpLastModeWep.load(std::memory_order_relaxed);
    if (powerStateWriteCalls) *powerStateWriteCalls = g_mpPowerStateWriteCalls.load(std::memory_order_relaxed);
    if (powerStateIgnoredWriteCalls) {
        *powerStateIgnoredWriteCalls = g_mpPowerStateIgnoredWriteCalls.load(std::memory_order_relaxed);
    }
    if (lastPowerStateWrite) *lastPowerStateWrite = g_mpLastPowerStateWrite.load(std::memory_order_relaxed);
    if (lastPowerState) *lastPowerState = g_mpLastPowerState.load(std::memory_order_relaxed);
    if (lastRxBufBegin) *lastRxBufBegin = g_mpLastRxBufBegin.load(std::memory_order_relaxed);
    if (lastRxBufEnd) *lastRxBufEnd = g_mpLastRxBufEnd.load(std::memory_order_relaxed);
    if (rxBufWriteAddrWriteCalls) {
        *rxBufWriteAddrWriteCalls = g_mpRxBufWriteAddrWriteCalls.load(std::memory_order_relaxed);
    }
    if (lastRxBufWriteAddr) *lastRxBufWriteAddr = g_mpLastRxBufWriteAddr.load(std::memory_order_relaxed);
    if (txBufWriteAddrWriteCalls) {
        *txBufWriteAddrWriteCalls = g_mpTxBufWriteAddrWriteCalls.load(std::memory_order_relaxed);
    }
    if (lastTxBufWriteAddr) *lastTxBufWriteAddr = g_mpLastTxBufWriteAddr.load(std::memory_order_relaxed);
    if (txBufCountWriteCalls) *txBufCountWriteCalls = g_mpTxBufCountWriteCalls.load(std::memory_order_relaxed);
    if (lastTxBufCount) *lastTxBufCount = g_mpLastTxBufCount.load(std::memory_order_relaxed);
    if (txBufDataWriteCalls) *txBufDataWriteCalls = g_mpTxBufDataWriteCalls.load(std::memory_order_relaxed);
    if (lastTxBufDataWriteAddr) {
        *lastTxBufDataWriteAddr = g_mpLastTxBufDataWriteAddr.load(std::memory_order_relaxed);
    }
    if (lastTxBufDataWriteValue) {
        *lastTxBufDataWriteValue = g_mpLastTxBufDataWriteValue.load(std::memory_order_relaxed);
    }
    if (lastTxBufDataWriteNextAddr) {
        *lastTxBufDataWriteNextAddr = g_mpLastTxBufDataWriteNextAddr.load(std::memory_order_relaxed);
    }
    if (lastTxBufDataWriteRemaining) {
        *lastTxBufDataWriteRemaining = g_mpLastTxBufDataWriteRemaining.load(std::memory_order_relaxed);
    }
    if (lastRfRaw1) *lastRfRaw1 = g_mpLastRfRaw1.load(std::memory_order_relaxed);
    if (lastRfRaw2) *lastRfRaw2 = g_mpLastRfRaw2.load(std::memory_order_relaxed);
    if (lastRfIndex1) *lastRfIndex1 = g_mpLastRfIndex1.load(std::memory_order_relaxed);
    if (lastRfIndex2) *lastRfIndex2 = g_mpLastRfIndex2.load(std::memory_order_relaxed);
    if (rfValidChannelSnapshots) {
        *rfValidChannelSnapshots = g_mpRfValidChannelSnapshots.load(std::memory_order_relaxed);
    }
    if (rfRaw1MatchChannel) *rfRaw1MatchChannel = g_mpRfRaw1MatchChannel.load(std::memory_order_relaxed);
    if (rfRaw2MatchChannel) *rfRaw2MatchChannel = g_mpRfRaw2MatchChannel.load(std::memory_order_relaxed);
    if (rfClosestChannel) *rfClosestChannel = g_mpRfClosestChannel.load(std::memory_order_relaxed);
    if (rfClosestRaw1) *rfClosestRaw1 = g_mpRfClosestRaw1.load(std::memory_order_relaxed);
    if (rfClosestRaw2) *rfClosestRaw2 = g_mpRfClosestRaw2.load(std::memory_order_relaxed);
}

extern "C" PUBLIC_SYMBOL void melondsds_get_mp_wifi_reg_write_trace(char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mpWifiRegWriteTraceMutex);
    strlcpy(buffer, g_mpWifiRegWriteTraceTail, bufferSize);
}

extern "C" PUBLIC_SYMBOL void melondsds_reset_mp_diagnostics(void) {
    resetMpDiagnostics();
}

extern "C" void MelonDsDs::HardwareContextReset() noexcept {
    try {
        Core.ResetRenderState();
    }
    catch (const opengl_exception& e) {
        retro::error("{}", e.what());
        retro::set_error_message(e.user_message());
        retro::shutdown();
        // TODO: Instead of shutting down, fall back to the software renderer
    }
    catch (const emulator_exception& e) {
        retro::error("{}", e.what());
        retro::set_error_message(e.user_message());
        retro::shutdown();
    }
    catch (const std::exception& e) {
        retro::set_error_message(e.what());
        retro::shutdown();
    }
    catch (...) {
        retro::set_error_message("OpenGL context initialization failed with an unknown error. Please report this issue.");
        retro::shutdown();
    }
}

extern "C" void MelonDsDs::HardwareContextDestroyed() noexcept {
    Core.DestroyRenderState();
}

extern "C" bool MelonDsDs::UpdateOptionVisibility() noexcept {
    return Core.UpdateOptionVisibility();
}

int Platform::Net_SendPacket(u8* data, int len, void*) {
    ZoneScopedN(TracyFunction);

    return MelonDsDs::Core.LanSendPacket(std::span((std::byte*)data, len));
}

int Platform::Net_RecvPacket(u8* data, void*) {
    ZoneScopedN(TracyFunction);

    return MelonDsDs::Core.LanRecvPacket(data);
}

void Platform::WriteNDSSave(const u8 *savedata, u32 savelen, u32 writeoffset, u32 writelen, void*) {
    ZoneScopedN(TracyFunction);

    MelonDsDs::Core.WriteNdsSave(span((const std::byte*)savedata, savelen), writeoffset, writelen);
}

void Platform::WriteGBASave(const u8 *savedata, u32 savelen, u32 writeoffset, u32 writelen, void*) {
    ZoneScopedN(TracyFunction);

    MelonDsDs::Core.WriteGbaSave(span((const std::byte*)savedata, savelen), writeoffset, writelen);
}

void Platform::WriteFirmware(const Firmware& firmware, u32 writeoffset, u32 writelen, void*) {
    ZoneScopedN(TracyFunction);

    MelonDsDs::Core.WriteFirmware(firmware, writeoffset, writelen);
}

extern "C" void MelonDsDs::MpStarted(uint16_t client_id, retro_netpacket_send_t send_fn, retro_netpacket_poll_receive_t poll_receive_fn) noexcept {
    MelonDsDs::Core.MpStarted(send_fn, poll_receive_fn);
}

extern "C" void MelonDsDs::MpReceived(const void* buf, size_t len, uint16_t client_id) noexcept {
    MelonDsDs::Core.MpPacketReceived(buf, len, client_id);
}

extern "C" void MelonDsDs::MpStopped() noexcept {
    MelonDsDs::Core.MpStopped();
}

int DeconstructPacket(u8 *data, u64 *timestamp, const std::optional<MelonDsDs::Packet> &o_p) {
    if (!o_p.has_value()) {
        return 0;
    }
    memcpy(data, o_p->Data(), o_p->Length());
    *timestamp = o_p->Timestamp();
    return o_p->Length();
}

int Platform::MP_SendPacket(u8* data, int len, u64 timestamp, void*) {
    g_mpSendCalls.fetch_add(1, std::memory_order_relaxed);
    return MelonDsDs::Core.MpSendPacket(MelonDsDs::Packet(data, len, timestamp, 0, MelonDsDs::Packet::Type::Other)) ? len : 0;
}

int Platform::MP_RecvPacket(u8* data, u64* timestamp, void*) {
    g_mpReceiveCalls.fetch_add(1, std::memory_order_relaxed);
    std::optional<MelonDsDs::Packet> o_p = MelonDsDs::Core.MpNextPacket();
    return DeconstructPacket(data, timestamp, o_p);
}

int Platform::MP_SendCmd(u8* data, int len, u64 timestamp, void*) {
    g_mpSendCalls.fetch_add(1, std::memory_order_relaxed);
    return MelonDsDs::Core.MpSendPacket(MelonDsDs::Packet(data, len, timestamp, 0, MelonDsDs::Packet::Type::Cmd)) ? len : 0;
}

int Platform::MP_SendReply(u8 *data, int len, u64 timestamp, u16 aid, void*) {
    g_mpSendCalls.fetch_add(1, std::memory_order_relaxed);
    // aid is always less than 16,
    // otherwise sending a 16-bit wide aidmask in RecvReplies wouldn't make sense,
    // and neither would this line[1] from melonDS itself.
    // A blog post from melonDS[2] from 2017 also confirms that
    // "each client is given an ID from 1 to 15"
    // [1] https://github.com/melonDS-emu/melonDS/blob/817b409ec893fb0b2b745ee18feced08706419de/src/net/LAN.cpp#L1074
    // [2] https://melonds.kuribo64.net/comments.php?id=25
    retro_assert(aid < 16);
    return MelonDsDs::Core.MpSendPacket(MelonDsDs::Packet(data, len, timestamp, aid, MelonDsDs::Packet::Type::Reply)) ? len : 0;
}

int Platform::MP_SendAck(u8* data, int len, u64 timestamp, void*) {
    g_mpSendCalls.fetch_add(1, std::memory_order_relaxed);
    return MelonDsDs::Core.MpSendPacket(MelonDsDs::Packet(data, len, timestamp, 0, MelonDsDs::Packet::Type::Cmd)) ? len : 0;
}

int Platform::MP_RecvHostPacket(u8* data, u64 * timestamp, void*) {
    g_mpReceiveCalls.fetch_add(1, std::memory_order_relaxed);
    std::optional<MelonDsDs::Packet> o_p = MelonDsDs::Core.MpNextPacketBlock();
    return DeconstructPacket(data, timestamp, o_p);
}

u16 Platform::MP_RecvReplies(u8* packets, u64 timestamp, u16 aidmask, void*) {
    g_mpReceiveCalls.fetch_add(1, std::memory_order_relaxed);
    if(!MelonDsDs::Core.MpActive()) {
        return 0;
    }
    u16 ret = 0;
    int loops = 0;
    while((ret & aidmask) != aidmask) {
        std::optional<MelonDsDs::Packet> o_p = MelonDsDs::Core.MpNextPacketBlock();
        if(!o_p.has_value()) {
            return ret;
        }
        MelonDsDs::Packet p = std::move(o_p).value();
        if(p.Timestamp() < (timestamp - 32)) {
            continue;
        }
        if(p.PacketType() != MelonDsDs::Packet::Type::Reply) {
            continue;
        }
        ret |= 1<<p.Aid();
        memcpy(&packets[(p.Aid()-1)*1024], p.Data(), std::min(p.Length(), (uint64_t)1024));
        loops++;
    }
    return ret;
}
