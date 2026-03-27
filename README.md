# Automotive Software Portfolio

A collection of embedded software projects demonstrating automotive
software development skills, including:

- UDS diagnostic simulation over virtual CAN and Ethernet
- Fault detection and DTC handling (ISO 14229 / ISO 26262 aware)
- Automated testing with Google Test and GitHub Actions

## Projects
- `diagnostic-simulator/` — UDS ECU simulator using Linux SocketCAN
- `fault-handler-lib/` — C++ DTC state machine library
- `sw-component-tests/` — Integration test suite

## Platform
Built and tested on Ubuntu 22.04 with virtual CAN (vcan0).
Designed to be portable to QNX via POSIX abstraction layer.
