#pragma once 

#include <cstdint>
#include <string>
#include <functional>

// DTC = Diagnostic Trouble Code  ....  Lifecycle of a fault

namespace diag {

enum class DtcState : uint8_t {
    NOT_PRESENT  = 0x00,  // No fault 
    PENDING      = 0x01,  // Fault seen once, not yet confirmed
    CONFIRMED    = 0x02,  // Fault seen enough times to be real
    AGED_OUT     = 0x03   // Was confirmed, not seen recently
};

std::string dtcStateToString(DtcState state);

// The DtcStateMachine class manages one DTC's lifecycle.
class DtcStateMachine {
public:
    explicit DtcStateMachine(uint32_t dtcCode,
                             uint8_t confirmationThreshold = 2);

    // If fault condition is detected
    bool reportFault();

    // Call this when the fault condition is NO longer detected
    bool reportFaultCleared();

    // Call this once per "drive cycle" to age out old faults
    void onDriveCycleEnd();

    // Clear the DTC completely ... mechanic called
    void clearDtc();

    // Getters — read-only access to internal data
    DtcState    getState()        const { return m_state; }
    uint32_t    getDtcCode()      const { return m_dtcCode; }
    uint8_t     getFaultCounter() const { return m_faultCounter; }
    bool        isTestFailed()    const { return m_testFailed; }

    // Callback: gets called whenever the state changes
    // This is how the DTC notifies the rest of the system
    using StateChangeCallback = std::function<void(uint32_t dtcCode,
                                                    DtcState oldState,
                                                    DtcState newState)>;
    void setStateChangeCallback(StateChangeCallback cb);

private:
    uint32_t             m_dtcCode;               // e.g. 0x123456
    DtcState             m_state;                 // current state
    uint8_t              m_faultCounter;           // how many faults seen
    uint8_t              m_confirmationThreshold;  // how many needed to confirm
    uint8_t              m_driveCyclesWithoutFault; // for aging logic
    bool                 m_testFailed;             // current test result
    StateChangeCallback  m_callback;               // optional notifier

    // Private helper: transitions state and fires the callback
    void transitionTo(DtcState newState);
};

}