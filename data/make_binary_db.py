#!/usr/bin/env python3

import csv
import struct
import sys

MAGIC = 0x4B455744
VERSION = 1

if len(sys.argv) != 3:
    print(f"usage: {sys.argv[0]} input.tsv output.db")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]

records = []

with open(input_file, "r", encoding="utf-8", newline="") as f:
    reader = csv.reader(f, delimiter="\t")

    for row in reader:
        if len(row) < 3:
            continue

        qid, name, value = row[0], row[1], row[2]

        records.append((name, value))

# sort by name
records.sort(key=lambda r: r[0].casefold())

string_pool = bytearray()
offsets = {}

def intern(s):
    off = offsets.get(s)
    if off is not None:
        return off

    off = len(string_pool)
    string_pool.extend(s.encode("utf-8"))
    string_pool.append(0)
    offsets[s] = off
    return off

binary_records = []

for name, value in records:
    binary_records.append((intern(name), intern(value)))

header_size = 16
record_size = 8
strings_offset = header_size + len(binary_records) * record_size

with open(output_file, "wb") as f:
    f.write(struct.pack("<IIII",
                        MAGIC,
                        VERSION,
                        len(binary_records),
                        strings_offset))

    for n_off, v_off in binary_records:
        f.write(struct.pack("<II", n_off, v_off))

    f.write(string_pool)

print("records:", len(binary_records))
print("unique strings:", len(offsets))
print("output:", output_file)
