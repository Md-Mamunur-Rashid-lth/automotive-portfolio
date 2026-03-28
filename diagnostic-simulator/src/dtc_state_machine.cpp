#include "dtc_state_machine.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>  // for std::hex formatting

namespace diag {

//  Helper: state to string 

std::string dtcStateToString(DtcState state) {
    switch (state) {
        case DtcState::NOT_PRESENT: return "NOT_PRESENT";
        case DtcState::PENDING:     return "PENDING";
        case DtcState::CONFIRMED:   return "CONFIRMED";
        case DtcState::AGED_OUT:    return "AGED_OUT";
        default:                    return "UNKNOWN";
    }
}

//  Constructor 

DtcStateMachine::DtcStateMachine(uint32_t dtcCode,
                                  uint8_t confirmationThreshold)
    : m_dtcCode(dtcCode)
    , m_state(DtcState::NOT_PRESENT)
    , m_faultCounter(0)
    , m_confirmationThreshold(confirmationThreshold)
    , m_driveCyclesWithoutFault(0)
    , m_testFailed(false)
    , m_callback(nullptr)
{
    // initialiser list above
}

//  reportFault 

bool DtcStateMachine::reportFault() {
    m_testFailed = true;
    m_driveCyclesWithoutFault = 0;
    DtcState oldState = m_state;

    switch (m_state) {
        case DtcState::NOT_PRESENT:
            // First time we see this fault: go to PENDING
            m_faultCounter = 1;
            transitionTo(DtcState::PENDING);
            break;

        case DtcState::PENDING:
            // We've seen it before: count up
            m_faultCounter++;
            if (m_faultCounter >= m_confirmationThreshold) {
                // Seen it enough times — it's real, confirm it
                transitionTo(DtcState::CONFIRMED);
            }
            break;

        case DtcState::CONFIRMED:
            // Already confirmed, nothing changes state-wise
            // but we reset the aging counter
            break;

        case DtcState::AGED_OUT:
            // It came back! Re-confirm immediately
            transitionTo(DtcState::CONFIRMED);
            break;
    }

    return (m_state != oldState); // true if state changed
}

//  reportFaultCleared ..... Called when the fault condition is NO LONGER present

bool DtcStateMachine::reportFaultCleared() {
    m_testFailed = false;
    DtcState oldState = m_state;

    if (m_state == DtcState::PENDING) {
        // Was only pending, never confirmed — drop it
        m_faultCounter = 0;
        transitionTo(DtcState::NOT_PRESENT);
    }
    // If CONFIRMED, we keep it confirmed until drive cycle ends .... a confirmed fault stays in memory)

    return (m_state != oldState);
}

//  onDriveCycleEnd 
// Called once per ignition cycle (when car is turned off)

void DtcStateMachine::onDriveCycleEnd() {
    if (m_state == DtcState::CONFIRMED && !m_testFailed) {
        m_driveCyclesWithoutFault++;
        // After 3 drive cycles without the fault, age it out
        if (m_driveCyclesWithoutFault >= 3) {
            transitionTo(DtcState::AGED_OUT);
        }
    }
}

//  clearDtc 
// Mechanic plugs in a scan tool and clears all faults

void DtcStateMachine::clearDtc() {
    m_faultCounter = 0;
    m_driveCyclesWithoutFault = 0;
    m_testFailed = false;
    transitionTo(DtcState::NOT_PRESENT);
}

// setStateChangeCallback 

void DtcStateMachine::setStateChangeCallback(StateChangeCallback cb) {
    m_callback = cb;
}

//  transitionTo (private) 
// Single place where all state transitions happen.
// to add a log or breakpoint.

void DtcStateMachine::transitionTo(DtcState newState) {
    if (newState == m_state) return; // no change, do nothing

    DtcState oldState = m_state;
    m_state = newState;

    // Fire the callback if one is registered
    if (m_callback) {
        m_callback(m_dtcCode, oldState, newState);
    }
}

}