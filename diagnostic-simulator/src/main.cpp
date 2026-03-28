#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include "dtc_state_machine.h"
#include "can_socket.h"

void printFrame(uint32_t id, const std::vector<uint8_t>& data) {
    std::cout << "  CAN ID: 0x" << std::hex << std::uppercase
              << std::setw(3) << std::setfill('0') << id
              << "  Data: ";
    for (auto b : data) {
        std::cout << std::setw(2) << std::setfill('0')
                  << (int)b << " ";
    }
    std::cout << std::dec << "\n";
}

int main() {
    std::cout << "=== Diagnostic Simulator ===\n\n";

    // --- Part 1: DTC state machine demo ---
    std::cout << "--- DTC State Machine ---\n";
    diag::DtcStateMachine dtc(0x123456, 2);
    dtc.setStateChangeCallback([](uint32_t code,
                                   diag::DtcState from,
                                   diag::DtcState to) {
        std::cout << "  [DTC EVENT] 0x" << std::hex << code << std::dec
                  << ": " << diag::dtcStateToString(from)
                  << " -> " << diag::dtcStateToString(to) << "\n";
    });
    dtc.reportFault();
    dtc.reportFault(); // confirmed
    std::cout << "  Final DTC state: "
              << diag::dtcStateToString(dtc.getState()) << "\n\n";

    // --- Part 2: CAN socket demo ---
    std::cout << "--- CAN Socket (vcan0) ---\n";
    diag::CanSocket sock("vcan0");

    if (!sock.isOpen()) {
        std::cerr << "Could not open vcan0. Did you run:\n"
                  << "  sudo modprobe vcan\n"
                  << "  sudo ip link add dev vcan0 type vcan\n"
                  << "  sudo ip link set up vcan0\n";
        return 1;
    }

    // Send a UDS "ReadDataByIdentifier" request
    // 0x7DF = OBD-II functional broadcast address
    // 02 22 F1 90 = UDS: length=2, service=0x22, DID=0xF190 (VIN request)
    std::vector<uint8_t> udsRequest = {0x02, 0x22, 0xF1, 0x90,
                                        0xAA, 0xAA, 0xAA, 0xAA};
    std::cout << "Sending UDS ReadDataByIdentifier (DID 0xF190 = VIN):\n";
    sock.send(0x7DF, udsRequest);
    printFrame(0x7DF, udsRequest);

    // Receive our own frame back (we hear our own transmissions on vcan0)
    uint32_t rxId;
    std::vector<uint8_t> rxData;
    std::cout << "Received back:\n";
    sock.receive(rxId, rxData);
    printFrame(rxId, rxData);

    std::cout << "\nDone.\n";
    return 0;
}