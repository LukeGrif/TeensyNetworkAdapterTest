# Teensy Ethernet Adapter Test

Firmware for a **Teensy 4.1 + Ethernet adapter kit** that brings the port up,
answers `ping`, and serves a live dashboard web page so you can confirm the
adapter works.

## What it does

- Uses a fixed IP address, **192.168.2.70**, so it works with the Ethernet
  cable plugged straight into your PC. No router or switch needed.
- Prints the IP, MAC and link status to the Serial Monitor
- Replies to **ICMP ping**
- Serves a web page at `http://192.168.2.70/` showing link status,
  speed/duplex, uptime and a live round-trip-time graph with packet-loss stats
- `GET /ping` returns `pong`, and `GET /api/status` returns JSON
- LED: **solid** = cable link up, **slow blink** = no link,
  **fast blink** = Ethernet failed to start

## Hardware

1. Solder the 2x3 header onto the Teensy 4.1's Ethernet pads (next to the USB port).
2. Connect the adapter board with the ribbon cable. Check pin 1 lines up.
3. Plug an Ethernet cable from the adapter straight into your PC's Ethernet
   port. A normal cable works; you don't need a crossover cable.

## Set your PC's IP address

There's no router to hand out addresses, so give your PC's **Ethernet**
port (not Wi-Fi) a fixed address on the same subnet as the Teensy:

| Setting | Value |
|---|---|
| IP address | `192.168.2.10` (anything `192.168.2.x` except `.70`) |
| Subnet mask | `255.255.255.0` |
| Gateway / DNS | leave blank |

- **Windows:** Settings → Network & Internet → Ethernet → IP assignment →
  Edit → Manual → turn on IPv4, then enter the values above.
- **macOS:** System Settings → Network → Ethernet → Details → TCP/IP →
  Configure IPv4: Manually.
- **Linux:** `sudo ip addr add 192.168.2.10/24 dev eth0` (swap `eth0` for your
  interface name).

## Build and upload

### Arduino IDE (with Teensyduino)

1. In the Library Manager, install **QNEthernet** (by Shawn Silverman), version 0.37 or newer.
2. Open `TeensyEthernetTest/TeensyEthernetTest.ino`.
3. Set Tools → Board to **Teensy 4.1**, then click Upload.
4. Open the Serial Monitor at 115200 baud.

### PlatformIO

```sh
pio run -t upload
pio device monitor
```

## Test it

The Serial Monitor shows something like:

```
=== Network up ===
  MAC:      04:E9:E5:xx:xx:xx
  IP:       192.168.2.70
  ...
  Web page: http://192.168.2.70/  (or http://teensy-eth.local/)
  Test:     ping 192.168.2.70
```

Then on your PC:

```sh
ping 192.168.2.70                         # macOS / Linux
ping -t 192.168.2.70                      # Windows (runs until Ctrl+C)
python3 tools/ping_test.py 192.168.2.70   # ICMP + HTTP test with a PASS/FAIL result
```

Or open `http://192.168.2.70/` in a browser to watch the dashboard. Unplugging
the cable shows timeouts and packet loss on the graph, and a `[link] DOWN`
message on Serial.

To use a different address, change `kStaticIP` near the top of the sketch.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Fast LED blink, `Ethernet.begin() failed` | Not a Teensy 4.1. The 4.0 has no Ethernet PHY. |
| `[link] DOWN`, no link lights | Ribbon cable on backwards or a bad solder joint on the header. Try another cable. |
| Link up but ping fails | PC's Ethernet port isn't set to a `192.168.2.x` address. Check with `ipconfig` (Windows) or `ip addr` (Linux). |
| Ping still fails on Windows | The firewall may treat the new network as Public. Set the network to Private, or try the web page instead. |
| Ping goes out over Wi-Fi | Your Wi-Fi uses `192.168.2.x` too. Turn Wi-Fi off while testing, or change `kStaticIP` to another subnet. |
| `teensy-eth.local` doesn't resolve | mDNS isn't supported on this PC (older Windows). Use the IP. |
