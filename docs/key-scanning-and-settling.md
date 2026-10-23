# Key scanning, contact resistance and column settling

This document describes the current scan design and the electrical principles
behind it. It applies to both firmware variants:

| Platform | Scan implementation |
|---|---|
| Teensy++ 2.0 v1.1 | direct port I/O, fixed 100-cycle settle delay (6.25 µs), 40 ms debounce |
| SAMD21 v0.20 | direct port I/O, break-before-make, adaptive settle, two-scan confirmation, 40 ms debounce |

The principles are the same; the SAMD21 has more software support because it
runs a 3.3 V matrix and has more CPU headroom.

---

## 1. How a column is scanned

Each keyboard is an 8 x 8 matrix:

- eight **rows** are inputs with pull-ups;
- eight **columns** are driven by a 74HC138 3-to-8 decoder;
- one diode is in series with every key (the GINO diode matrix), so multiple
  keys can be pressed without ghosting.

For each column:

1. select the keyboard board (74HC138 on the main board);
2. select the column (74HC138 on the keyboard board, active LOW);
3. wait for the column line to settle;
4. read all eight rows at once;
5. compare with the previous state, debounce and send MIDI.

The row bits are read in one port access because all eight row pins are on the
same port (Teensy port C, SAMD21 port A with a mask).

### Teensy v1.1

The column code is changed and then the firmware waits 100 CPU cycles at
16 MHz = **6.25 µs** before reading the rows. The board-select decoder is
deactivated while the column changes (break-before-make), then re-enabled.

### SAMD21 v0.20

The SAMD21 uses an **adaptive** sequence:

1. **Break-before-make:** select board code 7 (all GINO board-select lines
   HIGH, all daughterboards off), change the column with a single atomic port
   write, then select the real board.
2. **Fast path:** read the row port immediately. If the value equals the
   accepted state, accept it and move on. There is no key event to report, so
   even a premature "still high" reading is harmless; a real key press is
   caught on the next scan.
3. **Slow path:** if the value differs, wait about 1 µs, then require
   **3 identical samples** at about 1 µs intervals. Abort after 20 polls
   (~20 µs) and count a timeout.
4. **Two-scan confirmation:** a changed key is only a candidate on the first
   scan. MIDI is sent, and the 40 ms debounce starts, only when the same new
   state is seen on the next scan of that column.
5. **Overlapped ADC:** the expression-pedal conversion is started before the
   keyboard scan and read afterwards, so the ~8 µs conversion runs in the
   background.

The SAMD21 constants are at the top of the `.ino`:

```c
#define SETTLE_INITIAL_CYCLES 16   // ~1 us before the first slow-path sample
#define SETTLE_POLL_CYCLES    16   // ~1 us between slow-path samples
#define SETTLE_SLOW_SAMPLES   3    // identical samples before accepting a change
#define SETTLE_MAX_POLLS      20   // timeout (~20 us)
#define LOW_PRIO_LOOPS        10   // analog update every 11 loops
```

Diagnostics (`JSPX_DEBUG` build) print `confirm`, `reject` and `settle_to` with
the loop rate:

- `confirm`: accepted key events;
- `reject`: candidates that disappeared before confirmation (good: glitches);
- `settle_to`: columns that hit the polling timeout (bad: hardware marginal).

---

## 2. Why the column needs time to settle

The column line is capacitive. The metal keyboard frame, the wiring, the diode
matrix and the 74HC138 output all contribute. A simplified model is:

```
             R_driver          R_wire + R_contact      R_row_pullup
 74HC138 o----/\/\/----+------------/\/\/----+---------/\/\/---- +Vcc
                       |                     |
                    C_column              C_row
                       |                     |
                      GND                   GND
                                         (row input)
```

- `R_driver` is the 74HC138 output resistance and its limited sink current.
- `R_wire` and `R_contact` are the cable and key-contact resistance.
- `C_column` and `C_row` are the stray capacitances.

When a new column is selected, the row voltage follows an exponential curve.
It must cross the MCU input threshold before the row is read. The time constant
is dominated by the largest resistance in the path and the largest capacitance:

```
tau = (R_driver + R_wire + R_contact) * (C_column + C_row)
```

A clean reed switch and a short cable settle in well under 1 µs. A long cable,
a large metal frame and a high-resistance silver contact can take several
microseconds. The old fixed 6.25 µs was chosen for the worst column in the
original organ.

The 74HC138 is a logic decoder, not a cable driver. Its outputs are not intended
to drive a large capacitive load.

---

## 3. Contact resistance and wetting current

The original 1970s organ electronics drove the keyboards with a relatively high
voltage (probably 24 V). That provided enough **wetting current** to break
through the thin oxide/sulphide film on silvered contacts.

In this controller the key contact only carries the row pull-up current. The
rows use the MCU's internal pull-ups (roughly 20...50 kOhm):

