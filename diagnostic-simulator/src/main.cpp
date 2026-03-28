#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <csignal>
#include "uds_server.h"
#include "dtc_state_machine.h"

// Global pointer so our signal handler can stop the server cleanly
// (signal handlers can only access global state)
diag::UdsServer* g_server = nullptr;

void signalHandler(int) {
    std::cout << "\nShutting down...\n";
    if (g_server) g_server->stop();
}

int main() {
    std::cout << "==============================================\n";
    std::cout << "  Automotive Diagnostic Simulator\n";
    std::cout << "  UDS over virtual CAN (vcan0)\n";
    std::cout << "==============================================\n\n";

    // Create some DTCs our ECU can report
    // These represent faults the ECU monitors
    auto dtc1 = std::make_shared<diag::DtcStateMachine>(0x123456, 2);
    auto dtc2 = std::make_shared<diag::DtcStateMachine>(0xABCDEF, 2);
    auto dtc3 = std::make_shared<diag::DtcStateMachine>(0x001234, 2);

    // Register callbacks so we can see DTC state changes
    auto dtcLogger = [](uint32_t code,
                        diag::DtcState from,
                        diag::DtcState to) {
        std::cout << "[DTC] 0x" << std::hex << std::uppercase << code
                  << ": " << diag::dtcStateToString(from)
                  << " -> " << diag::dtcStateToString(to)
                  << std::dec << "\n";
    };
    dtc1->setStateChangeCallback(dtcLogger);
    dtc2->setStateChangeCallback(dtcLogger);
    dtc3->setStateChangeCallback(dtcLogger);

    // Inject a fault on DTC1 at startup so we have something to show
    dtc1->reportFault();
    dtc1->reportFault();  // confirmed

    // Create the UDS server
    diag::UdsServer server("vcan0", {dtc1, dtc2, dtc3});
    g_server = &server;

    // Pretty log output
    server.setLogCallback([](const std::string& msg) {
        std::cout << msg << "\n";
    });

    // Handle Ctrl+C gracefully
    std::signal(SIGINT, signalHandler);

    std::cout << "Tip: In another terminal, try these commands:\n";
    std::cout << "  candump vcan0\n";
    std::cout << "  cansend vcan0 7DF#0222F190AAAAAAAAA   (read VIN)\n";
    std::cout << "  cansend vcan0 7DF#0219020FAAAAAAAAA   (read DTCs)\n";
    std::cout << "  cansend vcan0 7DF#0414FFFFFFAAAAAA    (clear DTCs)\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    server.run();  // blocks until stop() is called

    return 0;
}