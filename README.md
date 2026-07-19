# web_i2c

**An in-browser I2C bus console, monitor, and controller for ESPHome.**

`web_i2c` turns an ESP8266 or ESP32 into a live I2C debugging tool you reach from
a browser. No logic analyzer, no extra hardware, no software to install: the ESP
serves a web page over WebSocket, runs the real I2C transactions on its own
SDA/SCL pins, and streams what it sees back to your browser as you watch.

It is **not** an electrical logic analyzer. It taps the bus at the
**transaction/API level**: the ESP is the bus master, and the component observes
and relays every transaction that passes through its own I2C driver. That makes
it a *console + monitor + controller* for the device that is already in your
circuit -- uniquely able to reconstruct an SSD1306 screen image from the bus,
take over the bus to compose on a display, and monitor the host's health while
it monitors traffic.

---

## Highlights

- **Transparent tap / monitor** -- every transaction relayed by the ESP is
  logged live, with per-transaction microsecond timestamps and inter-transaction
  deltas.
- **Device-aware decoder** -- decodes I2C transactions with SSD1306/SH1106
  semantics (Set Column Address, Charge Pump, etc.), grouping ESPHome's
  byte-by-byte command writes back into logical commands.
- **Framebuffer mirror** -- reconstructs the actual SSD1306 image from the pixel
  writes on the bus and draws it in the browser. No logic analyzer does this.
- **Bus takeover + composer** -- stop relaying device traffic and drive the bus
  yourself: draw or load an image, send it to the display, run test patterns.
- **Manual terminal** -- scan the bus, read/write registers, dump register maps.
- **Trigger -> capture** -- arm on an address, error, or byte pattern; the log
  freezes on the match and highlights it, keeping the pre-trigger context.
- **Live cockpit** -- semicircle gauges (bus traffic, loop rate, free heap), a
  reactive wiring diagram, and a plain-language status banner.
- **Export** -- CSV (UTF-8, Excel-ready) and JSON, including the ESP-side
  timestamps and deltas.
- **Serial mirror** -- mirror the log to a UART/USB serial port (Chrome/Edge).
- **Health guardrails** -- dynamic CPU throttle, bounded buffers, boot
  protection, and a bus-recovery routine for a stuck line.

---

## Requirements

- ESPHome (2024.x or newer recommended).
- An ESP8266 or ESP32 board on your network (Wi-Fi).
- An `i2c:` bus defined in your YAML.

Works on both platforms. The ESP8266 (single core, ~80 KB RAM) handles the tool
comfortably; heavy continuous traffic is throttled to protect the loop. An ESP32
sustains much higher message rates.

---

## Installation

Add the component from GitHub via `external_components`:

```yaml
external_components:
  - source: github://kaboom748/web_i2c
    components: [web_i2c]
```

Then define an I2C bus and the terminal:

```yaml
# the real I2C bus of the ESP (wire your SSD1306 / sensor here)
i2c:
  sda: D2        # GPIO4 on a D1 mini
  scl: D1        # GPIO5 on a D1 mini
  scan: true     # log addresses found at boot

# the in-browser I2C console
web_i2c:
  id: i2c_debug
  port: 8095     # open http://<esp-ip>:8095/ in a browser
```

Complete, ready-to-flash examples are in [`example/`](example/) for both
ESP8266 and ESP32.

### Configuration options

| Option     | Required | Description                                              |
|------------|----------|----------------------------------------------------------|
| `id`       | no       | Component ID (also usable as an `i2c_id`, see below).    |
| `i2c_id`   | no*      | The `i2c:` bus to drive. Auto-selected if only one bus.  |
| `port`     | **yes**  | TCP port of the embedded WebSocket server.               |

\* If your config has more than one `i2c:` bus, give each bus an `id` and set
`i2c_id` explicitly.

### Monitoring other components' traffic

`web_i2c` is also an `i2c::I2CBus` decorator. Point another component's `i2c_id`
at it and every transaction that component makes is relayed to the real bus
**and** captured by the tap:

