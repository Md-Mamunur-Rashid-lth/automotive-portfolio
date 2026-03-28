#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace diag {

// ============================================================
// DidStore holds key-value pairs where:
//   key   = 16-bit DID (e.g. 0xF190 for VIN)
//   value = raw bytes (any data the ECU wants to expose)
//
// In a real ECU this data would come from flash memory,
// calibration files, or live sensor readings.
// ============================================================
class DidStore {
public:
    DidStore();  // Constructor pre-loads some realistic default values

    // Write a DID value (bytes)
    void write(uint16_t did, const std::vector<uint8_t>& value);

    // Write a DID value from a plain string (convenience)
    void writeString(uint16_t did, const std::string& value);

    // Read a DID value — returns empty optional if DID doesn't exist
    std::optional<std::vector<uint8_t>> read(uint16_t did) const;

    // Check if a DID exists in the store
    bool exists(uint16_t did) const;

private:
    // unordered_map = hash map = O(1) lookup
    // Perfect for DID lookup since we search by DID number constantly
    std::unordered_map<uint16_t, std::vector<uint8_t>> m_store;
};

}