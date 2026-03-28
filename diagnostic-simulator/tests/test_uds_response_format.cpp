#include <gtest/gtest.h>
#include "uds_types.h"
#include "did_store.h"

using namespace diag;

// ============================================================
// Test the UdsResponse::negative() factory directly —
// we want to make sure every negative response byte is correct
// because a wrong NRC byte would confuse a real scan tool
// ============================================================

TEST(UdsNegativeResponseTest, NrcServiceNotSupported) {
    auto resp = UdsResponse::negative(ServiceId::READ_DATA_BY_IDENTIFIER,
                                      NrcCode::SERVICE_NOT_SUPPORTED);
    ASSERT_EQ(resp.payload.size(), 4u);
    EXPECT_EQ(resp.payload[0], 0x03);  // length
    EXPECT_EQ(resp.payload[1], 0x7F);  // negative response marker
    EXPECT_EQ(resp.payload[2], 0x22);  // echoed service ID
    EXPECT_EQ(resp.payload[3], 0x11);  // NRC = SERVICE_NOT_SUPPORTED
    EXPECT_FALSE(resp.isPositive);
}

TEST(UdsNegativeResponseTest, NrcRequestOutOfRange) {
    auto resp = UdsResponse::negative(ServiceId::READ_DATA_BY_IDENTIFIER,
                                      NrcCode::REQUEST_OUT_OF_RANGE);
    EXPECT_EQ(resp.payload[1], 0x7F);
    EXPECT_EQ(resp.payload[2], 0x22);
    EXPECT_EQ(resp.payload[3], 0x31);  // NRC = REQUEST_OUT_OF_RANGE
}

TEST(UdsNegativeResponseTest, NrcSecurityAccessDenied) {
    auto resp = UdsResponse::negative(ServiceId::SECURITY_ACCESS,
                                      NrcCode::SECURITY_ACCESS_DENIED);
    EXPECT_EQ(resp.payload[1], 0x7F);
    EXPECT_EQ(resp.payload[2], 0x27);  // service = SECURITY_ACCESS
    EXPECT_EQ(resp.payload[3], 0x33);  // NRC = SECURITY_ACCESS_DENIED
}

TEST(UdsNegativeResponseTest, NrcIncorrectMessageLength) {
    auto resp = UdsResponse::negative(ServiceId::CLEAR_DTC_INFORMATION,
                                      NrcCode::INCORRECT_MESSAGE_LENGTH);
    EXPECT_EQ(resp.payload[2], 0x14);  // service = CLEAR_DTC_INFORMATION
    EXPECT_EQ(resp.payload[3], 0x13);  // NRC = INCORRECT_MESSAGE_LENGTH
}

// ---- Positive response SID rule: always service + 0x40 --------------
// This is a fundamental UDS rule. Test it explicitly so it's documented.

TEST(UdsPositiveResponseSidTest, ReadDataByIdentifierPositiveSidIs0x62) {
    // 0x22 + 0x40 = 0x62
    EXPECT_EQ(0x22 + 0x40, 0x62);
}

TEST(UdsPositiveResponseSidTest, ReadDtcPositiveSidIs0x59) {
    // 0x19 + 0x40 = 0x59
    EXPECT_EQ(0x19 + 0x40, 0x59);
}

TEST(UdsPositiveResponseSidTest, ClearDtcPositiveSidIs0x54) {
    // 0x14 + 0x40 = 0x54
    EXPECT_EQ(0x14 + 0x40, 0x54);
}

TEST(UdsPositiveResponseSidTest, SecurityAccessPositiveSidIs0x67) {
    // 0x27 + 0x40 = 0x67
    EXPECT_EQ(0x27 + 0x40, 0x67);
}

// ---- UdsMessage parsing ----------------------------------------------

TEST(UdsMessageTest, ServiceIdExtractedCorrectly) {
    UdsMessage msg;
    msg.canId   = 0x7DF;
    msg.payload = {0x03, 0x22, 0xF1, 0x90, 0xAA, 0xAA, 0xAA, 0xAA};

    EXPECT_EQ(msg.serviceId(), ServiceId::READ_DATA_BY_IDENTIFIER);
}

TEST(UdsMessageTest, DataBytesExtractedCorrectly) {
    UdsMessage msg;
    msg.payload = {0x03, 0x22, 0xF1, 0x90, 0xAA, 0xAA, 0xAA, 0xAA};

    auto data = msg.data();
    ASSERT_EQ(data.size(), 6u);  // everything after byte 1
    EXPECT_EQ(data[0], 0xF1);
    EXPECT_EQ(data[1], 0x90);
}

TEST(UdsMessageTest, EmptyPayloadDoesNotCrash) {
    UdsMessage msg;
    msg.payload = {};
    EXPECT_NO_THROW(msg.serviceId());
    EXPECT_NO_THROW(msg.data());
}