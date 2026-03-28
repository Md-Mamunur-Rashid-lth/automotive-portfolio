#include <gtest/gtest.h>
#include "did_store.h"
#include "uds_types.h"

using namespace diag;

TEST(DidStoreTest, DefaultVinIsPresent) {
    DidStore store;
    auto result = store.read(Did::VIN);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 17u);  // VIN is always 17 chars
}

TEST(DidStoreTest, DefaultVinContent) {
    DidStore store;
    auto result = store.read(Did::VIN);
    ASSERT_TRUE(result.has_value());
    std::string vin(result->begin(), result->end());
    EXPECT_EQ(vin, "YV1PORTFOLIO12345");
}

TEST(DidStoreTest, ReadNonExistentDidReturnsEmpty) {
    DidStore store;
    auto result = store.read(0x1234);  // not in store
    EXPECT_FALSE(result.has_value());
}

TEST(DidStoreTest, WriteAndReadBack) {
    DidStore store;
    std::vector<uint8_t> testData = {0x01, 0x02, 0x03};
    store.write(0xBEEF, testData);

    auto result = store.read(0xBEEF);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, testData);
}

TEST(DidStoreTest, WriteStringAndReadBack) {
    DidStore store;
    store.writeString(0x1111, "hello");

    auto result = store.read(0x1111);
    ASSERT_TRUE(result.has_value());
    std::string str(result->begin(), result->end());
    EXPECT_EQ(str, "hello");
}

TEST(DidStoreTest, OverwriteExistingDid) {
    DidStore store;
    store.write(0xAAAA, {0x01});
    store.write(0xAAAA, {0x02, 0x03});  // overwrite

    auto result = store.read(0xAAAA);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2u);
    EXPECT_EQ((*result)[0], 0x02);
}

TEST(DidStoreTest, ExistsReturnsTrueForKnownDid) {
    DidStore store;
    EXPECT_TRUE(store.exists(Did::VIN));
}

TEST(DidStoreTest, ExistsReturnsFalseForUnknownDid) {
    DidStore store;
    EXPECT_FALSE(store.exists(0x9999));
}

TEST(DidStoreTest, AllDefaultDidsArePresent) {
    DidStore store;
    EXPECT_TRUE(store.exists(Did::VIN));
    EXPECT_TRUE(store.exists(Did::ECU_SERIAL_NUMBER));
    EXPECT_TRUE(store.exists(Did::SOFTWARE_VERSION));
    EXPECT_TRUE(store.exists(Did::ECU_SUPPLIER));
    EXPECT_TRUE(store.exists(Did::ACTIVE_DIAGNOSTIC_INFO));
}