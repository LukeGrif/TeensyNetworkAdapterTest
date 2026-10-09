# Teensy Ethernet Adapter Test

Firmware for a **Teensy 4.1 + Ethernet adapter kit** that brings the port up,
answers `ping`, and serves a live dashboard web page so you can confirm the
adapter works.

## What it does

- Gets an IP over **DHCP** (or a static IP, if you turn it on)
- Prints the IP, MAC, mask, gateway and DNS to the Serial Monitor
- Replies to **ICMP ping**
- Serves a web page at `http://<teensy-ip>/` (also `http://teensy-eth.local/`)
  showing link status, speed/duplex, uptime and a live round-trip-time graph
  with packet-loss stats
- `GET /ping` returns `pong`, and `GET /api/status` returns JSON
- LED: **solid** = link up with an IP, **slow blink** = waiting for
  link/DHCP, **fast blink** = Ethernet failed to start

## Hardware

1. Solder the 2x3 header onto the Teensy 4.1's Ethernet pads (next to the USB port).
2. Connect the adapter board with the ribbon cable. Check pin 1 lines up.
3. Plug an Ethernet cable into a router or switch that runs DHCP.

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
  IP:       192.168.1.57
  ...
  Web page: http://192.168.1.57/  (or http://teensy-eth.local/)
  Test:     ping 192.168.1.57
```

Then on your PC:

```sh
ping 192.168.1.57                         # macOS / Linux
ping -t 192.168.1.57                      # Windows (runs until Ctrl+C)
python3 tools/ping_test.py 192.168.1.57   # ICMP + HTTP test with a PASS/FAIL result
```

Or open `http://192.168.1.57/` in a browser to watch the dashboard. Unplugging
the cable shows timeouts and packet loss on the graph, and a `[link] DOWN`
message on Serial.

## Static IP

If there's no DHCP server (for example, the Teensy is cabled straight to your
PC), edit the top of the sketch:

```cpp
constexpr bool kUseStaticIP = true;
const IPAddress kStaticIP{192, 168, 1, 177};
```

Give your PC a static address on the same subnet (for example,
`192.168.1.10` / `255.255.255.0`).

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Fast LED blink, `Ethernet.begin() failed` | Not a Teensy 4.1. The 4.0 has no Ethernet PHY. |
| `[link] DOWN`, no link lights | Ribbon cable on backwards or a bad solder joint on the header. Try another cable or switch port. |
| Link up but no IP | No DHCP server on that network. Use a static IP. |
| IP shown but ping fails | PC is on a different subnet or VLAN, or a firewall/guest Wi-Fi blocks local traffic. |
| `teensy-eth.local` doesn't resolve | mDNS isn't supported on this PC (older Windows). Use the IP. |
