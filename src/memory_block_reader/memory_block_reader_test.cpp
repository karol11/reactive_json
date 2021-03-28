#include "memory_block_reader.h"

#define GROUP_NAME ReactiveJsonReader
#define MK_READER(name, text) reactive_json::memory_block_reader name(text)
#define RESET_READER(name, text) name.reset(text)

#include "reader_tests.inc"
