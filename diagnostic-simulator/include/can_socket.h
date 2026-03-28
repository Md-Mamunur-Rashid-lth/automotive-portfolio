#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <linux/can.h>    // CAN frame definitions from the Linux kernel

namespace diag {

// A CanSocket wraps the Linux SocketCAN API into a clean C++ class.
// SocketCAN is a Linux subsystem that lets you talk to CAN buses
// using normal socket() calls — the same API used for TCP/IP.
// This means: same code works on vcan0 (virtual) and can0 (real hardware)

class CanSocket {
public:
    explicit CanSocket(const std::string& interfaceName);

    // Rule of 5: in C++ when you manage a resource (like a file descriptor)
    // you must define or delete these to prevent double-close bugs
    ~CanSocket();
    CanSocket(const CanSocket&)            = delete; // no copying
    CanSocket& operator=(const CanSocket&) = delete; // no copy-assign
    CanSocket(CanSocket&&)                 = default; // moving is ok
    CanSocket& operator=(CanSocket&&)      = default;

    // Send one CAN frame
    // canId: the 11-bit or 29-bit CAN identifier
    // data: up to 8 bytes of payload
    bool send(uint32_t canId, const std::vector<uint8_t>& data);

    // Receive one CAN frame (blocks until a frame arrives)
    // Returns false if something went wrong
    bool receive(uint32_t& outCanId, std::vector<uint8_t>& outData);

    bool isOpen() const { return m_socketFd >= 0; }

private:
    int         m_socketFd;       // The file descriptor for our socket
    std::string m_interfaceName;  // e.g. "vcan0"
};

}