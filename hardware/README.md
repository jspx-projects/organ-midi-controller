# Hardware

Prototype hardware, schematics and board layouts.

The production build is based on the **GINO-MIDI** main-board/keyboard-board
concept (see [`../docs/reference/gino-midi/`](../docs/reference/gino-midi/)). The JSPX boards replace the original
ATmega8535 main board with a Teensy++ 2.0 or a SAMD21 M0 Mini, while keeping the
20-pin GINO bus and the 74HC138 column decoders.

---

## Photos

Photos of the prototype and test setup will be added later.

---

## Schematics and board layouts

| File | Description |
|---|---|
| `schematics/samd21-prototype-v1.1.svg` | Final SAMD21 M0 Mini stripboard prototype (2020-12-25) |
| `schematics/samd21-m0-mini-pinout-v1.1.svg` | SAMD21 M0 Mini pinout drawing (2020-12-24) |
| `schematics/proto-pedal-daughterboard-v3.svg` | Pedal/expression daughterboard v3 |

The `.svg` files are text and can be opened in Inkscape, a browser or converted
to PDF/PNG with ImageMagick.

---

## Power

The final system is powered from USB. Measured consumption of the Teensy
prototype is about **0.3 W**.

---

## What is missing / next hardware steps

* There is no KiCad/Eagle PCB project for the main board yet; the prototypes are
  stripboard builds documented by the SVG layouts above.
* The SAMD21 prototype removed the board-select enable that the Teensy board
  has. A next revision should bring it back (or use board code 7 as a software
  deselect; see [`../docs/key-scanning-and-settling.md`](../docs/key-scanning-and-settling.md)).
* A proper column line driver (e.g. 74HCT244 with active-LOW enable) would make
  the scan more robust on large, capacitive keyboards. It would, however, break
  direct compatibility with GINO daughterboards that have no line driver.
* The SAMD21 build must run the matrix at 3.3 V. If you want to reuse 5 V GINO
  boards, add level shifters.
