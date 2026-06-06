cmake_minimum_required(VERSION 3.16)
project(binance_capture CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Export compile commands for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -fsanitize=address,undefined")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address,undefined")

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(nlohmann_json REQUIRED)

# --------------------------------------------------------------------------
# Static library – all non-main compilation units
# --------------------------------------------------------------------------
set(LIB_SOURCES
    src/common/time_utils.cpp
    src/common/scaled_int.cpp
    src/common/signal_handler.cpp
    src/csv/csv_escape.cpp
    src/csv/market_data_writer.cpp
    src/csv/orderbook_writer.cpp
    src/marketdata/stream_parser.cpp
    src/marketdata/sequence_validator.cpp
    src/orderbook/order_book.cpp
    src/orderbook/lob_engine.cpp
    src/metrics/metrics.cpp
    src/replay/csv_reader.cpp
    src/replay/replay_engine.cpp
    src/websocket/websocket_client.cpp
    src/websocket/connection_manager.cpp
)

add_library(bncap_lib STATIC ${LIB_SOURCES})
target_include_directories(bncap_lib PUBLIC include)
target_link_libraries(bncap_lib PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    Boost::system
    nlohmann_json::nlohmann_json
)

# --------------------------------------------------------------------------
# Main executable
# --------------------------------------------------------------------------
add_executable(binance_capture src/main.cpp)
target_link_libraries(binance_capture PRIVATE bncap_lib)

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
