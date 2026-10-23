# Teensy++ 2.0 firmware

**Production version:** `JSPX_midi_organ_keyboard_Teensy_v1.1.ino`
**Board:** Teensy++ 2.0 (AT90USB1286, 16 MHz, 5 V)
**Status:** production, stable.
**Cost:** about **$30** from PJRC.

---

## Build

1. Install Teensyduino.
2. Open `JSPX_midi_organ_keyboard_Teensy_v1.1.ino`.
3. Board: **Teensy++ 2.0**.
4. USB Type: **MIDI** (or **Serial + MIDI** if `JSPX_DEBUG` is enabled).
5. Compile and upload.

## Configuration

```c
#define NUM_BOARDS     4   // number of keyboard boards
#define NUM_COLS       8   // 8 keys per column (64-key keyboard)
#define NUM_AN_INPUTS  2   // expression pedals
//#define JSPX_DEBUG       // serial diagnostics
```

`NUM_ROWS` is fixed at 8. `SETTLE_CYCLES` is 100 (6.25 us at 16 MHz).

## Pin map

| Function | Port/pin | Notes |
|---|---|---|
| Board select b0/b1/b2 | PB4 / PB5 / PB6 | to 74HC138 on main board |
| Board select enable | PB3 | HIGH disables all daughterboards |
| Column select c0/c1/c2 | PD4 / PD5 / PD7 | to 74HC138 on each keyboard board |
| Rows r0...r7 | PC0...PC7 | inputs with pull-ups, read in one port |
| Analog inputs | PF0...PF3 | 2 used in the current configuration |
| MIDI TX / RX | PD3 / PD2 | `Serial1`, 31250 baud |
| LED | PD6 | flashes on MIDI send |
| Test pin | PB2 | debug only (`JSPX_DEBUG`) |

The row pins must be the eight least-significant bits of one port, because the
firmware reads all eight rows in a single register read.

## Performance

* 4 keyboards, 8 columns each, 6.25 us settle: about **2.2 full scans/ms**.
* Key change detected within about **0.5 ms**.
* Without the settle delay: about 4.2 scans/ms, but phantom keys appear.
* Power: about **0.3 W**.

## Known issues

None known for the production build. See the top-level README for general
project TODOs.
