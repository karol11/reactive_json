cmake_minimum_required (VERSION 3.8)

project ("reactive_json")

set(CMAKE_CXX_STANDARD 17)
add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

include_directories (
    "tests"
)

add_executable (reactive_json
    "src/istream_reader/istream_reader.h"
    "src/istream_reader/istream_reader.cpp"
    "src/istream_reader/istream_reader_test.cpp"

    "src/memory_block_reader/memory_block_reader.h"
    "src/memory_block_reader/memory_block_reader.cpp"
    "src/memory_block_reader/memory_block_reader_test.cpp"

    "tests/gunit.h"
    "tests/gunit.cpp"
    "tests/reader_tests.inc"
 )
