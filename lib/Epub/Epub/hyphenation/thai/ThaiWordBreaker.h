#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ThaiWordBreaker {

bool containsThai(std::string_view text);
bool isThaiCodepoint(uint32_t cp);
bool boundaryTouchesThai(std::string_view text, size_t byteOffset);
bool isThaiGraphemeBoundary(std::string_view text, size_t byteOffset);
size_t nextBoundary(std::string_view text, size_t startByte);

}  // namespace ThaiWordBreaker