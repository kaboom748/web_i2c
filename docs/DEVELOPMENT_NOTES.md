# web_i2c -- Development Notes

Design decisions and the pitfalls solved while building `web_i2c`. Written for
anyone extending the component (or debugging a regression). Format for each
pitfall: **Symptom -> Cause -> Fix -> Lesson.**

---

## Architecture

`WebI2C` is two things at once:

1. **A WebSocket server** that serves a single HTML+JS page (from PROGMEM) and
   streams events to the browser. The browser sends command text; the ESP runs
   the I2C and returns results. The browser never speaks I2C.

2. **An `i2c::I2CBus` decorator.** It overrides `write_readv` so other components
   can point their `i2c_id` at it; every transaction is observed and relayed to
   the real bus. Manual terminal commands call the underlying bus directly
   (`cmd_write_`), bypassing `write_readv`.

Capture is at the **transaction/API level**, not electrical. The ESP is bus
master; ACK is inferred from the driver's return code, not read off the wire.

Two paths reach the bus:
- Device components -> `write_readv` (observed + relayed).
- Manual terminal commands -> the bus directly (so they work even during
  takeover, when device traffic is blocked).

The frontend is plain JS in one `<script>`. The page source lives in a raw
string literal (`R"HTMLDOC( ... )HTMLDOC"`) in `page.cpp`.

---

## Hard constraints (respect these when editing)

- **`page.cpp` must stay ASCII-clean.** Non-ASCII in a PROGMEM string literal is
  risky; JS uses `\uXXXX` escapes and HTML entities for any non-ASCII glyph
  (`>>` guillemet, `x` multiply, `us` micro, etc. are escaped in source).
- **Balance:** the raw-string delimiter count, C++ braces, and the embedded
  `<script>` must all stay valid. Validate with a brace count, an HTMLDOC
  delimiter count, and `node --check` on the extracted script.
- **`web_i2c.h` changes require a CLEAN build.** Changing a struct (e.g. adding
  a field to `TapEntry`) changes the ABI; an incremental build links stale
  objects. `page.cpp`/`.cpp`-only changes are incremental-safe.
- **The frontend can't be compiled here.** Validate structurally and host-test
  the pure logic in Node; a real `esphome compile` + flash is the final check.

---

## Pitfalls solved

### 1. `HEX` macro collision
**Symptom:** build errors / wrong output around a `HEX` identifier.
**Cause:** Arduino defines `HEX` as a macro; using it as a name collides.
**Fix:** renamed the internal helper to `HEXD`.
**Lesson:** avoid Arduino's reserved macro names (`HEX`, `B1`, `min`, etc.).

### 2. Stack overflow on deep call paths
**Symptom:** crashes/resets when building an event JSON from inside a deep call.
**Cause:** a large stack buffer overflowed the small ESP8266 stack when reached
from a deep path.
**Fix:** build response strings on the heap (`std::string`), like the other
responses.
**Lesson:** the ESP8266 stack is tiny; do not put big buffers on it.

### 3. Heap fragmentation
**Symptom:** free heap drops over time; instability under load.
**Cause:** repeated allocation/free of variable-size buffers fragments the heap.
**Fix:** fixed-size buffers for the hot path (aggregation slots, output backlog);
per-message `std::string` is short-lived and bounded.
**Lesson:** on the hot path, prefer fixed buffers; keep transient allocations
small and short-lived.

### 4. Framebuffer mirror -- vertical offset / wrong image
**Symptom:** the mirrored screen image was shifted or garbled.
**Cause:** the SSD1306 addressing is stateful (column/page pointers, addressing
mode, ranges); decoding pixel bytes without tracking that state misplaces them.
**Fix:** a full addressing state machine (`fb_mode_`, `fb_col_`, `fb_page_`,
range registers, a command param skip-table). Both `write_readv` and the manual
`cmd_write_` feed it, so takeover + manual composing also mirror correctly.
**Lesson:** to reconstruct display RAM you must emulate the controller's
addressing, not just collect bytes.

