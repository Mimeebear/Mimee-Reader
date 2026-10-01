/*
 * Thai trie traversal is adapted from libthai 0.1.29 and libdatrie 0.2.13.
 * Copyright (C) 2001-2021 Theppitak Karoonboonyanan and libthai contributors.
 * Copyright (C) 2006 Theppitak Karoonboonyanan (libdatrie).
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * See COPYING.LGPL-2.1 and AUTHORS in this directory.
 */

#include "ThaiWordBreaker.h"

#include <algorithm>

#include "ThaiWordTrieData.h"

namespace ThaiWordBreaker {
namespace {

using detail::kTailIndexStride;
using detail::kTailRecordCount;
using detail::kTailRecordOffsets;
using detail::kTrieBytes;
using detail::kTrieSize;

constexpr uint32_t ALPHAMAP_SIGNATURE = 0xD9FCD9FC;
constexpr uint32_t DARRAY_SIGNATURE = 0xDAFCDAFC;
constexpr uint32_t TAIL_SIGNATURE = 0xDFFCDFFC;
constexpr size_t TAIL_INDEX_STRIDE = 16;

struct TrieLayout {
  uint32_t rangeCount;
  uint32_t darrayOffset;
  uint32_t cellCount;
  uint32_t tailOffset;
  uint32_t tailCount;
  bool valid;
};

uint32_t readU32(const uint32_t offset) {
  if (offset > kTrieSize || kTrieSize - offset < 4) return 0;
  return (static_cast<uint32_t>(kTrieBytes[offset]) << 24) |
         (static_cast<uint32_t>(kTrieBytes[offset + 1]) << 16) |
         (static_cast<uint32_t>(kTrieBytes[offset + 2]) << 8) | static_cast<uint32_t>(kTrieBytes[offset + 3]);
}

int32_t readI32(const uint32_t offset) { return static_cast<int32_t>(readU32(offset)); }

uint16_t readU16(const uint32_t offset) {
  if (offset > kTrieSize || kTrieSize - offset < 2) return 0;
  return static_cast<uint16_t>((static_cast<uint16_t>(kTrieBytes[offset]) << 8) | kTrieBytes[offset + 1]);
}

TrieLayout getLayout() {
  TrieLayout layout{};
  if (readU32(0) != ALPHAMAP_SIGNATURE) return layout;
  layout.rangeCount = readU32(4);
  if (layout.rangeCount == 0 || layout.rangeCount > 16) return layout;

  layout.darrayOffset = 8 + layout.rangeCount * 8;
  if (readU32(layout.darrayOffset) != DARRAY_SIGNATURE) return layout;
  layout.cellCount = readU32(layout.darrayOffset + 4);
  if (layout.cellCount < 3 || layout.cellCount > (kTrieSize / 8)) return layout;

  layout.tailOffset = layout.darrayOffset + layout.cellCount * 8;
  if (readU32(layout.tailOffset) != TAIL_SIGNATURE) return layout;
  layout.tailCount = readU32(layout.tailOffset + 8);
  layout.valid = layout.tailCount == kTailRecordCount && kTailIndexStride == TAIL_INDEX_STRIDE;
  return layout;
}

const TrieLayout& trieLayout() {
  static const TrieLayout layout = getLayout();
  return layout;
}

uint32_t cellBaseOffset(const TrieLayout& layout, const uint32_t state) {
  return layout.darrayOffset + state * 8;
}

int32_t cellBase(const TrieLayout& layout, const uint32_t state) {
  if (state >= layout.cellCount) return -1;
  return readI32(cellBaseOffset(layout, state));
}

int32_t cellCheck(const TrieLayout& layout, const uint32_t state) {
  if (state >= layout.cellCount) return -1;
  return readI32(cellBaseOffset(layout, state) + 4);
}

bool tailByte(const TrieLayout& layout, const uint32_t tailIndex, const uint16_t suffixOffset, uint8_t& value) {
  if (tailIndex == 0 || tailIndex > layout.tailCount) return false;
  const uint32_t block = (tailIndex - 1) / kTailIndexStride;
  const uint32_t firstIndex = block * kTailIndexStride + 1;
  if (block >= (layout.tailCount + kTailIndexStride - 1) / kTailIndexStride) return false;

  uint32_t recordOffset = kTailRecordOffsets[block];
  for (uint32_t index = firstIndex; index < tailIndex; ++index) {
    const uint16_t suffixLength = readU16(recordOffset + 8);
    recordOffset += 10 + suffixLength;
  }

  const uint16_t suffixLength = readU16(recordOffset + 8);
  if (suffixOffset >= suffixLength) {
    value = 0;
    return true;
  }
  const uint32_t byteOffset = recordOffset + 10 + suffixOffset;
  if (byteOffset >= kTrieSize) return false;
  value = kTrieBytes[byteOffset];
  return true;
}

uint32_t trieCode(const TrieLayout& layout, const uint32_t cp) {
  for (uint32_t index = 0; index < layout.rangeCount; ++index) {
    const uint32_t offset = 8 + index * 8;
    const uint32_t begin = readU32(offset);
    const uint32_t end = readU32(offset + 4);
    if (begin <= cp && cp <= end) {
      const uint32_t mapped = cp - begin + 1;
      return mapped == 0 ? 0 : mapped;
    }
  }
  return UINT32_MAX;
}

uint32_t decodeCodepoint(const std::string_view text, const size_t offset, size_t& nextOffset) {
  const auto first = static_cast<uint8_t>(text[offset]);
  if (first < 0x80) {
    nextOffset = offset + 1;
    return first;
  }

  uint32_t cp;
  size_t length;
  if ((first & 0xE0) == 0xC0) {
    cp = first & 0x1F;
    length = 2;
  } else if ((first & 0xF0) == 0xE0) {
    cp = first & 0x0F;
    length = 3;
  } else if ((first & 0xF8) == 0xF0) {
    cp = first & 0x07;
    length = 4;
  } else {
    nextOffset = offset + 1;
    return 0xFFFD;
  }

  if (length > text.size() - offset) {
    nextOffset = offset + 1;
    return 0xFFFD;
  }
  for (size_t index = 1; index < length; ++index) {
    const auto continuation = static_cast<uint8_t>(text[offset + index]);
    if ((continuation & 0xC0) != 0x80) {
      nextOffset = offset + 1;
      return 0xFFFD;
    }
    cp = (cp << 6) | (continuation & 0x3F);
  }
  nextOffset = offset + length;
  return cp;
}

uint32_t codepointBefore(const std::string_view text, const size_t byteOffset) {
  if (byteOffset == 0 || byteOffset > text.size()) return 0;
  size_t start = byteOffset - 1;
  while (start > 0 && (static_cast<uint8_t>(text[start]) & 0xC0) == 0x80) --start;
  size_t ignored = start;
  return decodeCodepoint(text, start, ignored);
}

bool isThaiLeadingVowel(const uint32_t cp) { return cp >= 0x0E40 && cp <= 0x0E44; }

bool isThaiFollowingVowelOrMark(const uint32_t cp) {
  return (cp >= 0x0E30 && cp <= 0x0E3A) || cp == 0x0E45 || (cp >= 0x0E47 && cp <= 0x0E4E);
}

bool isSafeThaiBoundary(const std::string_view text, const size_t offset) {
  if (offset == 0 || offset >= text.size()) return true;
  size_t nextOffset = offset;
  const uint32_t next = decodeCodepoint(text, offset, nextOffset);
  const uint32_t previous = codepointBefore(text, offset);
  if (isThaiFollowingVowelOrMark(next)) return false;
  return !isThaiLeadingVowel(previous) || next < 0x0E01 || next > 0x0E2E;
}

size_t fallbackClusterEnd(const std::string_view text, const size_t start, const size_t runEnd) {
  size_t offset = start;
  size_t nextOffset = offset;
  uint32_t cp = decodeCodepoint(text, offset, nextOffset);

  while (isThaiLeadingVowel(cp) && nextOffset < runEnd) {
    offset = nextOffset;
    cp = decodeCodepoint(text, offset, nextOffset);
  }

  if (isThaiFollowingVowelOrMark(cp)) {
    while (nextOffset < runEnd) {
      offset = nextOffset;
      cp = decodeCodepoint(text, offset, nextOffset);
      if (isThaiFollowingVowelOrMark(cp)) continue;
      break;
    }
  } else {
    offset = nextOffset;
  }

  while (offset < runEnd) {
    cp = decodeCodepoint(text, offset, nextOffset);
    if (!isThaiFollowingVowelOrMark(cp)) break;
    offset = nextOffset;
  }
  return std::max(offset, start + 1);
}

struct TrieState {
  uint32_t index;
  uint16_t suffixOffset;
  bool inTail;
};

bool walk(const TrieLayout& layout, TrieState& state, const uint32_t cp) {
  const uint32_t symbol = trieCode(layout, cp);
  if (symbol == UINT32_MAX) return false;

  if (state.inTail) {
    uint8_t current = 0;
    if (!tailByte(layout, state.index, state.suffixOffset, current) || current != symbol) return false;
    if (current != 0) ++state.suffixOffset;
    return true;
  }

  const int32_t base = cellBase(layout, state.index);
  if (base < 0 || symbol > static_cast<uint32_t>(INT32_MAX - base)) return false;
  const uint32_t nextState = static_cast<uint32_t>(base) + symbol;
  if (nextState >= layout.cellCount || cellCheck(layout, nextState) != static_cast<int32_t>(state.index)) {
    return false;
  }

  const int32_t nextBase = cellBase(layout, nextState);
  state.index = nextBase < 0 ? static_cast<uint32_t>(-nextBase) : nextState;
  state.suffixOffset = 0;
  state.inTail = nextBase < 0;
  return true;
}

bool isTerminal(const TrieLayout& layout, const TrieState& state) {
  if (state.inTail) {
    uint8_t next = 0xFF;
    return tailByte(layout, state.index, state.suffixOffset, next) && next == 0;
  }

  const int32_t base = cellBase(layout, state.index);
  if (base < 0 || static_cast<uint32_t>(base) >= layout.cellCount) return false;
  const uint32_t nextState = static_cast<uint32_t>(base);
  return cellCheck(layout, nextState) == static_cast<int32_t>(state.index) && cellBase(layout, nextState) < 0;
}

size_t thaiRunEnd(const std::string_view text, const size_t start) {
  size_t offset = start;
  while (offset < text.size()) {
    size_t nextOffset = offset;
    const uint32_t cp = decodeCodepoint(text, offset, nextOffset);
    if (!isThaiCodepoint(cp)) break;
    offset = nextOffset;
  }
  return offset;
}

}  // namespace

bool isThaiCodepoint(const uint32_t cp) { return cp >= 0x0E00 && cp <= 0x0E7F; }

bool containsThai(const std::string_view text) {
  size_t marker = text.find('\xE0');
  while (marker != std::string_view::npos && marker + 2 < text.size()) {
    const auto second = static_cast<uint8_t>(text[marker + 1]);
    const auto third = static_cast<uint8_t>(text[marker + 2]);
    if ((second == 0xB8 || second == 0xB9) && (third & 0xC0) == 0x80) return true;
    marker = text.find('\xE0', marker + 1);
  }
  return false;
}

bool isThaiGraphemeBoundary(const std::string_view text, const size_t byteOffset) {
  return isSafeThaiBoundary(text, byteOffset);
}

bool boundaryTouchesThai(const std::string_view text, const size_t byteOffset) {
  if (byteOffset > text.size()) return false;
  if (byteOffset > 0 && isThaiCodepoint(codepointBefore(text, byteOffset))) return true;
  if (byteOffset < text.size()) {
    size_t nextOffset = byteOffset;
    return isThaiCodepoint(decodeCodepoint(text, byteOffset, nextOffset));
  }
  return false;
}

size_t nextBoundary(const std::string_view text, const size_t startByte) {
  if (startByte >= text.size()) return text.size();
  size_t firstEnd = startByte;
  if (!isThaiCodepoint(decodeCodepoint(text, startByte, firstEnd))) return text.size();

  const size_t runEnd = thaiRunEnd(text, startByte);
  const TrieLayout& layout = trieLayout();
  if (!layout.valid) return fallbackClusterEnd(text, startByte, runEnd);

  TrieState state{2, 0, false};
  size_t offset = startByte;
  size_t bestBoundary = startByte;
  while (offset < runEnd) {
    size_t nextOffset = offset;
    const uint32_t cp = decodeCodepoint(text, offset, nextOffset);
    if (!walk(layout, state, cp)) break;
    offset = nextOffset;
    if (isTerminal(layout, state) && isSafeThaiBoundary(text, offset)) bestBoundary = offset;
  }

  return bestBoundary > startByte ? bestBoundary : fallbackClusterEnd(text, startByte, runEnd);
}

}  // namespace ThaiWordBreaker