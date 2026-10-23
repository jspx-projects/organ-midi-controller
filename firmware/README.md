# Firmware

Each implementation is kept in one folder named after its `.ino` file, together
with its README, so it can be opened directly in the Arduino IDE / Teensyduino.

```
firmware/
├── JSPX_midi_organ_keyboard_Teensy_v1.1/          <- production Teensy
│   ├── JSPX_midi_organ_keyboard_Teensy_v1.1.ino
│   └── README.md
├── JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20/    <- production SAMD21
│   ├── JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20.ino
│   └── README.md
└── test-sketches/                                 <- small MIDI/library tests
```

> The repository contains only the current production implementations: Teensy
> v1.1 and SAMD21 v0.20.

## Which one should I use?

* **Teensy++ 2.0, 5 V matrix:** use
  `JSPX_midi_organ_keyboard_Teensy_v1.1`. Production, stable.
* **SAMD21 M0 Mini / Arduino Zero, 3.3 V matrix:** use
  `JSPX_midi_organ_keyboard_SAMD21_MINI_v0.20` (production, full MIDI
  pass-through). The scan path is measured at **7.57 loops/ms** on the bench.
  See [`docs/samd21-build-notes.md`](../docs/samd21-build-notes.md) and
  [`docs/scan-performance.md`](../docs/scan-performance.md).
* **New build:** the Teensy version is the safest choice. The SAMD21 v0.20 is
  functionally complete on the bench; the DIN pass-through still needs a test
  on the real organ.

## Configuration

All user settings are at the top of the `.ino`:

```c
#define NUM_BOARDS     4      // number of keyboard boards (0 < NUM_BOARDS < 8)
#define NUM_COLS       8      // 8 for Teensy; 7 or 8 for SAMD21
#define NUM_AN_INPUTS  2      // number of analog pedal inputs
#define MIDI_LOWEST_NOTE 0x24 // 36 = C2
//#define JSPX_DEBUG           // serial diagnostics
```

`NUM_ROWS` must stay 8: the firmware stores one column of eight keys in one
byte.
