#include "did_store.h"
#include "uds_types.h"  // for Did:: constants

namespace diag {

DidStore::DidStore() {
    // Pre-load realistic automotive data
    // In a real ECU these come from flash / NVM at startup

    // VIN: Vehicle Identification Number — always exactly 17 ASCII chars
    writeString(Did::VIN, "YV1PORTFOLIO12345");

    // ECU serial number
    writeString(Did::ECU_SERIAL_NUMBER, "ECU-SIM-001");

    // Software version — format typical of Volvo/Geely supplier ECUs
    writeString(Did::SOFTWARE_VERSION, "SW_v1.0.0-demo");

    // ECU supplier name
    writeString(Did::ECU_SUPPLIER, "Anthropic Embedded");

    // Active diagnostic session (0x01 = default session)
    write(Did::ACTIVE_DIAGNOSTIC_INFO, {0x01});
}

void DidStore::write(uint16_t did, const std::vector<uint8_t>& value) {
    m_store[did] = value;
}

void DidStore::writeString(uint16_t did, const std::string& value) {
    // Convert string characters to bytes (ASCII)
    write(did, std::vector<uint8_t>(value.begin(), value.end()));
}

std::optional<std::vector<uint8_t>> DidStore::read(uint16_t did) const {
    auto it = m_store.find(did);
    if (it == m_store.end())
        return std::nullopt;  // DID not found
    return it->second;
}

bool DidStore::exists(uint16_t did) const {
    return m_store.count(did) > 0;
}

}