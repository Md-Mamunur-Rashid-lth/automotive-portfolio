#include <iostream>
#include <iomanip>
#include "dtc_state_machine.h"

// Helper: print the current status of a DTC clearly
void printStatus(const diag::DtcStateMachine& dtc) {
    std::cout << "  DTC 0x" << std::hex << std::uppercase
              << std::setw(6) << std::setfill('0') << dtc.getDtcCode()
              << std::dec
              << " | State: " << std::setw(12) << std::left
              << diag::dtcStateToString(dtc.getState())
              << " | Counter: " << (int)dtc.getFaultCounter()
              << "\n";
}

int main() {
    std::cout << "=== DTC State Machine Demo ===\n\n";

    // Create a DTC with code 0x123456
    // Needs 2 fault reports to become CONFIRMED
    diag::DtcStateMachine dtc(0x123456, 2);

    // Register a callback so we can see every state change
    dtc.setStateChangeCallback([](uint32_t code,
                                   diag::DtcState from,
                                   diag::DtcState to) {
        std::cout << "  [EVENT] DTC 0x" << std::hex << code << std::dec
                  << " changed: "
                  << diag::dtcStateToString(from)
                  << " -> "
                  << diag::dtcStateToString(to) << "\n";
    });

    std::cout << "Initial state:\n";
    printStatus(dtc);

    std::cout << "\n--- Reporting fault (1st time) ---\n";
    dtc.reportFault();
    printStatus(dtc);  // should be PENDING

    std::cout << "\n--- Reporting fault (2nd time) ---\n";
    dtc.reportFault();
    printStatus(dtc);  // should be CONFIRMED

    std::cout << "\n--- Drive cycle ends (no fault seen) ---\n";
    dtc.reportFaultCleared();
    dtc.onDriveCycleEnd();
    printStatus(dtc);  // still CONFIRMED (need 3 clean cycles)

    std::cout << "\n--- Two more clean drive cycles ---\n";
    dtc.onDriveCycleEnd();
    dtc.onDriveCycleEnd();
    printStatus(dtc);  // should be AGED_OUT

    std::cout << "\n--- Mechanic clears DTCs ---\n";
    dtc.clearDtc();
    printStatus(dtc);  // back to NOT_PRESENT

    std::cout << "\nDone.\n";
    return 0;
}