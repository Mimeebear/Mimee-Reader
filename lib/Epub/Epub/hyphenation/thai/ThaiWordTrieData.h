#pragma once

#include <cstdint>

namespace ThaiWordBreaker::detail {

extern const uint8_t kTrieBytes[];
extern const uint32_t kTrieSize;
extern const uint32_t kTailRecordOffsets[];
extern const uint32_t kTailRecordCount;
extern const uint32_t kTailIndexStride;

}  // namespace ThaiWordBreaker::detail