### 5. Manual composing didn't mirror during takeover
**Symptom:** in takeover, images composed via manual commands didn't appear in
the mirror.
**Cause:** manual commands bypass `write_readv`, so the mirror never saw them.
**Fix:** `cmd_write_` also feeds the framebuffer state machine.
**Lesson:** if two code paths reach the bus, both must feed any observer.

### 6. Dynamic CPU throttle (the reason Live doesn't crash)
**Symptom:** heavy I2C (a refreshing display) starved the web server / Wi-Fi.
**Cause:** sending a WebSocket frame per transaction at ~700 tx/s overwhelms the
single-core ESP8266.
**Fix:** send at most one log per `K * smoothed-loop-period`. A fast loop (idle)
sends freely; a bogged loop sends almost nothing, always yielding CPU. Excess is
dropped and marked "skipping".
**Lesson:** rate-limit against real loop headroom, not a fixed number.

### 7. Boot handshake starved by a big transfer
**Symptom:** boot instability when a display pushes a full framebuffer early.
**Cause:** a slow large write blocks the loop during the Wi-Fi handshake.
**Fix:** defer heavy writes during a boot grace window.
**Lesson:** protect the boot/handshake window from long blocking operations.

### 8. Batch coalescing without allocation
**Design:** Batch mode de-duplicates identical transactions into a fixed slot
array (`tap_agg_`), flushed as one message a few times per second. When the slot
array is full of distinct patterns, extras are dropped (bounded).
**Trade-off:** de-dup **scrambles the command order** -- see pitfalls 14-15.

### 9. Microsecond timestamps + `micros()` wrap
**Symptom:** browser-side arrival time was polluted by Wi-Fi/WebSocket jitter.
**Fix:** timestamp on the ESP with `micros()` at observation time; send it. The
frontend shows the delta from the previous transaction. `micros()` wraps every
~71 min; the delta uses unsigned 32-bit arithmetic (`(us - last) >>> 0`) so it
stays correct across the wrap. Verified in host tests.
**Lesson:** timestamp at the source; handle counter wrap with unsigned math.

### 10. Decoder broke after adding timestamps
**Symptom:** decoder showed "TAP (not decoded)" for every line.
**Cause:** the decoder read each log line via `d.textContent`, which now included
the prepended delta span (`+105us>> W 0x3C...`). Its parser required the line to
start with the tap marker at index 0, which was no longer true.
**Fix:** store the raw message text on the line (`data-raw`) and decode from
that, not from the rendered cell.
**Lesson:** never parse a rendered DOM cell; keep a clean data source separate
from presentation.

### 11. Decoder missing timestamps
**Symptom:** decoded blocks had no timing, so they didn't line up with the log.
**Fix:** store the formatted delta on the line (`data-dt`); the decoder reads it
and prepends it to each block.
**Lesson:** carry the structured data through, not the formatted string.

### 12. Serial mirror didn't match the log
**Symptom:** the serial output lacked the delta the log displayed.
**Cause:** the delta was added only in the DOM and the serial prefix, at
different points.
**Fix:** the serial mirror prepends the same delta as the display.
**Lesson:** one source of truth for a value shown in two places.

