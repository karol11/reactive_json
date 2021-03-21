cmake_minimum_required (VERSION 3.8)

project ("reactive_json")

set(CMAKE_CXX_STANDARD 17)
add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

include_directories (
    "include"
    "tests"
)

add_executable (reactive_json
    "include/reactive_json_reader.h"
    "src/reactive_json_reader.cpp"

    "tests/gunit.h"
    "tests/gunit.cpp"
    "tests/reactive_json_reader_test.cpp"
)
