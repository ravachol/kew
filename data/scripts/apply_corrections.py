#!/usr/bin/env python3

import csv
import argparse


def read_tsv(filename):
    with open(filename, "r", encoding="utf-8", newline="") as f:
        return list(csv.reader(f, delimiter="\t"))


def write_tsv(filename, rows):
    with open(filename, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f, delimiter="\t", lineterminator="\n")
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(
        description="Apply corrections to an artists TSV."
    )
    parser.add_argument("artists_raw")
    parser.add_argument("corrections")
    parser.add_argument("artists_out")
    args = parser.parse_args()

    artists = read_tsv(args.artists_raw)
    corrections = read_tsv(args.corrections)

    # Preserve original order
    artists_by_key = {}
    ordered_keys = []

    for row in artists:
        if len(row) < 3:
            continue
        key, name, homepage = row[:3]
        artists_by_key[key] = [key, name, homepage]
        ordered_keys.append(key)

    for row in corrections:
        # Pad short rows
        while len(row) < 3:
            row.append("")

        key, name, homepage = row[:3]

        # New artist
        if key == "":
            if name == "DELETE":
                continue

            new_key = "".join(name.split())
            artists_by_key[new_key] = [new_key, name, homepage]
            ordered_keys.append(new_key)
            continue

        # Existing artist
        if name == "DELETE":
            if key in artists_by_key:
                del artists_by_key[key]
            continue

        artists_by_key[key] = [key, name, homepage]
        if key not in ordered_keys:
            ordered_keys.append(key)

    output = [
        artists_by_key[key]
        for key in ordered_keys
        if key in artists_by_key
    ]

    write_tsv(args.artists_out, output)


if __name__ == "__main__":
    main()
