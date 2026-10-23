# JSPX MIDI Organ Keyboard Controller

A DIY MIDI controller that implements MIDI for any electrical keyboard or
pedalboard. We used it to adapt the keyboards from a '70s Johannes organ so we
can use the high-quality keyboards to play a church-organ simulation on our
laptop. We equipped the old keyboards with reed switches (normally OPEN, with a
small magnet on each switch).

---

## Status

| Platform | Firmware | Status | Notes |
|---|---|---|---|
| Teensy++ 2.0 (AT90USB1286, 16 MHz, 5 V) | **v1.1** | **Production, stable** | 4 keyboards, 8 columns each, 2 analog inputs, USB + DIN MIDI |
| SAMD21 M0 Mini / Arduino Zero (48 MHz, 3.3 V) | **v0.20** | **Production, stable** | 4 keyboards, 7 columns each, 2 analog inputs, USB + DIN MIDI + full pass-through |

Both platforms are production-ready. Approximate comparison:

| | Teensy++ 2.0 | SAMD21 M0 Mini / Arduino Zero |
|---|---|---|
| CPU | 16 MHz AVR | 48 MHz Cortex-M0+ |
| Scan rate | ~2.2 full scans/ms (4 boards, 8 columns, 6.25 µs settle) | 7.57 loops/ms fast path (4 boards, 7 columns); 3.19 loops/ms if every column needs the slow path |
| Power | ~0.3 W measured | **< 0.1 W measured** |
| Cost | ~$30 (PJRC) | **~$6** (SAMD21 M0 Mini clone, AliExpress) |

Prices vary by vendor and region. The scan-rate figures are explained in
[docs/scan-performance.md](docs/scan-performance.md).

---

## How it works

Each keyboard is wired as an **8 row x 8 column switch matrix** with a diode in
series with every key (the GINO diode matrix) so that multiple simultaneous keys
can be read without ghosting. The keys themselves are the original organ
actions; in this build each key closes a reed switch.

```
                 +--------------------------------------+
                 |             main board               |
   USB / DIN <---+  MCU (Teensy++ 2.0 or SAMD21)        |
                 |                                      |
                 |  3 board-select bits -> 74HC138      |
                 |  3 column bits ------> GINO bus      |
                 |  8 row inputs <------- GINO bus      |
                 +------------------+-------------------+
                                    |
                            20-pin GINO bus
                                    |
        +---------------------------+---------------------------+
        |                                                       |
  +-----+------+                                          +-----+------+
  | keyboard 1 |                   ...                    | keyboard 4 |
  | 74HC138    |                                          | 74HC138    |
  | 8x8 matrix |                                          | 8x8 matrix |
  +------------+                                          +------------+
```

For every keyboard the firmware selects one column (pulls it LOW through a
74HC138), waits for the column to settle, and reads all eight rows at once.
A row that is LOW means that the key at (row, column) is pressed. Note numbers
increase with the column and, within a column, with the row (row 0 is the
left-most/first key of that group):

```
note = MIDI_LOWEST_NOTE + (NUM_ROWS * column) + row
```

