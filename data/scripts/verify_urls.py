#!/usr/bin/env python3

import asyncio
import aiohttp
import csv
import sys
import signal
import socket
import aiodns
from urllib.parse import urlparse

CONCURRENCY = 80
TIMEOUT = aiohttp.ClientTimeout(total=5)

REJECTED_FILE = "rejected.tsv"

stop_requested = False
processed = 0

dns_cache = {}
result_cache = {}

# ============================================================
# SIGNALS
# ============================================================

def signal_handler(sig, frame):
    global stop_requested
    stop_requested = True
    print("\nStopping...")


signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

# ============================================================
# HELPERS
# ============================================================

def normalize(url):
    url = url.strip()
    if not url:
        return None
    if not url.startswith(("http://", "https://")):
        url = "https://" + url
    return url


def host(url):
    try:
        return urlparse(url).netloc.lower()
    except Exception:
        return None


def variants(h):
    base = h.replace("www.", "")
    return [
        f"https://{base}",
        f"https://www.{base}",
        f"http://{base}",
        f"http://www.{base}",
    ]

# ============================================================
# DNS
# ============================================================

async def resolve(resolver, hostname):
    if hostname in dns_cache:
        return dns_cache[hostname]

    try:
        result = await resolver.gethostbyname(hostname, socket.AF_INET)
        ip = result.addresses[0] if result.addresses else None
        dns_cache[hostname] = ip
        return ip
    except Exception:
        dns_cache[hostname] = None
        return None

# ============================================================
# HTTP
# ============================================================

async def fetch(session, url):
    try:
        async with session.head(url, allow_redirects=True) as r:
            if 200 <= r.status < 400:
                return True
            if r.status != 405:
                return False
    except Exception:
        pass

    try:
        async with session.get(url, allow_redirects=True) as r:
            return 200 <= r.status < 400
    except Exception:
        return False

# ============================================================
# CORE CHECK
# ============================================================

async def check(session, resolver, h):
    if h in result_cache:
        return result_cache[h]

    base = h.replace("www.", "")

    ip = await resolve(resolver, base)
    if not ip:
        result_cache[h] = False
        return False

    for u in variants(base):
        ok = await fetch(session, u)
        if ok:
            result_cache[h] = True
            return True

    result_cache[h] = False
    return False

# ============================================================
# WORKER
# ============================================================

async def worker(queue, session, resolver):
    global processed, results

    while True:
        item = await queue.get()
        if item is None:
            queue.task_done()
            return

        row, h = item

        ok = await check(session, resolver, h)

        results.append((row, ok))

        processed += 1
        if processed % 1000 == 0:
            print(f"processed {processed:,}")

        queue.task_done()

# ============================================================
# MAIN
# ============================================================

async def main():
    global results

    if len(sys.argv) != 3:
        print("usage: python3 verify_urls.py input.tsv output.tsv")
        sys.exit(1)

    infile = sys.argv[1]
    outfile = sys.argv[2]

    results = []

    resolver = aiodns.DNSResolver()

    rows = []

    with open(infile, "r", encoding="utf-8") as f:
        r = csv.reader(f, delimiter="\t")

        for row in r:
            if len(row) < 3:
                continue

            u = normalize(row[2])
            if not u:
                continue

            h = host(u)
            if not h:
                continue

            rows.append((row, h))

    print(f"rows: {len(rows):,}")

    queue = asyncio.Queue()

    for item in rows:
        queue.put_nowait(item)

    connector = aiohttp.TCPConnector(limit=CONCURRENCY)

    async with aiohttp.ClientSession(
        connector=connector,
        timeout=TIMEOUT,
        headers={"User-Agent": "FastVerifier/3.0"},
    ) as session:

        workers = [
            asyncio.create_task(worker(queue, session, resolver))
            for _ in range(CONCURRENCY)
        ]

        await queue.join()

        for _ in workers:
            queue.put_nowait(None)

        await asyncio.gather(*workers)

    with open(outfile, "w", newline="", encoding="utf-8") as vf, \
         open(REJECTED_FILE, "a", newline="", encoding="utf-8") as rf:

        vw = csv.writer(vf, delimiter="\t")
        rw = csv.writer(rf, delimiter="\t")

        for row, ok in results:
            if ok:
                vw.writerow(row)
            else:
                rw.writerow(row)

    print("\nDONE")
    print(f"processed: {processed:,}")


if __name__ == "__main__":
    asyncio.run(main())
