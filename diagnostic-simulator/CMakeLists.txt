cmake_minimum_required(VERSION 3.16)
project(DiagnosticSimulator VERSION 0.1.0 LANGUAGES CXX)

# Tell the compiler to use C++17 and be strict about warnings
# -Wall = show all warnings, -Wextra = show extra warnings
# Warnings in embedded code often mean bugs — treat them seriously
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

# Tell CMake where to find our header files
include_directories(include)

# Build the main executable from our source files
add_executable(diag_sim
    src/main.cpp
    src/dtc_state_machine.cpp
    src/can_socket.cpp
)