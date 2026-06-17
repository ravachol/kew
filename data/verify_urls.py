#!/usr/bin/env python3

import csv
import sys
import socket
import http.client
import time
import random
import threading
from urllib.parse import urlparse
from concurrent.futures import ThreadPoolExecutor, as_completed

# -----------------------
# CONFIG
# -----------------------

TIMEOUT = 3
MAX_WORKERS = 8

MIN_INTERVAL = 0.25   # 4 requests/sec max globally
JITTER = 0.2

socket.setdefaulttimeout(TIMEOUT)

# -----------------------
# GLOBAL RATE LIMIT STATE
# -----------------------

lock = threading.Lock()
last_request_time = 0.0

domain_cache = {}
fail_count = 0


def rate_limit():
    global last_request_time

    with lock:
        now = time.time()
        elapsed = now - last_request_time

        wait = MIN_INTERVAL - elapsed
        if wait > 0:
            time.sleep(wait)

        # jitter AFTER waiting (so we don't break pacing guarantees too much)
        if JITTER > 0:
            time.sleep(random.random() * JITTER)

        last_request_time = time.time()


def normalize_url(url):
    if not url.startswith("http"):
        return "https://" + url
    return url


def check_url(url):
    global fail_count

    try:
        url = normalize_url(url)
        parsed = urlparse(url)
        host = parsed.netloc

        if not host:
            return False

        if host in domain_cache:
            return domain_cache[host]

        # DNS check (no rate limit needed here)
        socket.gethostbyname(host)

        rate_limit()

        path = parsed.path or "/"

        # HTTPS attempt
        try:
            conn = http.client.HTTPSConnection(host, timeout=TIMEOUT)
            conn.request("HEAD", path)
            res = conn.getresponse()
            conn.close()

            ok = 200 <= res.status < 400
            domain_cache[host] = ok

            if not ok:
                fail_count += 1

            return ok

        except Exception:
            pass

        rate_limit()

        # HTTP fallback
        try:
            conn = http.client.HTTPConnection(host, timeout=TIMEOUT)
            conn.request("HEAD", path)
            res = conn.getresponse()
            conn.close()

            ok = 200 <= res.status < 400
            domain_cache[host] = ok

            if not ok:
                fail_count += 1

            return ok

        except Exception:
            fail_count += 1
            domain_cache[host] = False
            return False

    except Exception:
        fail_count += 1
        return False


def worker(row):
    key, name, url = row
    return (row, check_url(url))


if len(sys.argv) != 3:
    print("usage: safe_filter.py input.tsv output.tsv")
    sys.exit(1)

inp = sys.argv[1]
out = sys.argv[2]

rows = []

with open(inp, "r", encoding="utf-8") as f:
    r = csv.reader(f, delimiter="\t")
    for row in r:
        if len(row) < 3:
            continue
        rows.append((row[0], row[1], row[2]))

total = len(rows)
kept = 0

with open(out, "w", encoding="utf-8", newline="") as f_out:
    w = csv.writer(f_out, delimiter="\t")

    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as ex:
        futures = [ex.submit(worker, row) for row in rows]

        for fut in as_completed(futures):
            (key, name, url), ok = fut.result()

            if ok:
                w.writerow([key, name, url])
                kept += 1

print(f"total: {total}")
print(f"kept: {kept}")
print(f"removed: {total - kept}")
print(f"cached domains: {len(domain_cache)}")
