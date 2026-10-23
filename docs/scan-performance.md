# Scan performance

Measured scan rate of the SAMD21 firmware with the current adaptive settling.

## Conditions

| Item | Value |
|---|---|
| Board | Arduino/Genuino Zero (SAMD21G18A, 48 MHz, 3.3 V) |
| Configuration | `NUM_BOARDS=4`, `NUM_ROWS=8`, `NUM_COLS=7`, `NUM_AN_INPUTS=2` |
| Optimisation | `-O2` |
| Build | `JSPX_DEBUG` enabled for the loop-rate report |
| Load | no keyboard, no pedals, no GINO boards connected |
| Power | USB |

Because nothing is connected, the row inputs are pulled HIGH and every column
takes the **fast path**. This is the best-case scan rate.

## Results

| Settle strategy | Loops/ms | µs/loop | µs/column |
|---|---:|---:|---:|
| old fixed 6 µs | 2.79 | 358.4 | 12.80 |
| **current adaptive fast path** | **7.57** | **132.1** | **4.72** |
| current slow path forced on every column | 3.19 | 313.5 | 11.20 |

- One loop at 7.57 loops/ms is **132 µs**.
- Worst-case key-to-MIDI latency is about **2 loops ≈ 0.27 ms** (key change plus
  two-scan confirmation).
- The slow-path row is the absolute worst case where every column on every scan
  changes. It is not a realistic playing pattern; it is the lower bound.
- The fast path is also about 6 % faster than the old no-delay build, because the
  board-select and column writes use direct register access.

## What the numbers mean

The scan loop is:

```
for each board:
    for each column:
        select board 7        (break-before-make)
        write column code      (atomic)
        select real board
        read rows              (fast or slow path)
        process rows           (candidate/confirm/debounce)
```

On the real organ, columns whose state is unchanged take the fast path. A
column with a key press/release takes the slow path (~6.5 µs extra), and the
column after a held key may also take the slow path once because the shared row
bus still reflects the previous column. Sparse playing therefore stays close to
7 loops/ms; only dense activity approaches the slow figure.

The loop-rate report also prints:

```
Stats: confirm=<n> reject=<n> settle_to=<n>
```

- `confirm`: accepted key events;
- `reject`: candidates that disappeared before confirmation (glitches);
- `settle_to`: columns that hit the slow-path timeout.

If `settle_to` rises on the real organ, increase `SETTLE_MAX_POLLS` or
`SETTLE_SLOW_SAMPLES`, or improve the hardware. See
[`key-scanning-and-settling.md`](key-scanning-and-settling.md).

## Reproducing

Enable `JSPX_DEBUG` in the current v0.20 `.ino`, compile and upload, then read
the USB serial port:

```bash
arduino-cli compile --fqbn arduino:samd:arduino_zero_native \
  "firmware/JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20"

arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:samd:arduino_zero_native \
  "firmware/JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20"

stty -F /dev/ttyACM0 115200 raw -echo
timeout 15 cat /dev/ttyACM0
```

Raw captures:

- `docs/measurements/2026-10-23-samd21-loop-rate/fastpath.txt`
- `docs/measurements/2026-10-23-samd21-loop-rate/slowpath.txt`
- `docs/measurements/2026-10-23-samd21-loop-rate/fixed-delay-sweep.csv`