```
5 V   / 20...50 kOhm = 0.10...0.25 mA
3.3 V / 20...50 kOhm = 0.07...0.17 mA
```

That is a "dry" current. It is fine for a clean gold/rhodium reed contact, but
it is marginal for a tarnished silver contact.

The actual build uses **reed switches with a small magnet** on every key. Reed
contacts are sealed and work reliably at dry-circuit currents, so contact
resistance is no longer a problem.

If you want to use the original silver contacts:

1. **Increase the wetting current** with lower row pull-ups (e.g. 4.7 kOhm).
   Check the 74HC138 sink current: with 8 keys pressed in one column, the
   column must sink 8 x the row current. 22 kOhm gives about 1.8 mA total;
   4.7 kOhm gives about 8.5 mA, which needs a stronger column driver.
2. **Use a pulsed wetting current** (a small capacitor on each row, charged to
   Vcc and discharged through the contact when the column is selected).
3. **Run the matrix at a higher voltage** (12...24 V) and level-shift the rows
   down to the MCU. This reproduces the original organ's conditions but needs
   high-voltage column drivers and protection.
4. **Clean and maintain the contacts** — temporary, not a permanent solution.

Contact resistance and capacitance interact: a high contact resistance slows
the row discharge, which looks like extra capacitance. Improving the contact
can reduce the required settling time.

---

## 4. Measuring and tuning

Do not tune the delay by guesswork. Put a scope on:

1. a 74HC138 column output at the main board;
2. the same column at the far end of the keyboard;
3. a row line at the MCU, with a key pressed in that column.

Trigger on the column-select edge and measure the 10...90 % rise/fall times and
the time until the row crosses the MCU threshold. Repeat with the longest cable
and the key farthest from the decoder.

In a `JSPX_DEBUG` build, watch:

- `settle_to`: if it rises, the slow path is hitting the 20-poll timeout;
- `reject`: candidates that vanished before confirmation;
- `confirm`: accepted events.

Tuning rules:

- If `settle_to` is zero and `reject` is low, the scan is healthy.
- If `settle_to` rises, increase `SETTLE_MAX_POLLS` or `SETTLE_SLOW_SAMPLES`,
  or fix the hardware (cable, driver, contact).
- If the loop rate matters more than worst-case margin, reduce
  `SETTLE_POLL_CYCLES` / `SETTLE_SLOW_SAMPLES`, but verify with a scope and the
  counters first.

---

## 5. Hardware improvements

If the settling time is still too long:

- **Use a line driver.** An octal non-inverting buffer/driver with active-LOW
  enable (e.g. 74HCT244/245) has more drive current and a proper enable. The
  trade-off is losing direct compatibility with GINO daughterboards that have
  no line driver.
- **Reduce cable length and loop area.** Keep column wires close to their
  ground return; use a ribbon with interleaved grounds; avoid long stubs.
- **Keep column wires away from the metal frame.** The frame is the largest
  stray capacitance.
- **Do not add series resistors** at the decoder outputs unless you have a
  ringing problem; they increase settling time.
- **Decouple each daughterboard** with 100 nF close to the 74HC138.
- **Use 74HCT, not 74HC,** if the inputs are driven from 3.3 V. HCT has
  TTL-compatible thresholds (2.0 V), so a 3.3 V column code is recognized
  reliably.

---

## 6. Current measured performance

SAMD21, 4 boards x 7 columns, `-O2`, no keyboard connected, `JSPX_DEBUG` build:

| Settle strategy | Loops/ms | µs/loop | µs/column |
|---|---:|---:|---:|
| old fixed 6 µs | 2.79 | 358.4 | 12.80 |
| current adaptive fast path | 7.57 | 132.1 | 4.72 |
| current slow path forced on every column | 3.19 | 313.5 | 11.20 |

The fast path is the normal case when the column state has not changed. The
slow path is paid only for columns that actually differ, plus occasionally the
column after a held key because the shared row bus may still reflect the
previous state.

Worst-case key-to-MIDI latency is about **2 loops ≈ 0.27 ms** (key change plus
confirmation), far below pipe speech onset.

The figures were measured with the same scan loop as the current v0.20; v0.20
only disables the serial report. See [`scan-performance.md`](scan-performance.md)
for the measurement conditions and raw captures.

---

## 7. Principles in one page

- **Break before make:** never leave the old column active while changing the
  code.
- **One atomic column write:** the decoder must see the old code or the new
  code, never an intermediate code.
- **Fast path when nothing changes:** no delay is needed if the row state
  equals the accepted state.
- **Slow path when it changes:** require several identical samples before
  trusting a new state.
- **Confirm across scans:** one extra scan filters the settling glitch that
  survives the stable-read.
- **Debounce separately:** 40 ms for mechanical bounce, not for analog
  settling.
- **Overlap the ADC:** the expression-pedal conversion runs while the keyboard
  is being scanned.
- **Make it observable:** timeout/reject/confirm counters turn a tuning problem
  into data.
