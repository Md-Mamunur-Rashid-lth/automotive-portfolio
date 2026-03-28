cmake_minimum_required(VERSION 3.16)
project(DiagnosticSimulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(include)

# ---- Library target (all code except main.cpp) ----
# We build a library so both the main executable and the
# tests can link against it without compiling twice
add_library(diag_lib
    src/dtc_state_machine.cpp
    src/can_socket.cpp
    src/did_store.cpp
    src/uds_server.cpp
)
target_link_libraries(diag_lib pthread)

# ---- Main executable ----
add_executable(diag_sim src/main.cpp)
target_link_libraries(diag_sim diag_lib)

# ---- Tests ----
enable_testing()
find_package(GTest REQUIRED)

add_executable(diag_tests
    tests/test_dtc_state_machine.cpp
    tests/test_did_store.cpp
    tests/test_uds_response_format.cpp
)
target_link_libraries(diag_tests
    diag_lib
    GTest::GTest
    GTest::Main   # provides main() for the test runner
)

# Register with ctest so "ctest" command runs everything
include(GoogleTest)
gtest_discover_tests(diag_tests)