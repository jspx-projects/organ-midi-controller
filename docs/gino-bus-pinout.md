# GINO bus — 20-pin pinout

The main board and the keyboard (daughter) boards are connected by the 20-pin
IDC bus used by the Dutch **GINO-MIDI** system. This page records the pinout from
the official GINO Big-MIDI manual and how JSPX uses each signal.

---

## Connector

20-pin IDC header, PCB connector viewed from the top:

```
 __________________________________
|                                  |
| 2  4  6  8 10 12 14 16 18 20     |
| 1  3  5  7  9 11 13 15 17 19     |
|               notch              |
 ---------------|____|-------------
```

Pin 1 is marked with a small triangle on the connector.

---

## Official GINO pinout

Source: *Handleiding hoofdprint Big-MIDI 3.1*, section **GINO-BUS** (page 8).
The same table appears in `reference/gino-midi/BIG-MIDI-mainboard-manual.pdf`.

| Pin | Signal | Meaning |
|---:|---|---|
| 1 | GND | ground |
| 2 | A0 | column address bit 0 |
| 3 | D7 | row/data bit 7 |
| 4 | A1 | column address bit 1 |
| 5 | D6 | row/data bit 6 |
| 6 | A2 | column address bit 2 |
| 7 | D5 | row/data bit 5 |
| 8 | POORT 7 | board select 7 (active LOW) |
| 9 | D4 | row/data bit 4 |
| 10 | POORT 6 | board select 6 (active LOW) |
| 11 | D3 | row/data bit 3 |
| 12 | POORT 5 | board select 5 (active LOW) |
| 13 | D2 | row/data bit 2 |
| 14 | POORT 4 | board select 4 (active LOW) |
| 15 | D1 | row/data bit 1 |
| 16 | POORT 3 | board select 3 (active LOW) |
| 17 | D0 | row/data bit 0 |
| 18 | POORT 2 | board select 2 (active LOW) |
| 19 | +5 V | supply for the daughterboards |
| 20 | POORT 1 | board select 1 (active LOW) |

`A0...A2` are the three column-select bits. The 74HC138 on each daughterboard
decodes them into one active-LOW column.

`POORT 1...POORT 7` are the seven board-select lines. The main board makes one
of them LOW to activate that daughterboard.

`D0...D7` are the eight row inputs. The rows have pull-ups (on the main board or
in the MCU); a pressed key pulls its row LOW through the selected column.

---

## JSPX naming

The JSPX firmware uses a different naming convention for the same wires:

| GINO | JSPX | Notes |
|---|---|---|
| A0, A1, A2 | c0, c1, c2 | column select, active LOW after decoding |
| POORT 1...7 | b0...b6 | board select, active LOW; b0 = LSB |
| D7...D0 | r0...r7 | rows; bit 0 is the first key of the column |
| +5 V | +5 V | daughterboard supply |
| GND | GND | ground |

So, for the JSPX cable:

| Pin | JSPX | GINO |
|---:|---|---|
| 1 | GND | GND |
| 2 | c0 | A0 |
| 3 | r0 | D7 |
| 4 | c1 | A1 |
| 5 | r1 | D6 |
| 6 | c2 | A2 |
| 7 | r2 | D5 |
| 8 | b6 | POORT 7 |
| 9 | r3 | D4 |
| 10 | b5 | POORT 6 |
| 11 | r4 | D3 |
| 12 | b4 | POORT 5 |
| 13 | r5 | D2 |
| 14 | b3 | POORT 4 |
| 15 | r6 | D1 |
| 16 | b2 | POORT 3 |
| 17 | r7 | D0 |
| 18 | b1 | POORT 2 |
| 19 | +5 V | +5 V |
| 20 | b0 | POORT 1 |

> **Row order:** `r0` is `D7` on the GINO connector and is the first (lowest)
> note of each column. When making a cable, verify with a single key that the
> left-most key produces the expected note; if it does not, reverse the row
> wiring or adjust `MIDI_LOWEST_NOTE`.

---

## Electrical compatibility

* The GINO bus is a **5 V** bus: pin 19 is +5 V.
* The **Teensy++ 2.0** is a 5 V board and is directly compatible.
* The **SAMD21** is a 3.3 V board and its I/O is **not 5 V tolerant**. Power the
  matrix from 3.3 V, or use level shifters on the rows and column/board-select
  lines. Do not connect a 5 V GINO board directly to a SAMD21.
* The 74HC138 is a CMOS decoder. Its outputs are intended for logic loads, not
  for driving a capacitive keyboard cable. See
  [`key-scanning-and-settling.md`](key-scanning-and-settling.md).
