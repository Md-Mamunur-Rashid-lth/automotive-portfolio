#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace diag {

// ============================================================
// UDS Service IDs (ISO 14229-1)
// These are the "verbs" of the UDS language.
// A tester sends one of these to ask the ECU to do something.
// ============================================================
enum class ServiceId : uint8_t {
    READ_DATA_BY_IDENTIFIER  = 0x22,  // Read a named piece of data
    CLEAR_DTC_INFORMATION    = 0x14,  // Clear fault codes
    READ_DTC_INFORMATION     = 0x19,  // Read fault codes
    SECURITY_ACCESS          = 0x27,  // Unlock protected functions
    ECU_RESET                = 0x11,  // Restart the ECU
};

// ============================================================
// UDS Negative Response Codes (ISO 14229-1 Table A.1)
// When the ECU can't fulfil a request, it sends one of these
// back instead of the normal response. Like HTTP error codes.
// ============================================================
enum class NrcCode : uint8_t {
    GENERAL_REJECT                  = 0x10,
    SERVICE_NOT_SUPPORTED           = 0x11,
    SUB_FUNCTION_NOT_SUPPORTED      = 0x12,
    INCORRECT_MESSAGE_LENGTH        = 0x13,
    REQUEST_OUT_OF_RANGE            = 0x31,
    SECURITY_ACCESS_DENIED          = 0x33,
    REQUEST_CORRECTLY_RCVD_RSP_PEND = 0x78, // "I'm working on it, wait"
    CONDITIONS_NOT_CORRECT          = 0x22,
};

// ============================================================
// A raw UDS message — what travels over CAN
// ============================================================
struct UdsMessage {
    uint32_t             canId;   // who sent this (0x7DF = tester broadcast)
    std::vector<uint8_t> payload; // the actual bytes

    // Convenience: get the service ID from byte 1
    // Byte 0 = length, byte 1 = service ID (UDS PDU format)
    ServiceId serviceId() const {
        if (payload.size() >= 2)
            return static_cast<ServiceId>(payload[1]);
        return static_cast<ServiceId>(0x00);
    }

    // Get the data bytes after the service ID
    std::vector<uint8_t> data() const {
        if (payload.size() > 2)
            return std::vector<uint8_t>(payload.begin() + 2, payload.end());
        return {};
    }
};

// ============================================================
// A UDS response — what the ECU sends back
// ============================================================
struct UdsResponse {
    bool                 isPositive; // true = success, false = negative resp
    std::vector<uint8_t> payload;    // the response bytes to put on the bus

    // Factory: build a negative response frame
    // NRC = Negative Response Code
    static UdsResponse negative(ServiceId requestedService, NrcCode nrc) {
        UdsResponse r;
        r.isPositive = false;
        r.payload = {
            0x03,                          // length = 3 bytes follow
            0x7F,                          // 0x7F always means "negative response"
            static_cast<uint8_t>(requestedService),
            static_cast<uint8_t>(nrc)
        };
        return r;
    }
};

// ============================================================
// Known DIDs (Data Identifiers) — ISO 14229 + OEM-defined
// A DID is a 16-bit address for a named piece of ECU data.
// 0xF1xx DIDs are standardised across all manufacturers.
// ============================================================
namespace Did {
    constexpr uint16_t VIN                    = 0xF190; // Vehicle ID Number
    constexpr uint16_t ECU_SERIAL_NUMBER      = 0xF18C;
    constexpr uint16_t SOFTWARE_VERSION       = 0xF189;
    constexpr uint16_t ECU_SUPPLIER           = 0xF18A;
    constexpr uint16_t ACTIVE_DIAGNOSTIC_INFO = 0xF186;
}

}