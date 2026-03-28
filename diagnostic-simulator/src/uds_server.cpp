#include "uds_server.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>   // rand()
#include <ctime>     // time()

namespace diag {

// ---- Constructor --------------------------------------------------------

UdsServer::UdsServer(const std::string& canInterface,
                     std::vector<std::shared_ptr<DtcStateMachine>> dtcs)
    : m_socket(canInterface)
    , m_dtcs(std::move(dtcs))
    , m_running(false)
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

// ---- Public API ---------------------------------------------------------

void UdsServer::setLogCallback(LogCallback cb) {
    m_logCb = cb;
}

void UdsServer::stop() {
    m_running = false;
}

void UdsServer::injectFault(uint32_t dtcCode) {
    for (auto& dtc : m_dtcs) {
        if (dtc->getDtcCode() == dtcCode) {
            dtc->reportFault();
            dtc->reportFault(); // twice = confirmed
            log("Injected fault: DTC 0x" +
                [dtcCode]{ std::ostringstream s;
                           s << std::hex << dtcCode; return s.str(); }());
            return;
        }
    }
    log("No DTC found with code 0x" +
        [dtcCode]{ std::ostringstream s;
                   s << std::hex << dtcCode; return s.str(); }());
}

// ---- Main loop ----------------------------------------------------------

void UdsServer::run() {
    if (!m_socket.isOpen()) {
        log("Cannot run: CAN socket not open");
        return;
    }

    m_running = true;
    log("UDS server running on — waiting for requests...");
    log("Tester request ID : 0x7DF");
    log("ECU response ID   : 0x7E8");

    while (m_running) {
        uint32_t rxId;
        std::vector<uint8_t> rxData;

        // Block here waiting for a CAN frame
        if (!m_socket.receive(rxId, rxData)) {
            continue; // receive error — try again
        }

        // Only process frames addressed to us
        // 0x7DF = broadcast (for all ECUs)
        // 0x7E0 = physical address (for our ECU specifically)
        if (rxId != TESTER_REQUEST_ID && rxId != 0x7E0) {
            continue; // not for us
        }

        // Build a UdsMessage from the raw frame
        UdsMessage req;
        req.canId   = rxId;
        req.payload = rxData;

        // Log what we received
        std::ostringstream rxLog;
        rxLog << "RX [0x" << std::hex << std::uppercase
              << std::setw(3) << std::setfill('0') << rxId << "] ";
        for (auto b : rxData)
            rxLog << std::setw(2) << std::setfill('0') << (int)b << " ";
        log(rxLog.str());

        // Process the request and get a response
        UdsResponse resp = processRequest(req);

        // Send the response back on the bus
        sendResponse(resp);
    }

    log("UDS server stopped.");
}

// ---- Request router -----------------------------------------------------

UdsResponse UdsServer::processRequest(const UdsMessage& req) {
    if (req.payload.size() < 2) {
        return UdsResponse::negative(static_cast<ServiceId>(0x00),
                                     NrcCode::INCORRECT_MESSAGE_LENGTH);
    }

    // Route by service ID — byte 1 of the UDS PDU
    switch (req.serviceId()) {
        case ServiceId::READ_DATA_BY_IDENTIFIER:
            return handleReadDataByIdentifier(req);
        case ServiceId::READ_DTC_INFORMATION:
            return handleReadDtcInformation(req);
        case ServiceId::CLEAR_DTC_INFORMATION:
            return handleClearDtcInformation(req);
        case ServiceId::SECURITY_ACCESS:
            return handleSecurityAccess(req);
        default:
            log("Unknown service: 0x" +
                [&]{ std::ostringstream s;
                     s << std::hex << (int)req.payload[1]; return s.str(); }());
            return UdsResponse::negative(req.serviceId(),
                                         NrcCode::SERVICE_NOT_SUPPORTED);
    }
}

// ---- 0x22 ReadDataByIdentifier ------------------------------------------
// Tester sends: [len] [0x22] [DID_high] [DID_low] [padding...]
// ECU responds: [len] [0x62] [DID_high] [DID_low] [data bytes...]
// 0x62 = 0x22 + 0x40 (positive response = service ID + 0x40, always)

UdsResponse UdsServer::handleReadDataByIdentifier(const UdsMessage& req) {
    // Need at least: length byte + service ID + 2 DID bytes = 4 bytes
    if (req.payload.size() < 4) {
        return UdsResponse::negative(ServiceId::READ_DATA_BY_IDENTIFIER,
                                     NrcCode::INCORRECT_MESSAGE_LENGTH);
    }

    // Extract the 16-bit DID from bytes 2 and 3
    uint16_t did = (static_cast<uint16_t>(req.payload[2]) << 8)
                 |  static_cast<uint16_t>(req.payload[3]);

    log("  Service 0x22: Read DID 0x" +
        [did]{ std::ostringstream s;
               s << std::hex << std::uppercase << did; return s.str(); }());

    // Look up the DID in our store
    auto value = m_didStore.read(did);
    if (!value.has_value()) {
        log("  DID not found -> NRC 0x31");
        return UdsResponse::negative(ServiceId::READ_DATA_BY_IDENTIFIER,
                                     NrcCode::REQUEST_OUT_OF_RANGE);
    }

    // Build positive response:
    // [total_length] [0x62] [DID_high] [DID_low] [data...]
    std::vector<uint8_t> respPayload;
    uint8_t dataLen = static_cast<uint8_t>(3 + value->size()); // 0x62 + DID + data
    respPayload.push_back(dataLen);
    respPayload.push_back(0x62);             // positive response SID
    respPayload.push_back((did >> 8) & 0xFF);
    respPayload.push_back(did & 0xFF);
    for (auto b : *value)
        respPayload.push_back(b);

    // Log the response nicely
    std::string valStr(value->begin(), value->end());
    log("  -> Responding with: \"" + valStr + "\"");

    UdsResponse resp;
    resp.isPositive = true;
    resp.payload    = respPayload;
    return resp;
}

// ---- 0x19 ReadDtcInformation --------------------------------------------
// Sub-function 0x02 = reportDtcByStatusMask
// Tester sends: [len] [0x19] [0x02] [status_mask]
// ECU responds: [len] [0x59] [0x02] [availability_mask]
//               then 4 bytes per DTC: [dtc_high] [dtc_mid] [dtc_low] [status]

UdsResponse UdsServer::handleReadDtcInformation(const UdsMessage& req) {
    if (req.payload.size() < 3) {
        return UdsResponse::negative(ServiceId::READ_DTC_INFORMATION,
                                     NrcCode::INCORRECT_MESSAGE_LENGTH);
    }

    uint8_t subFunction = req.payload[2];
    log("  Service 0x19 sub-function: 0x" +
        [subFunction]{ std::ostringstream s;
                       s << std::hex << (int)subFunction; return s.str(); }());

    // We support sub-function 0x02: reportDtcByStatusMask
    if (subFunction != 0x02) {
        return UdsResponse::negative(ServiceId::READ_DTC_INFORMATION,
                                     NrcCode::SUB_FUNCTION_NOT_SUPPORTED);
    }

    // Collect all confirmed DTCs
    std::vector<std::shared_ptr<DtcStateMachine>> confirmedDtcs;
    for (auto& dtc : m_dtcs) {
        if (dtc->getState() == DtcState::CONFIRMED) {
            confirmedDtcs.push_back(dtc);
        }
    }

    log("  Found " + std::to_string(confirmedDtcs.size()) + " confirmed DTC(s)");

    // Build the response payload
    std::vector<uint8_t> respPayload;
    respPayload.push_back(0x00);  // length placeholder (fill in at end)
    respPayload.push_back(0x59);  // positive response SID for 0x19
    respPayload.push_back(0x02);  // echo sub-function
    respPayload.push_back(0xFF);  // DTC status availability mask (all bits)

    // Append each DTC: 3 bytes for the code + 1 byte status
    for (auto& dtc : confirmedDtcs) {
        uint32_t code = dtc->getDtcCode();
        respPayload.push_back((code >> 16) & 0xFF); // high byte
        respPayload.push_back((code >>  8) & 0xFF); // mid byte
        respPayload.push_back( code        & 0xFF); // low byte
        respPayload.push_back(0x08);                // status: confirmed bit set
    }

    // Fill in the actual length byte now we know the payload size
    respPayload[0] = static_cast<uint8_t>(respPayload.size() - 1);

    UdsResponse resp;
    resp.isPositive = true;
    resp.payload    = respPayload;
    return resp;
}

// ---- 0x14 ClearDtcInformation -------------------------------------------
// Tester sends: [len] [0x14] [group_high] [group_mid] [group_low]
// 0xFFFFFF as group = clear ALL DTCs
// ECU responds: [0x01] [0x54]  (simple positive ack)

UdsResponse UdsServer::handleClearDtcInformation(const UdsMessage& req) {
    if (req.payload.size() < 5) {
        return UdsResponse::negative(ServiceId::CLEAR_DTC_INFORMATION,
                                     NrcCode::INCORRECT_MESSAGE_LENGTH);
    }

    // Read the DTC group (3 bytes)
    uint32_t group = (static_cast<uint32_t>(req.payload[2]) << 16)
                   | (static_cast<uint32_t>(req.payload[3]) <<  8)
                   |  static_cast<uint32_t>(req.payload[4]);

    log("  Service 0x14: Clear DTC group 0x" +
        [group]{ std::ostringstream s;
                 s << std::hex << std::uppercase << group; return s.str(); }());

    // 0xFFFFFF means "clear all groups"
    if (group == 0xFFFFFF) {
        for (auto& dtc : m_dtcs) {
            dtc->clearDtc();
        }
        log("  Cleared all " + std::to_string(m_dtcs.size()) + " DTC(s)");
    }

    // Positive response: just two bytes
    UdsResponse resp;
    resp.isPositive = true;
    resp.payload    = {0x01, 0x54};  // length=1, positive response SID
    return resp;
}

// ---- 0x27 SecurityAccess ------------------------------------------------
// A real seed/key exchange (simplified here)
// Sub-function 0x01 = request seed
// Sub-function 0x02 = send key
// Our "key" algorithm: key = seed XOR 0xDEAD (simplified, real ECUs use AES)

UdsResponse UdsServer::handleSecurityAccess(const UdsMessage& req) {
    if (req.payload.size() < 3) {
        return UdsResponse::negative(ServiceId::SECURITY_ACCESS,
                                     NrcCode::INCORRECT_MESSAGE_LENGTH);
    }

    uint8_t subFunc = req.payload[2];

    if (subFunc == 0x01) {
        // Tester is requesting a seed
        // Generate a random 2-byte seed
        m_lastSeed = static_cast<uint32_t>(std::rand() % 0xFFFF);
        log("  Service 0x27: Sending seed 0x" +
            [this]{ std::ostringstream s;
                    s << std::hex << m_lastSeed; return s.str(); }());

        UdsResponse resp;
        resp.isPositive = true;
        resp.payload = {
            0x04,          // length
            0x67,          // positive response SID for 0x27
            0x01,          // echo sub-function
            static_cast<uint8_t>((m_lastSeed >> 8) & 0xFF),
            static_cast<uint8_t>( m_lastSeed        & 0xFF)
        };
        return resp;
    }

    if (subFunc == 0x02) {
        // Tester is sending the key — validate it
        if (req.payload.size() < 5) {
            return UdsResponse::negative(ServiceId::SECURITY_ACCESS,
                                         NrcCode::INCORRECT_MESSAGE_LENGTH);
        }

        uint16_t receivedKey = (static_cast<uint16_t>(req.payload[3]) << 8)
                             |  static_cast<uint16_t>(req.payload[4]);

        // Our algorithm: expected key = seed XOR 0xDEAD
        uint16_t expectedKey = static_cast<uint16_t>(m_lastSeed ^ 0xDEAD);

        if (receivedKey == expectedKey) {
            m_securityUnlocked = true;
            log("  Security access GRANTED");
            UdsResponse resp;
            resp.isPositive = true;
            resp.payload = {0x02, 0x67, 0x02}; // positive ack
            return resp;
        } else {
            m_securityUnlocked = false;
            log("  Security access DENIED (wrong key)");
            return UdsResponse::negative(ServiceId::SECURITY_ACCESS,
                                         NrcCode::SECURITY_ACCESS_DENIED);
        }
    }

    return UdsResponse::negative(ServiceId::SECURITY_ACCESS,
                                 NrcCode::SUB_FUNCTION_NOT_SUPPORTED);
}

// ---- sendResponse -------------------------------------------------------

void UdsServer::sendResponse(const UdsResponse& resp) {
    // Pad response to 8 bytes (CAN classic frame is always 8 bytes)
    std::vector<uint8_t> frame = resp.payload;
    while (frame.size() < 8)
        frame.push_back(0xAA); // 0xAA = padding byte (industry convention)

    m_socket.send(ECU_RESPONSE_ID, frame);

    // Log what we sent
    std::ostringstream txLog;
    txLog << "TX [0x" << std::hex << std::uppercase
          << std::setw(3) << std::setfill('0') << ECU_RESPONSE_ID << "] ";
    for (auto b : frame)
        txLog << std::setw(2) << std::setfill('0') << (int)b << " ";
    log(txLog.str());
}

// ---- log ----------------------------------------------------------------

void UdsServer::log(const std::string& msg) {
    if (m_logCb)
        m_logCb(msg);
    else
        std::cout << msg << "\n";
}

} // namespace diag