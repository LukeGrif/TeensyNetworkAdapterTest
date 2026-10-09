#!/usr/bin/env python3
"""Ping the Teensy from your PC: ICMP ping, then HTTP round-trips to /ping.

Usage:  python3 tools/ping_test.py <teensy-ip> [count]
No extra packages needed (standard library only).
"""
import platform
import subprocess
import sys
import time
import urllib.request


def icmp_ping(host, count):
    flag = "-n" if platform.system() == "Windows" else "-c"
    print(f"--- ICMP ping {host} ({count} packets) ---")
    result = subprocess.run(["ping", flag, str(count), host])
    return result.returncode == 0


def http_ping(host, count):
    print(f"\n--- HTTP ping http://{host}/ping ({count} requests) ---")
    times = []
    for i in range(count):
        start = time.perf_counter()
        try:
            with urllib.request.urlopen(f"http://{host}/ping", timeout=2) as r:
                body = r.read().decode().strip()
            ms = (time.perf_counter() - start) * 1000
            times.append(ms)
            print(f"  {i + 1:3d}: {body}  {ms:6.1f} ms")
        except Exception as e:
            print(f"  {i + 1:3d}: FAILED ({e})")
        time.sleep(0.2)
    lost = count - len(times)
    if times:
        print(f"  min/avg/max = {min(times):.1f}/{sum(times) / len(times):.1f}/"
              f"{max(times):.1f} ms, lost {lost}/{count}")
    return lost == 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    host = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    icmp_ok = icmp_ping(host, count)
    http_ok = http_ping(host, count)
    print("\nRESULT:", "PASS" if icmp_ok and http_ok else "FAIL",
          f"(ICMP {'ok' if icmp_ok else 'failed'}, HTTP {'ok' if http_ok else 'failed'})")
    sys.exit(0 if icmp_ok and http_ok else 1)


if __name__ == "__main__":
    main()
