#include <sstream>
#include <memory>
#include "istream_reader.h"

#define GROUP_NAME ReactiveJsonStream
#define MK_READER(NAME, TEXT) reactive_json::istream_reader NAME(std::make_unique<std::stringstream>(TEXT))
#define RESET_READER(NAME, TEXT) NAME.reset(std::make_unique<std::stringstream>(TEXT))

#include "reader_tests.inc"