### 13. `kind` classification (cmd / data / read)
**Design:** the ESP classifies each transaction from the control byte: a
`0x40`/`0xC0` write is a `data` (pixel) stream; a read is `read`; otherwise
`cmd`. Sent as a `kind` field (a `TapEntry` field for Batch). This makes the log
self-describing **in both Summary and Full** -- the frontend can't tell cmd from
data in Summary (no bytes), but the backend can. For non-display devices `data`
never appears (they don't use `0x40`); everything is `cmd`/`read`, which is
honest.
**Lesson:** classify where the information exists (the backend), not where it is
inconvenient (the frontend).

### 14. Display format changes kept breaking the decoder and export
**Symptom:** making the log descriptive (cmd/data labels, readable sizes) would
break the decoder and CSV export, which both re-parsed the log text.
**Cause:** three consumers (display, decoder, export) all parsed the same
formatted text; any format change broke two of them.
**Fix:** store the **structured transaction** on each line (`data-txn`, JSON).
The decoder and export read the structured object; the display text is free to
change. This permanently decouples presentation from data.
**Lesson:** the recurring bug was text-format coupling. Structured data on the
element is the real fix -- presentation and data must not share a parser.

### 15. ESPHome sends commands byte-by-byte -> "undefined" params
**Symptom:** a multi-byte command (`0x21` Set Column Address) decoded as
"Column address undefined..undefined (incomplete)".
**Cause:** ESPHome's ssd1306 `command()` is `write_byte(0x00, cmd)` -- **one I2C
write per command byte**. So `0x21` and its params arrive as separate
transactions; the per-transaction decoder never sees the params together.
**Fix (two parts):**
- *Defensive decode:* missing operands render as `?`, not `undefined`, with an
  honest "params sent separately" note.
- *Stateful merge (Live only):* consecutive command writes to the same address
  are merged into one byte stream, which the decoder groups correctly
  (`Column address 0..127`).
**Lesson:** a per-transaction decoder can't group commands a driver splits across
transactions unless you reassemble the stream first.

### 16. Merge produced plausible-but-wrong output on a flooded bus
**Symptom:** in Live on a refreshing display, the decoder showed
"Column address 33..33" repeated -- **plausible but false**, the worst outcome.
**Cause:** the merge assumes a complete, contiguous sequence. `count > 1` detects
Batch coalescing but **not throttle drops**. On a flooded Live bus, `count == 1`
but most transactions are dropped, so the survivors are sparse and aliased (all
`0x21`); merging them yields garbage.
**Fix:** track when the throttle last dropped a transaction; if drops are recent,
the capture has gaps -- **don't merge**, and show an honest note that decoding is
unreliable (point the user to the mirror). Merging is only trusted for a clean,
un-dropped, un-coalesced (Live) capture.
**Lesson:** a debug tool must never present false-but-plausible output. Detect
unreliable input and say so, rather than decoding it anyway.

### 17. CSV garbled in Excel
**Symptom:** `>>`, `us`, `x` glyphs showed as mojibake when opening the CSV.
**Cause:** the file is UTF-8; Excel assumes the local codepage without a BOM.
**Fix:** prepend a UTF-8 BOM (`U+FEFF`) to the CSV.
**Lesson:** UTF-8 files for Excel need a BOM.

### 18. `Clear` left stale state
**Symptom:** after clearing post-trigger, the trigger button stayed "fired" and
the first new line showed a bogus delta.
**Cause:** `Clear` reset the buffer but not `trgHit` or the delta baseline
`lastTxUs`.
**Fix:** `Clear` resets `trgHit` (and re-arms if a condition is set) and
`lastTxUs`.
**Lesson:** a reset must clear *all* derived state, not just the obvious buffer.

---

## Robustness checklist (verified)

- Malformed WebSocket message -> `JSON.parse` is guarded (logged, no crash).
- Corrupt stored txn -> `JSON.parse(data-txn)` is guarded.
- Event JSON is built from hex + safe strings -> no injection.
- Buffers are bounded; the DOM log is trimmed to a fixed maximum.
- CSV fields are quoted/escaped per RFC 4180.

---

## Testing

The frontend can't be compiled in the dev harness, so validation is layered:

1. Structural: ASCII check, brace balance, HTMLDOC delimiter count, `node
   --check` on the extracted `<script>`, and an "no id removed" invariant when
   refactoring the UI.
2. Host tests in Node for the pure logic (delta wrap, CSV escaping, filter/error
   matching, trigger matching, the merge decision, defensive decode, export
   field extraction).
3. Final: `esphome compile` + flash on real hardware, then click through LOG,
   Decoder, Trigger, Export, and Serial.

Always finish with step 3 before publishing a release.
