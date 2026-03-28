#pragma once

#include "uds_types.h"
#include "did_store.h"
#include "dtc_state_machine.h"
#include "can_socket.h"

#include <vector>
#include <memory>
#include <atomic>
#include <functional>

namespace diag {

// ============================================================
// UdsServer listens on a CAN socket and responds to UDS
// diagnostic requests like a real ECU would.
//
// Supported services:
//   0x22 — ReadDataByIdentifier  (read VIN, SW version, etc.)
//   0x14 — ClearDtcInformation   (clear all fault codes)
//   0x19 — ReadDtcInformation    (read active fault codes)
//   0x27 — SecurityAccess        (seed/key unlock, simplified)
// ============================================================
class UdsServer {
public:
    static constexpr uint32_t TESTER_REQUEST_ID  = 0x7DF; // broadcast
    static constexpr uint32_t ECU_RESPONSE_ID    = 0x7E8; // our response ID

    UdsServer(const std::string& canInterface,
              std::vector<std::shared_ptr<DtcStateMachine>> dtcs);

    // Run the server — blocks and processes messages until stop() is called
    void run();

    // Call from another thread or signal handler to stop the server
    void stop();

    // Inject a DTC fault (for demo / testing purposes)
    void injectFault(uint32_t dtcCode);

    // Optional: log callback so the caller can print decoded messages
    using LogCallback = std::function<void(const std::string& msg)>;
    void setLogCallback(LogCallback cb);

private:
    // Process one incoming UDS message and return a response
    UdsResponse processRequest(const UdsMessage& req);

    // Handlers for each service — one function per service ID
    UdsResponse handleReadDataByIdentifier(const UdsMessage& req);
    UdsResponse handleReadDtcInformation(const UdsMessage& req);
    UdsResponse handleClearDtcInformation(const UdsMessage& req);
    UdsResponse handleSecurityAccess(const UdsMessage& req);

    // Send a response frame on the CAN bus
    void sendResponse(const UdsResponse& resp);

    // Internal log helper
    void log(const std::string& msg);

    CanSocket                                    m_socket;
    DidStore                                     m_didStore;
    std::vector<std::shared_ptr<DtcStateMachine>> m_dtcs;
    std::atomic<bool>                            m_running;
    LogCallback                                  m_logCb;

    // Security access state (simplified seed/key)
    bool     m_securityUnlocked = false;
    uint32_t m_lastSeed         = 0;
};

} // namespace diag