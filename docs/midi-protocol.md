# MIDI protocol used by the JSPX controller

The controller sends standard MIDI 1.0 messages over two transports at the same
time:

* **USB-MIDI** (class-compliant, no driver needed on Linux/macOS/Windows).
* **DIN-5 MIDI out** at **31250 baud**, 8 data bits, no parity, 1 stop bit,
  5 mA current loop.

The SAMD21 version can additionally act as a **MIDI merge / pass-through**
between DIN-5 and USB.

---

## Messages

All messages use the standard 3-byte format. `ch` is the MIDI channel in the
status byte, 0...15 (displayed as channel 1...16).

### Note On

```
0x90 | ch   note   velocity
```

Sent when a key is pressed. Velocity is fixed (the keyboards are not velocity
sensitive):

| Firmware | Velocity |
|---|---|
| Teensy v1.1 | 60 |
| SAMD21 | 64 |

### Note Off

```
0x80 | ch   note   velocity
```

Sent when a key is released. The release velocity is the same fixed value.

> The firmware sends an explicit Note Off. It does not use Note On with
> velocity 0 (which is also valid MIDI).

### Control Change

```
0xB0 | ch   7   value
```

Controller **7 = Channel Volume**. Used for the analog pedal inputs. `value` is
0...127.

---

## Channel assignment

Each keyboard board gets its own MIDI channel:

| Board index | MIDI channel (status nibble) | MIDI channel (human) |
|---:|---:|---:|
| 0 | 0 | 1 |
| 1 | 1 | 2 |
| 2 | 2 | 3 |
| 3 | 3 | 4 |

Analog inputs use the same channel numbering:

| Analog input | MIDI channel (human) |
|---:|---:|
| 0 | 1 |
| 1 | 2 |
| 2 | 3 |
| 3 | 4 |

This is intentional: analog input 0 is normally the swell/expression pedal of
the division played on keyboard 0.

> Pass-through preserves the incoming MIDI channel unchanged. See
> [`midi-passthrough.md`](midi-passthrough.md).

---

## Note map

The intended note number is:

```
note = MIDI_LOWEST_NOTE + (NUM_ROWS * column) + row
```

with:

```c
#define MIDI_LOWEST_NOTE 0x24   // 36 decimal = C2 when C4 = 60
```

`column` and `row` are zero-based. Row 0 is the first key of a column, i.e. the
left-most key of that group of eight. Every column contains `NUM_ROWS` = 8
keys, so the column stride is **8** (`NOTE_STRIDE = NUM_ROWS`), not `NUM_COLS`.

### Teensy v1.1 (`NUM_COLS = 8`, `NUM_ROWS = 8`)

| Column | Notes |
|---:|---|
| 0 | 36...43 |
| 1 | 44...51 |
| 2 | 52...59 |
| 3 | 60...67 |
| 4 | 68...75 |
| 5 | 76...83 |
| 6 | 84...91 |
| 7 | 92...99 |

A 61-key manual uses notes 36...96; the last three positions of column 7 are
unused. A 64-key manual uses 36...99.

### SAMD21 v0.20 (`NUM_COLS = 7`, `NUM_ROWS = 8`)

| Column | Notes |
|---:|---|
| 0 | 36...43 |
| 1 | 44...51 |
| 2 | 52...59 |
| 3 | 60...67 |
| 4 | 68...75 |
| 5 | 76...83 |
| 6 | 84...91 |

This is the 56-key map. For a 61/64-key keyboard set `NUM_COLS` to 8; the note
stride stays 8.

> The original source comment says "key 24, c2", but `0x24` is 36 decimal.
> Note 36 is C2 in the convention where middle C (C4) is 60. If your organ uses
> a different octave convention, change `MIDI_LOWEST_NOTE`.

### Transposing

To shift the whole manual by an octave, change `MIDI_LOWEST_NOTE` by 12. To
assign a different channel per keyboard, change the `board` value passed to
`midi_send()` (Teensy) or the `channel` argument (SAMD21).

---

## Pass-through / merge (SAMD21)

The SAMD21 firmware forwards:

* DIN-5 MIDI in -> USB-MIDI
* USB-MIDI in -> DIN-5 MIDI out

It listens in **OMNI** mode. `Active Sensing` (0xFE) messages are discarded.
The TX/RX LEDs flash on traffic.

The pass-through is a simple merge: locally generated keyboard/pedal messages
and forwarded messages are interleaved on the same output. There is no advanced
merge buffering, so very dense simultaneous traffic can reorder messages
slightly. For organ use this is not audible.

---

## Timing

At 31250 baud one MIDI byte takes 320 us. A 3-byte message takes 960 us. The
DIN-5 output is therefore the bottleneck for dense chords/glissandi, not the
keyboard scan. USB-MIDI is much faster.

The keyboard scan itself (Teensy, 4 boards, with the 6.25 us settle delay) runs
at about 2.2 full scans per millisecond, so a key change is detected within
about 0.5 ms.

---

## Example byte sequences

Keyboard 0, key C2 (note 36, channel 1), press and release:

```
press:    90 24 3C
release:  80 24 3C
```

Keyboard 1, note 48 (C3, channel 2):

```
press:    91 30 3C
release:  81 30 3C
```

Analog input 0 (channel 1), volume 100:

```
B0 07 64
```

(`3C` = 60, `30` = 48, `64` = 100.)