```yaml
i2c:
  id: real_bus
  sda: D2
  scl: D1

web_i2c:
  id: i2c_debug
  i2c_id: real_bus
  port: 8095

display:
  - platform: ssd1306_i2c
    i2c_id: i2c_debug      # route the display through the tap
    address: 0x3C
    # ...
```

Now the display's traffic shows up live in the browser, and you can mirror its
framebuffer or take over the bus.

---

## Using the interface

Open `http://<esp-ip>:<port>/`. The top status bar shows live pills: WebSocket,
real bus state, tap mode, device count, and a saturation alert.

### Tabs

- **Commands** -- manual bus operations (scan, read, write, register access).
- **Display** -- SSD1306/SH1106 command helpers.
- **Init / Scroll** -- display init and scroll helpers.
- **Info** -- the cockpit: gauges, wiring diagram, and the tap Monitor & Control
  panel.
- **System** -- exact numeric readouts (bus config, monitor counters, bus lines,
  memory, device, server).

### The composer (top of the page)

Draw on a virtual SSD1306, load a photo (converted to 1-bit with adjustable
threshold and dithering), or apply Fill / Checkerboard patterns. Nothing is sent
until you press **Send to display** -- the composer fills the preview, you review
it, then send.

### Manual command syntax

Type these in the terminal (or use the buttons):

| Command                      | Action                                            |
|------------------------------|---------------------------------------------------|
| `SCAN`                       | Scan the bus for devices.                         |
| `W <addr> <b0> <b1> ...`     | Write bytes to a device (hex).                    |
| `R <addr> <len>`             | Read `len` bytes.                                 |
| `WR <addr> <reg> <b0> ...`   | Write to a register.                              |
| `RR <addr> <reg> <len>`      | Read `len` bytes from a register.                 |

Addresses and bytes are hexadecimal (e.g. `W 3C 00 AF`). ACK/NACK is reported
per transaction.

---

## The LOG panel

Every relayed transaction and manual result appears here, newest at the bottom,
each with a delta timestamp:

```
+118us   >> W 0x3C  cmd   00 21 00 7F  OK
+545us   >> W 0x3C  data  128 x 17 B   OK
+199ms   >> W 0x3C  cmd   00 22 00 07  OK
```

Each line is self-describing:

- **`cmd`** -- a command/register write (bytes shown when captured).
- **`data`** -- a bulk pixel stream to the display (control byte 0x40); shown as
  a readable size, not decoded (it would flood).
- **`read`** -- a read transaction.

Toolbar (left to right): **Pause** (freezes the display; a badge shows how many
lines were buffered while paused -- nothing is lost), **Errors** (show only
NACK/TIMEOUT/... lines), **Filter** (substring), **Trigger** (see below),
**CSV** / **JSON** export, and **Clear**.

### Timestamps and deltas

Timestamps are taken **on the ESP** with `micros()` at the moment the
transaction is observed -- free of Wi-Fi/WebSocket jitter. The log shows the
**delta from the previous transaction** (`+12.3ms`), which reveals the rhythm of
the bus: polling intervals, refresh bursts, and abnormal gaps. The `micros()`
wrap (~71 minutes) is handled correctly. Exports include the raw `esp_us` and
`delta_ms` columns.

### Trigger -> capture

Type a condition in the **trigger** field (an address like `0x50`, an error like
`NACK`, or any byte pattern) and arm it. When a matching line arrives, the log
**auto-pauses**, the triggering line is **highlighted**, and the pre-trigger
context stays visible. Click the trigger button again to re-arm. This catches
transient events you would otherwise miss as they scroll past.

---

## The decoder

Click the arrow between the LOG and the Decoder to decode the current log.
Command transactions are broken down byte-by-byte with device semantics:

```
+118us
START
0x3C     ADDR . Write
00       CONTROL . command stream
21 00 7F Column address 0..127
22 00 07 Page address 0..7
STOP
```

Because ESPHome sends each SSD1306 command byte as its own I2C write, the decoder
**merges consecutive command writes** to reconstruct multi-byte commands -- but
only when the capture is reliable. It is honest about its limits:

- **In Batch delivery** the sequence is coalesced, so the decoder shows a note to
  switch to Live for exact decoding.