`MIDI_LOWEST_NOTE` is `0x24` = 36 = C2 (with C4 = 60), the left-most key of an
8-foot organ register. The column stride is eight rows (`NOTE_STRIDE = NUM_ROWS`).
See [docs/midi-protocol.md](docs/midi-protocol.md#note-map).

Up to four analog inputs (expression pedals, crescendo pedal) are converted to
MIDI CC #7 (channel volume). Key velocity is not measured; a fixed velocity is
sent.

---

## Electrical notes that matter for old organ keyboards

### 1. Contact resistance

The original 1970s organ electronics drove the keyboards with (probably) 24 V
signals. That high voltage easily breaks through the thin oxide/sulphide film
that forms on silvered contacts.

In this controller the contact only carries the row pull-up current. The rows
use the MCU's internal pull-ups (roughly 20...50 kOhm), so the current is only

```
I ~= 5 V   / 20...50 kOhm ~= 0.10...0.25 mA   (Teensy, 5 V)
I ~= 3.3 V / 20...50 kOhm ~= 0.07...0.17 mA   (SAMD21, 3.3 V)
```

(The 22 kOhm resistors in the design are on the *analog* inputs, not on the
rows.)

That is far below the "wetting current" needed to clean a tarnished mechanical
contact. For that reason the actual build uses **reed switches with a small
magnet** on every key. Reed contacts are sealed, gold/rhodium plated and switch
reliably at very low currents, so contact resistance is no longer a problem.

If you want to use the original silver contacts, see
[docs/key-scanning-and-settling.md](docs/key-scanning-and-settling.md) for
options (higher wetting current, pulse charging, etc.).

### 2. Capacitance and settling time

The keyboards contain a lot of metal and long interconnecting wires. Each
column line therefore has a significant capacitance. When the 74HC138 switches
to a new column, the line has to charge/discharge through the driver and the
wiring before the row inputs are valid.

The two firmware variants handle this differently:

| Platform | Strategy | Settling |
|---|---|---|
| Teensy++ 2.0 | fixed delay | `SETTLE_CYCLES 100` = 100 / 16 MHz = **6.25 us** |
| SAMD21 | adaptive fast/slow path | **~0 us** when the column state is unchanged; ~6.5 us when it changes |

The Teensy version uses the simplest solution: a fixed delay. It works, but it
is a fixed cost on every column of every scan.
[docs/key-scanning-and-settling.md](docs/key-scanning-and-settling.md)
describes the adaptive strategy, the two-scan confirmation and the hardware
options (line drivers, lower capacitance).

The SAMD21 implementation reaches **7.57 loops/ms** on the bench (132 us/loop),
up from 2.79 loops/ms with the old fixed 6 us delay. See
[docs/scan-performance.md](docs/scan-performance.md).

### 3. Voltage compatibility

* The **Teensy++ 2.0** is a 5 V board. It can drive original GINO/matrix boards
  directly.
* The **SAMD21** is a **3.3 V** board and its pins are **not 5 V tolerant**.
  The whole matrix (74HC138 supply and row pull-ups) must run at 3.3 V, or the
  row signals must be level-shifted. Do not connect a 5 V keyboard board
  directly to the SAMD21.

### 4. MIDI output

DIN-5 MIDI is a 5 mA current loop at 31250 baud:

* 5 V transmitter: two 220 Ohm resistors, about 5.5...5.9 mA.
* 3.3 V transmitter: two 44 Ohm resistors, about 6.1...7.1 mA (1/2 W types,
  mounted 1 mm above the PCB).
* A reverse-protection diode and a decoupling capacitor are fitted at the MIDI
  input optocoupler.

Measured power consumption: about **0.3 W** for the Teensy prototype and
**< 0.1 W** for the SAMD21 build.

---

## Repository layout

```
.
├── README.md                     <- this file
├── docs/
│   ├── key-scanning-and-settling.md  <- current scan algorithm and electrical principles
│   ├── scan-performance.md           <- measured loop rate
│   ├── samd21-build-notes.md         <- build, configuration, pin map, open items
│   ├── midi-protocol.md              <- MIDI messages, channels, note map
│   ├── midi-passthrough.md           <- pass-through design and verification
│   ├── gino-bus-pinout.md            <- 20-pin GINO bus pinout
│   ├── linux-debug-notes.md
│   ├── licensing.md                  <- license scope and copyleft summary
│   ├── measurements/                 <- raw measurement captures
│   ├── switch-matrix-explained.pdf
│   └── reference/                    <- GINO manuals, datasheets, app notes
├── firmware/
│   ├── JSPX_midi_organ_keyboard_Teensy_v1.1/          <- production Teensy
│   │   ├── JSPX_midi_organ_keyboard_Teensy_v1.1.ino
│   │   └── README.md
│   ├── JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20/    <- production SAMD21
│   │   ├── JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20.ino
│   │   └── README.md
│   └── test-sketches/            <- small MIDI/library tests
├── hardware/
│   └── schematics/               <- SVG layouts
```

> The retained schematics are under `hardware/schematics/`. Photos of the
> prototypes and test setup will be added later.

---

## Building the firmware

### Teensy++ 2.0 (production)

1. Install [Teensyduino](https://www.pjrc.com/teensy/teensyduino.html).
2. Open `firmware/JSPX_midi_organ_keyboard_Teensy_v1.1/JSPX_midi_organ_keyboard_Teensy_v1.1.ino`.
3. Board: **Teensy++ 2.0**.
4. USB Type: **MIDI** (or **Serial + MIDI** if you enable `JSPX_DEBUG`).
5. Compile and upload.

Configuration lives at the top of the `.ino` file:

```c
#define NUM_BOARDS     4   // number of keyboard boards
#define NUM_COLS       8   // 8 keys per column
#define NUM_AN_INPUTS  2   // number of expression pedals
//#define JSPX_DEBUG       // enable serial diagnostics
```

### SAMD21 M0 Mini

See [docs/samd21-build-notes.md](docs/samd21-build-notes.md). In short:

1. Install the Arduino SAMD core and select **Arduino M0 (Native USB)**.
2. Install **MIDI Library** 5.0.2 (47effects) and **USB-MIDI** 1.1.2 (which
   installs MIDIUSB 1.0.5). Later/upstream versions also work; see
   [docs/midi-passthrough.md](docs/midi-passthrough.md).
3. Enable the RX/TX LED pins in `variant.h` (see the build notes).
4. Set the release optimisation to `-O2` in `platform.txt`.
5. Open `firmware/JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20/JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20.ino`.

---

## Current status and limitations

**SAMD21 v0.20**

* The keyboard scan uses break-before-make, an atomic column write, adaptive
  fast/slow settling, two-scan confirmation and an overlapped ADC. On the bench
  it runs at **7.57 loops/ms** fast path; 3.19 loops/ms if every column takes the
  slow path. See [docs/scan-performance.md](docs/scan-performance.md).
* MIDI pass-through preserves the incoming channel and forwards channel
  messages, real-time, System Common, SysEx and Active Sensing. The USB side is
  loopback-verified; see [docs/midi-passthrough.md](docs/midi-passthrough.md).
* The DIN pass-through was not captured on the bench (no MIDI receiver/adapter).
  Test DIN -> USB and USB -> DIN on the organ with a MIDI monitor.
* SysEx is limited to the library buffer (128 bytes default); longer messages
  are split.
* The whole matrix must run at 3.3 V; see the voltage warning above.
* `analogReadFast()` leaves the digital input buffer enabled; call
  `pinPeripheral()` once per analog pin for best analog stability.
* Verify that 74HC138 output Y7 is unconnected before relying on board code 7
  as the break-before-make deselect.
* `col_state` is 32-bit but only 8 bits are used; `pinStr()` is unused.

**General**

* No enclosure / panel drawings yet.
* The main-board and keyboard-board PCB designs are only available as the
  original GINO-compatible schematics and prototype drawings in `hardware/`.
* Photos are intentionally not in the repository; new ones will be added later.

---

## References

* GINO-MIDI (gino-midi.nl) — the original Dutch organ-MIDI system whose 20-pin
  bus and keyboard boards this project reuses. Manuals are in
  `docs/reference/gino-midi/`.
* MIDI 1.0 Detailed Specification (MMA).
* Atmel/Microchip AT90USB1286 and SAMD21 datasheets.
* TI/Philips 74HC138 datasheet.
* "Cookbook for SAR ADC Measurements" (AN4373) — ADC input impedance and
  sampling-time background.

## License

This project uses **strong-reciprocal (copyleft) licenses**. Everyone can use,
modify, manufacture and sell it, but if you distribute a modified firmware or
hardware design, you must make your modifications available under the same
license and keep the notices.

| Part | License | File |
|---|---|---|
| Firmware and software (`firmware/`) | **GPL-3.0-or-later** | [`LICENSE`](LICENSE) |
| Hardware design files (`hardware/schematics/`) | **CERN-OHL-S-2.0** | [`LICENSE-HARDWARE`](LICENSE-HARDWARE) |
| Documentation and measurements (`docs/`, this README) | **CC-BY-SA-4.0** | [`LICENSE-DOCS`](LICENSE-DOCS) |

Photos, when added, will be licensed under CC-BY-SA-4.0 as well. Third-party
reference material in `docs/reference/` (GINO manuals, datasheets) is **not**
covered by these licenses and remains under its original copyright.
See [`docs/licensing.md`](docs/licensing.md) for the plain-language summary and
the full texts. This is not legal advice.

## Credits

Hardware, firmware and documentation: John (JSPX).
GINO bus and original organ-MIDI concept: GINO Electronics, Zeist (NL).
