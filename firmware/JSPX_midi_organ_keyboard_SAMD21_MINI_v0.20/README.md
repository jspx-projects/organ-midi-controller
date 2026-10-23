# SAMD21 firmware

**Current version:** `JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20.ino`
**Board:** Arduino/Genuino Zero or SAMD21 M0 Mini (48 MHz, 3.3 V)
**Status:** production, stable. Scan and MIDI pass-through implemented.
**Cost:** about **$6** for a SAMD21 M0 Mini clone on AliExpress.
**Power:** **< 0.1 W** measured.

Only the production v0.20 is included.

---

## Read this first

* [Key scanning and settling](../../docs/key-scanning-and-settling.md)
* [Scan performance](../../docs/scan-performance.md)
* [Build notes and remaining issues](../../docs/samd21-build-notes.md)
* [MIDI protocol](../../docs/midi-protocol.md)
* [MIDI pass-through](../../docs/midi-passthrough.md)

## Quick start

1. Arduino IDE with the **Arduino SAMD Boards** core.
2. Board: **Arduino Zero (Native USB Port)** or **Arduino M0 (Native USB)**.
3. Libraries: **MIDI Library** 5.0.2 and **USB-MIDI** 1.1.2 (installs MIDIUSB
   1.0.5).
4. Enable `PIN_LED_RXL` / `PIN_LED_TXL` in `variant.h` if the board needs it
   (see build notes).
5. Set `compiler.optimization_flags.release=-O2` in `platform.txt`.
6. Compile and upload.

## Configuration

```c
#define NUM_BOARDS     4   // keyboard boards
#define NUM_ROWS       8   // fixed: 8 rows per column
#define NUM_COLS       7   // 7 = 56 keys; use 8 for 61/64-key keyboards
#define NOTE_STRIDE    NUM_ROWS   // note spacing between columns (must be 8)
#define NUM_AN_INPUTS  2   // expression pedals
//#define JSPX_DEBUG

// Adaptive column settling
#define SETTLE_INITIAL_CYCLES 16  // ~1 us before the first slow-path sample
#define SETTLE_POLL_CYCLES    16  // ~1 us between slow-path samples
#define SETTLE_SLOW_SAMPLES   3   // identical samples before accepting a change
#define SETTLE_MAX_POLLS      20  // timeout (~20 us)
#define LOW_PRIO_LOOPS        10  // analog update every 11 loops
```

## Library versions

Tested with:

* MIDI Library 5.0.2 (47effects)
* USB-MIDI 1.1.2 (lathoub)
* MIDIUSB 1.0.5

The upstream master branches fix most of the library issues, but Tick (0xF9)
over USB is still broken. The raw-CIN path in v0.20 remains necessary for Tick
and works with both the released and master libraries. See
[`docs/midi-passthrough.md`](../../docs/midi-passthrough.md).

## Scan path

The SAMD21 scan uses:

* direct register writes for the board-select and column pins;
* a single atomic column write;
* **break-before-make** via board code 7;
* an adaptive settle:
  * **fast path:** one immediate row read when the value equals the accepted
    state;
  * **slow path:** ~1 µs wait + 3 identical samples, 20-poll timeout, when it
    differs;
* **two-scan confirmation** before sending MIDI, followed by the 40 ms debounce;
* an **overlapped ADC** conversion for the expression pedals.

The detailed rationale and tuning are in
[`docs/key-scanning-and-settling.md`](../../docs/key-scanning-and-settling.md).

## MIDI pass-through

The DIN and USB ports are bridged in both directions using the MIDI library's
message callbacks:

* channel messages keep their incoming 1-based channel;
* System Real-Time (Clock, Start, Continue, Stop, Tick, Active Sensing, System
  Reset) is forwarded as one byte; USB uses CIN `0x0F`;
* System Common (MTC Quarter Frame, Song Position, Song Select, Tune Request)
  is forwarded explicitly; USB uses CIN `0x02/0x03/0x05`;
* SysEx is forwarded with `sendSysEx(..., true)`; the library buffer is 128
  bytes, longer messages are split;
* Active Sensing is forwarded.

The USB side is loopback-verified; the DIN output path uses the library's
standard senders plus direct `Serial1.write()` for real-time and still needs an
organ test. See
[`docs/midi-passthrough.md`](../../docs/midi-passthrough.md).

## Measured performance

Board: Arduino/Genuino Zero, `-O2`, 4 boards × 7 columns, no keyboard connected.

| Settle strategy | Loops/ms | µs/loop | µs/column |
|---|---:|---:|---:|
| fixed 6 µs | 2.79 | 358.4 | 12.80 |
| **adaptive fast path** | **7.57** | **132.1** | **4.72** |
| slow path forced on every column | 3.19 | 313.5 | 11.20 |

The fast path is the normal no-change case. On the real organ, columns with key
changes take the slow path; the rate sits between 3.19 (every column slow) and
7.57. Worst-case key-to-MIDI latency is about 2 loops ≈ 0.27 ms.

The performance figures were measured with the loop-rate report enabled; v0.20
has the same scan loop. See [`docs/scan-performance.md`](../../docs/scan-performance.md).

## Remaining work

* Test the DIN pass-through on the organ with a MIDI monitor.
* SysEx is limited to the library buffer (128 bytes default); longer dumps are
  split.
* The whole matrix must run at 3.3 V (SAMD21 is not 5 V tolerant).
* `analogReadFast()` leaves the digital input buffer enabled; call
  `pinPeripheral()` once per analog pin for best analog stability.
* `TEST_COL_SEL` pin names are fixed, but the debug mode is not regularly
  tested.
* `pinStr()` is unused dead code.
* Verify on your main board that 74HC138 output Y7 is unconnected before
  relying on board code 7 as the break-before-make deselect.
