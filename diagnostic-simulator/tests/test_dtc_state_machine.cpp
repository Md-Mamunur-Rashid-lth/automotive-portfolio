#include <gtest/gtest.h>
#include "dtc_state_machine.h"

using namespace diag;

// ============================================================
// Google Test works like this:
//   TEST(SuiteName, TestName) { ... }
//   EXPECT_EQ(actual, expected)  -- continues on failure
//   ASSERT_EQ(actual, expected)  -- stops test on failure
//   EXPECT_TRUE(condition)
//   EXPECT_FALSE(condition)
// ============================================================

// ---- Initial state --------------------------------------------------

TEST(DtcStateMachineTest, InitialStateIsNotPresent) {
    DtcStateMachine dtc(0x123456, 2);
    EXPECT_EQ(dtc.getState(), DtcState::NOT_PRESENT);
    EXPECT_EQ(dtc.getFaultCounter(), 0);
    EXPECT_FALSE(dtc.isTestFailed());
}

// ---- NOT_PRESENT -> PENDING -----------------------------------------

TEST(DtcStateMachineTest, FirstFaultGoesPending) {
    DtcStateMachine dtc(0x123456, 2);
    bool stateChanged = dtc.reportFault();
    EXPECT_TRUE(stateChanged);
    EXPECT_EQ(dtc.getState(), DtcState::PENDING);
    EXPECT_EQ(dtc.getFaultCounter(), 1);
}

// ---- PENDING -> CONFIRMED -------------------------------------------

TEST(DtcStateMachineTest, SecondFaultConfirmsWithThresholdTwo) {
    DtcStateMachine dtc(0x123456, 2);
    dtc.reportFault();  // -> PENDING
    bool stateChanged = dtc.reportFault();  // -> CONFIRMED
    EXPECT_TRUE(stateChanged);
    EXPECT_EQ(dtc.getState(), DtcState::CONFIRMED);
}

TEST(DtcStateMachineTest, ThresholdThreeNeedsThreeFaults) {
    DtcStateMachine dtc(0xABCDEF, 3);
    dtc.reportFault();
    EXPECT_EQ(dtc.getState(), DtcState::PENDING);
    dtc.reportFault();
    EXPECT_EQ(dtc.getState(), DtcState::PENDING);  // not yet!
    dtc.reportFault();
    EXPECT_EQ(dtc.getState(), DtcState::CONFIRMED); // now
}

// ---- PENDING -> NOT_PRESENT (cleared before confirmed) --------------

TEST(DtcStateMachineTest, PendingFaultClearedDropsToNotPresent) {
    DtcStateMachine dtc(0x123456, 2);
    dtc.reportFault();  // PENDING
    bool stateChanged = dtc.reportFaultCleared();
    EXPECT_TRUE(stateChanged);
    EXPECT_EQ(dtc.getState(), DtcState::NOT_PRESENT);
    EXPECT_EQ(dtc.getFaultCounter(), 0);
}

// ---- CONFIRMED stays CONFIRMED until drive cycles -------------------

TEST(DtcStateMachineTest, ConfirmedStaysConfirmedAfterOneFaultClear) {
    DtcStateMachine dtc(0x123456, 2);
    dtc.reportFault();
    dtc.reportFault();  // CONFIRMED
    dtc.reportFaultCleared();
    // Still CONFIRMED — needs drive cycles to age out
    EXPECT_EQ(dtc.getState(), DtcState::CONFIRMED);
}

// ---- CONFIRMED -> AGED_OUT after 3 clean drive cycles ---------------

TEST(DtcStateMachineTest, AgesOutAfterThreeCleanDriveCycles) {
    DtcStateMachine dtc(0x123456, 2);
    dtc.reportFault();
    dtc.reportFault();          // CONFIRMED
    dtc.reportFaultCleared();   // fault gone but still CONFIRMED

    dtc.onDriveCycleEnd();
    EXPECT_EQ(dtc.getState(), DtcState::CONFIRMED);  // 1 clean cycle
    dtc.onDriveCycleEnd();
    EXPECT_EQ(dtc.getState(), DtcState::CONFIRMED);  // 2 clean cycles
    dtc.onDriveCycleEnd();
    EXPECT_EQ(dtc.getState(), DtcState::AGED_OUT);   // 3 clean cycles
}

TEST(DtcStateMachineTest, DriveCycleWithFaultDoesNotAge) {
    DtcStateMachine dtc(0x123456, 2);
    dtc.reportFault();
    dtc.reportFault();  // CONFIRMED, testFailed=true

    // Drive cycles while fault is still active — should NOT age out
    dtc.onDriveCycleEnd();
    dtc.onDriveCycleEnd();
    dtc.onDriveCycleEnd();
    dtc.onDriveCycleEnd();
    EXPECT_EQ(dtc.getState(), DtcState::CONFIRMED);
}

// ---- AGED_OUT -> CONFIRMED (fault returned) -------------------------

TEST(DtcStateMachineTest, AgedOutReturnsToConfirmedOnNewFault) {
    DtcStateMachine dtc(0x123456, 2);
    dtc.reportFault();
    dtc.reportFault();
    dtc.reportFaultCleared();
    dtc.onDriveCycleEnd();
    dtc.onDriveCycleEnd();
    dtc.onDriveCycleEnd();
    ASSERT_EQ(dtc.getState(), DtcState::AGED_OUT);

    bool changed = dtc.reportFault();
    EXPECT_TRUE(changed);
    EXPECT_EQ(dtc.getState(), DtcState::CONFIRMED);
}

// ---- clearDtc resets everything -------------------------------------

TEST(DtcStateMachineTest, ClearDtcResetsToNotPresent) {
    DtcStateMachine dtc(0x123456, 2);
    dtc.reportFault();
    dtc.reportFault();  // CONFIRMED
    dtc.clearDtc();
    EXPECT_EQ(dtc.getState(), DtcState::NOT_PRESENT);
    EXPECT_EQ(dtc.getFaultCounter(), 0);
    EXPECT_FALSE(dtc.isTestFailed());
}

// ---- Callback fires on every state change ---------------------------

TEST(DtcStateMachineTest, CallbackFiresOnStateChange) {
    DtcStateMachine dtc(0x123456, 2);

    int callbackCount = 0;
    DtcState lastNewState = DtcState::NOT_PRESENT;

    dtc.setStateChangeCallback([&](uint32_t, DtcState, DtcState to) {
        callbackCount++;
        lastNewState = to;
    });

    dtc.reportFault();  // -> PENDING
    EXPECT_EQ(callbackCount, 1);
    EXPECT_EQ(lastNewState, DtcState::PENDING);

    dtc.reportFault();  // -> CONFIRMED
    EXPECT_EQ(callbackCount, 2);
    EXPECT_EQ(lastNewState, DtcState::CONFIRMED);

    dtc.clearDtc();     // -> NOT_PRESENT
    EXPECT_EQ(callbackCount, 3);
    EXPECT_EQ(lastNewState, DtcState::NOT_PRESENT);
}

// ---- No callback = no crash -----------------------------------------

TEST(DtcStateMachineTest, NoCallbackDoesNotCrash) {
    DtcStateMachine dtc(0x123456, 2);
    // No callback registered — should not crash or throw
    EXPECT_NO_THROW({
        dtc.reportFault();
        dtc.reportFault();
        dtc.clearDtc();
    });
}

// ---- DTC code is preserved ------------------------------------------

TEST(DtcStateMachineTest, DtcCodePreserved) {
    DtcStateMachine dtc(0xDEADBE, 2);
    EXPECT_EQ(dtc.getDtcCode(), static_cast<uint32_t>(0xDEADBE));
}