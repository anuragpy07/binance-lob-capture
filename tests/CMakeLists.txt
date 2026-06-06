add_executable(unit_tests
    test_main.cpp
    test_csv_escape.cpp
    test_scaled_int.cpp
    test_order_book.cpp
    test_sequence_validator.cpp
    test_csv_schema.cpp
    test_market_data_writer.cpp
    test_orderbook_writer.cpp
    test_replay.cpp
    test_stream_parser.cpp
    test_lob_engine.cpp
    test_replay_integration.cpp
    test_connection_manager.cpp
)

target_include_directories(unit_tests PRIVATE .)
target_link_libraries(unit_tests PRIVATE bncap_lib)

add_test(NAME unit_tests COMMAND unit_tests)

# 'tests' alias so "cmake --build build --target tests" (PDF reviewer command) works
add_custom_target(tests DEPENDS unit_tests)