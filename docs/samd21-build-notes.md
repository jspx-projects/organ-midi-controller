# Building the SAMD21 version

**Firmware:** `firmware/JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20/`
**Board:** Arduino/Genuino Zero or SAMD21 M0 Mini (48 MHz, 3.3 V)
**Status:** production, stable. Scan and MIDI pass-through implemented.

---

## Arduino environment

1. Install the **Arduino SAMD Boards** core.
2. Select **Arduino Zero (Native USB Port)** or **Arduino M0 (Native USB)**,
   depending on the board. Both use the `g_APinDescription` / `REG_PORT_*`
   variant expected by the code.
3. Install:
   * **MIDI Library** 5.0.2 (47effects)
   * **USB-MIDI** 1.1.2 (this installs **MIDIUSB** 1.0.5)
4. Enable the RX/TX LEDs in `variant.h` if your board needs it:

   ```
   ~/.arduino15/packages/arduino/hardware/samd/<version>/variants/arduino_zero/variant.h
   ```

   Uncomment:

   ```c
   #define PIN_LED_RXL          (25u)
   #define PIN_LED_TXL          (26u)
   ```

5. Set the release optimisation in:

   ```
   ~/.arduino15/packages/arduino/hardware/samd/<version>/platform.txt
   ```

   ```ini
   compiler.optimization_flags.release=-O2
   ```

   The default `-Os` works but is slightly slower.

6. Open the sketch and upload.

> The released library versions are what the project is tested with. The
> upstream master branches fix the `MidiMessage::length` and USB system-CIN
> issues, but Tick over USB is still broken; the v0.20 code works with both
> release and master. See [`midi-passthrough.md`](midi-passthrough.md).

---

## Configuration

At the top of the `.ino`:

```c
#define NUM_BOARDS     4      // number of keyboard boards
#define NUM_ROWS       8      // fixed: 8 rows per column
#define NUM_COLS       7      // 7 = 56 keys; use 8 for 61/64-key keyboards
#define NOTE_STRIDE    NUM_ROWS   // note spacing between columns (must be 8)
#define NUM_AN_INPUTS  2      // expression pedals
//#define JSPX_DEBUG           // enable loop-rate/diagnostic output

// Adaptive column settling
#define SETTLE_INITIAL_CYCLES 16  // ~1 us before the first slow-path sample
#define SETTLE_POLL_CYCLES    16  // ~1 us between slow-path samples
#define SETTLE_SLOW_SAMPLES   3   // identical samples before accepting a change
#define SETTLE_MAX_POLLS      20  // timeout (~20 us)
#define LOW_PRIO_LOOPS        10  // analog update every 11 loops
```

`NUM_ROWS` must stay 8: one column of eight keys is stored in one byte/word.

---

## Pin map

The time-critical rows and columns use direct port registers; the rest use
Arduino pin numbers.

### Rows (inputs, pull-ups)

| Firmware bit | Port pin | Arduino pin |
|---|---|---|
| `ROW0` (bit 7) | PA07 | D9 |
| `ROW1` (bit 6) | PA06 | D8 |
| `ROW2` (bit 21) | PA21 | D7 |
| `ROW3` (bit 20) | PA20 | D6 |
| `ROW4` (bit 15) | PA15 | D5 |
| `ROW5` (bit 14) | PA14 | D2 |
| `ROW6` (bit 9) | PA09 | D3 |
| `ROW7` (bit 8) | PA08 | D4 |

`ROW_PORT` is `REG_PORT_IN0` (port A). The eight row bits are not contiguous, so
the code masks them.

The `row_pin[]` array used for `pinMode(..., INPUT_PULLUP)` is in `ROW0...ROW7`
order: `{9,8,7,6,5,2,3,4}`.

### Columns (outputs)

| Signal | Port pin | Arduino pin | Notes |
|---|---|---|---|
| `CS0` (LSB) | PA03 | AREF (D42) | direct port only |
| `CS1` | PA22 | SDA (D20) | direct port only |
| `CS2` | PA23 | SCL (D21) | direct port only |

The three column bits are changed with a single atomic read-modify-write of
`REG_PORT_OUT0`. This preserves the board-select bits and avoids the old
CLR-then-SET glitch.

### Board select (outputs)

| Signal | Arduino pin | Port pin |
|---|---|---|
| `BOARD_SEL0` (LSB) | D17 / A3 | PA04 |
| `BOARD_SEL1` | D18 / A4 | PA05 |
| `BOARD_SEL2` | D19 / A5 | PB02 |

Direct register writes are used. Board code 7 is the break-before-make
"all off" code (Y7 of the main-board 74HC138 is not on the GINO bus).

### Analog inputs

| Signal | Arduino pin | Port pin | ADC channel |
|---|---|---|---|
| `AN0` | A0 / D14 | PA02 | AIN0 |
| `AN1` | A1 / D15 | PB08 | AIN2 |
| `AN2` | A2 / D16 | PB09 | AIN3 |

`initADC()` sets prescaler /16 (3 MHz ADC clock), 10-bit resolution and sample
time 8. The Arduino core selects the default reference (internal VDDANA/2 with
gain 1/2, full scale 3.3 V), so PA03/AREF is free to use as `CS0`.

### LEDs and MIDI

| Signal | Arduino pin | Port pin |
|---|---|---|
| TX LED | D26 | PA27 |
| RX LED | D25 | PB03 |
| MIDI DIN TX/RX | D1 / D0 | PA10 / PA11 (`Serial1`) |

---

## 3.3 V warning

The SAMD21 is a **3.3 V** device; its I/O is **not 5 V tolerant**.

* Power the 74HC138s and row pull-ups from **3.3 V**, or
* level-shift every row and column/board-select signal.

Do not connect a 5 V GINO board directly. The Teensy++ 2.0 build is the
5 V-compatible version.

If you use 74HC (not 74HCT) decoders at 3.3 V, check the datasheet: 74HCT has
TTL-compatible inputs (2.0 V threshold) and is the safer choice.

---

## Scan and pass-through

The scan uses break-before-make, a single atomic column write, adaptive
fast/slow settling, two-scan key confirmation and an overlapped ADC. See
[`key-scanning-and-settling.md`](key-scanning-and-settling.md).

MIDI pass-through uses the library message callbacks, explicit dispatch by
message type, correct channel preservation and raw USB-MIDI packets for
real-time/System Common. See [`midi-passthrough.md`](midi-passthrough.md).

Measured performance: **7.57 loops/ms** fast path on the bench, 3.19 loops/ms
when every column takes the slow path. See
[`scan-performance.md`](scan-performance.md).

---

## Remaining issues

1. **DIN pass-through** was not captured on the bench (no MIDI receiver/adapter).
   Test DIN -> USB and USB -> DIN on the organ with a MIDI monitor.
2. **SysEx buffer** is 128 bytes by default; longer messages are split. Raise
   `SysExMaxSize` or stream for large dumps.
3. **3.3 V only** — see the warning above.
4. `analogReadFast()` leaves the digital input buffer enabled; call
   `pinPeripheral()` once per analog pin for best analog stability.
5. Verify that 74HC138 output **Y7** is unconnected before relying on board code
   7 as the break-before-make deselect.
6. `col_state` is 32-bit but only 8 bits are used; `pinStr()` is unused.

---

## Build and upload

```bash
arduino-cli compile --fqbn arduino:samd:arduino_zero_native \
  "firmware/JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20"

arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:samd:arduino_zero_native \
  "firmware/JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20"
```

For the loop-rate report, temporarily enable `JSPX_DEBUG` in the `.ino`.
