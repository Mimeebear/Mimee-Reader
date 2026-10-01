#!/usr/bin/env python3
"""Generate a flash-resident C++ view of the libdatrie Thai trie."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parent
TRIE_PATH = ROOT / "thbrk.tri"
OUTPUT_PATH = ROOT / "ThaiWordTrieData.cpp"
TAIL_INDEX_STRIDE = 16


def read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def main() -> None:
    data = TRIE_PATH.read_bytes()
    if read_u32(data, 0) != 0xD9FCD9FC:
        raise ValueError("Unexpected libdatrie alphabet-map signature")

    range_count = read_u32(data, 4)
    darray_offset = 8 + range_count * 8
    if read_u32(data, darray_offset) != 0xDAFCDAFC:
        raise ValueError("Unexpected libdatrie double-array signature")

    cell_count = read_u32(data, darray_offset + 4)
    tail_offset = darray_offset + cell_count * 8
    if read_u32(data, tail_offset) != 0xDFFCDFFC:
        raise ValueError("Unexpected libdatrie tail signature")

    tail_count = read_u32(data, tail_offset + 8)
    offsets = []
    record_offset = tail_offset + 12
    for index in range(tail_count):
        if index % TAIL_INDEX_STRIDE == 0:
            offsets.append(record_offset)
        suffix_length = struct.unpack_from(">H", data, record_offset + 8)[0]
        record_offset += 10 + suffix_length

    if record_offset != len(data):
        raise ValueError("Trie tail records do not consume the source file")

    with OUTPUT_PATH.open("w", encoding="ascii", newline="\n") as output:
        output.write("/*\n")
        output.write(" * Generated from libthai thbrk.tri.\n")
        output.write(" * Copyright (C) 2001-2021 Theppitak Karoonboonyanan and libthai contributors.\n")
        output.write(" * SPDX-License-Identifier: LGPL-2.1-or-later\n */\n")
        output.write('#include "ThaiWordTrieData.h"\n\n')
        output.write("namespace ThaiWordBreaker {\nnamespace detail {\n\n")
        output.write("alignas(4) const uint8_t kTrieBytes[] = {\n")
        for start in range(0, len(data), 16):
            row = data[start : start + 16]
            output.write("  " + ", ".join(f"0x{value:02X}" for value in row) + ",\n")
        output.write("};\n")
        output.write("const uint32_t kTrieSize = sizeof(kTrieBytes);\n\n")
        output.write("const uint32_t kTailRecordOffsets[] = {\n")
        for start in range(0, len(offsets), 8):
            row = offsets[start : start + 8]
            output.write("  " + ", ".join(str(value) for value in row) + ",\n")
        output.write("};\n")
        output.write(f"const uint32_t kTailRecordCount = {tail_count};\n")
        output.write(f"const uint32_t kTailIndexStride = {TAIL_INDEX_STRIDE};\n")
        output.write("\n}  // namespace detail\n}  // namespace ThaiWordBreaker\n")


if __name__ == "__main__":
    main()