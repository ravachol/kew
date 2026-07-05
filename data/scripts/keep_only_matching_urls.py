#!/usr/bin/env python3

import csv
import re
import sys
from urllib.parse import urlparse

# Words to ignore when matching
STOPWORDS = {
    "the",
    "a",
    "an",
    "and",
    "of",
    "for",
    "in",
    "on",
    "at",
    "to",
    "by",
    "with",
    "&",
}

def normalize_text(text):
    """Return a list of lowercase alphanumeric words."""
    return re.findall(r"[a-z0-9]+", text.lower())


def artist_words(name):
    """Extract significant words from the artist name."""
    return [w for w in normalize_text(name) if w not in STOPWORDS]


def normalize_url(url):
    """Convert URL into a searchable lowercase string."""
    parsed = urlparse(url)
    return "".join(normalize_text(parsed.netloc + parsed.path))


def matches(name, url):
    """Return True if the URL appears to belong to the artist."""
    words = artist_words(name)
    url_text = normalize_url(url)

    if not words:
        return False

    # Any significant word appears in the URL
    if any(word in url_text for word in words):
        return True

    # Initialism (Nine Inch Nails -> NIN, Red Hot Chili Peppers -> RHCP)
    initials = "".join(word[0] for word in words)
    if len(initials) >= 2 and initials in url_text:
        return True

    return False


def main():
    if len(sys.argv) != 3:
        print("Usage: python remove_non-matching_urls.py input.tsv output.tsv")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    kept = 0
    removed = 0

    with open(input_file, "r", encoding="utf-8", newline="") as infile, \
         open(output_file, "w", encoding="utf-8", newline="") as outfile:

        reader = csv.reader(infile, delimiter="\t")
        writer = csv.writer(outfile, delimiter="\t")

        for row in reader:
            if len(row) < 3:
                continue

            key, artist, homepage = row[:3]

            if matches(artist, homepage):
                writer.writerow(row)
                kept += 1
            else:
                removed += 1

    print(f"Done.")
    print(f"Kept:    {kept}")
    print(f"Removed: {removed}")


if __name__ == "__main__":
    main()