- **When the bus is flooded** (transactions dropped by the throttle), the
  captured sequence has gaps; the decoder says so instead of inventing values.
- **For unknown (non-display) devices** it shows the raw bytes without pretending
  to know the register meanings.

---

## Capture modes (Monitor & Control panel)

- **Detail: Summary / Full** -- Summary logs only a byte count per transaction;
  Full streams the bytes of command writes (decodable). Pixel/bulk writes are
  always counted only.
- **Delivery: Live / Batch** -- Live sends one message per transaction (exact
  order, needed for precise decoding). Batch coalesces and de-duplicates to a few
  messages per second (much lighter, but scrambles command order).
- **Bus: Relay / Take over** -- Relay is normal operation. Take over stops
  relaying device traffic to the bus so your terminal commands own it -- compose
  on the display without its component overwriting you.
- **Screen: Off / Mirror** -- Mirror reconstructs the display's framebuffer from
  the pixel writes on the bus and draws it below.
- **Filter** -- restrict the tap to a single address (blank = all).

---

## Serial mirror

Connect a USB/UART serial port (Chrome/Edge, Web Serial API) to mirror the log
lines to a terminal for external capture. The serial output matches the on-screen
log exactly, deltas included. The output is UTF-8; set your terminal to UTF-8.

---

## Guardrails and recovery

- **Dynamic CPU throttle** -- the tap sends at most one log per a multiple of the
  smoothed loop period, so it always yields CPU to the web server and never
  starves the Wi-Fi handshake. On a flooded bus it drops log output (never bus
  data) and marks itself "skipping". *Log lines skipped* is display throttling,
  **not** lost bus traffic.
- **Bounded buffers** -- fixed-size aggregation and output buffers; no dynamic
  allocation in the hot path.
- **Boot protection** -- heavy writes are deferred during the first seconds after
  boot so a slow transfer cannot block the loop during the Wi-Fi handshake.
- **Bus recovery** -- if a line is held low (stuck device, missing pull-up), the
  *Recover bus* button clocks SCL to release SDA.

---

## Known limitations

`web_i2c` is a transaction-level tap, not an electrical analyzer. Being honest
about this is a feature:

- **No electrical view.** It cannot see waveforms, glitches, rise/fall times, the
  true ACK bit (ACK is *inferred* from the driver's return code), clock
  stretching, bus contention, or timing at the signal level. For those, use a
  logic analyzer.
- **Only its own bus.** It sees traffic that passes through its own I2C driver
  (the ESP as master) or manual commands. It cannot passively sniff traffic
  between two other chips it is not part of.
- **Continuous display refresh on ESP8266.** ~700 tx/s saturates the ESP8266; the
  throttle drops most transactions, so precise command decoding is not possible
  in either mode. Use the framebuffer mirror to see the screen, or capture a
  lighter/one-shot sequence (boot init, a manual command) for exact decoding. An
  ESP32 sustains far higher rates.
- **Serial mirror is Chrome/Edge only** (Web Serial API).
- **Tested configuration.** Developed and exercised on a Wemos D1 mini (ESP8266)
  and an ESP32 dev board with an SSD1306 at 0x3C. Other displays, sensors, and
  boards are supported structurally but not all exercised -- feedback welcome.

---

## Tuning

Compile-time constants (top of `web_i2c.cpp` / `web_i2c.h`) let you trade
responsiveness for CPU/RAM: tap backlog, throttle multiplier, batch interval and
slots, boot grace window, and the framebuffer refresh interval. Defaults are
conservative for the ESP8266.

---

## How it works

The browser never speaks I2C. It sends command text over WebSocket; the ESP runs
the I2C and returns results. `WebI2C` is both the WebSocket server and an
`i2c::I2CBus` decorator: it overrides `write_readv` to observe and relay every
transaction, while manual terminal commands call the underlying bus directly.
The page (HTML + JS) is served from PROGMEM as a single document. See
[`docs/DEVELOPMENT_NOTES.md`](docs/DEVELOPMENT_NOTES.md) for the design decisions
and the pitfalls solved along the way.

---

## License

MIT. See [LICENSE](LICENSE).
