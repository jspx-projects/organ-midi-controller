# MIDI pass-through

The SAMD21 controller bridges its two MIDI transports:

```
DIN-5 MIDI IN  ──►  USB-MIDI OUT
USB-MIDI IN    ──►  DIN-5 MIDI OUT
```

Locally generated keyboard and pedal messages are merged into the same outputs.
This document describes the current pass-through design and the principles
behind it.

---

## 1. Channels

MIDI channel voice messages encode the channel in the low nibble of the status
byte:

```
0x90 = Note On, channel 1
0x9F = Note On, channel 16
```

The firmware uses two conventions internally:

| Context | Convention |
|---|---|
| Local keyboard/pedal events | 0-based `board` / `g_an_input` (0...15) |
| MIDI library API | 1-based (1...16) |
| Received `MidiMessage.channel` | 1-based (1...16) |

Local events go through `midi_send()`, which adds 1 for the library. Local
keyboard board 0...3 becomes channel 1...4; analog input 0...1 becomes channel
1...2.

Pass-through does **not** go through `midi_send()`. It forwards the received
`MidiMessage` with its already-1-based channel:

```c
MIDI.send(msg.type, msg.data1, msg.data2, msg.channel);
usbMIDI.send(msg.type, msg.data1, msg.data2, msg.channel);
```

No `+1` is applied. A message received on channel 1 leaves on channel 1, and
channel 16 stays 16.

---

## 2. Message types and lengths

MIDI messages are not all three bytes:

| Category | Message | Length |
|---|---|---|
| Channel voice | Note Off, Note On, Poly Pressure, Control Change, Pitch Bend | 3 bytes |
| Channel voice | Program Change, Channel Pressure | 2 bytes |
| System Common | MTC Quarter Frame, Song Select | 2 bytes |
| System Common | Song Position Pointer | 3 bytes |
| System Common | Tune Request | 1 byte |
| System Real-Time | Clock, Start, Continue, Stop, Active Sensing, System Reset, Tick | 1 byte |
| System Exclusive | SysEx | variable (`F0 ... F7`) |

The pass-through dispatches explicitly by type instead of assuming every
message is a 3-byte channel message.

---

## 3. Current implementation

### Message callbacks

The two interfaces receive complete `MidiMessage` objects:

```c
MIDI.setHandleMessage(on_din_message);     // DIN in -> USB out
usbMIDI.setHandleMessage(on_usb_message);  // USB in -> DIN out
```

The callback forwards the message to the other transport with
`forward_message(msg, transport)`.

### Channel messages

Use the 4-argument send with the incoming 1-based channel:

```c
MIDI.send(msg.type, msg.data1, msg.data2, msg.channel);
usbMIDI.send(msg.type, msg.data1, msg.data2, msg.channel);
```

This handles 2-byte Program Change and Channel Pressure correctly and keeps the
channel unchanged.

> Do not use `send(MidiMessage)` for channel messages on the released library:
> the parser does not set `MidiMessage::length` for channel messages, so the
> library writes only the status byte. The 4-argument send is the safe path.

### System Real-Time

One byte, no channel:

```c
HW_SERIAL.write((uint8_t)msg.type);              // DIN
usb_send_packet(0x0F, (uint8_t)msg.type, 0, 0);  // USB, CIN 0x0F
```

This covers Clock, Start, Continue, Stop, Tick, Active Sensing and System
Reset.

### System Common

Explicit senders on DIN, raw USB-MIDI packets with the correct CIN:

| Message | DIN | USB CIN |
|---|---|---|
| MTC Quarter Frame | `sendTimeCodeQuarterFrame(msg.data1)` | `0x02` |
| Song Select | `sendSongSelect(msg.data1)` | `0x02` |
| Song Position | `sendSongPosition(msg.data1 \| (msg.data2 << 7))` | `0x03` |
| Tune Request | `sendTuneRequest()` | `0x05` |

`msg.data1` for MTC is the complete data byte (`0nnndddd`), not two nibbles.

### SysEx

The library's SysEx buffer includes the `F0` and `F7` boundaries, so use:

```c
MIDI.sendSysEx(msg.getSysExSize(), msg.sysexArray, true);
usbMIDI.sendSysEx(msg.getSysExSize(), msg.sysexArray, true);
```

The default buffer is 128 bytes. Longer messages are split by the library and
each chunk is forwarded as a separate SysEx message. For large dumps, raise
`SysExMaxSize` with a custom settings class or stream the data.

### Active Sensing

Active Sensing (`0xFE`) is forwarded like any other real-time message. It is no
longer dropped.

---

## 4. USB-MIDI packet encoding

USB-MIDI uses a 4-byte event packet with a **Code Index Number (CIN)**:

| Message class | CIN |
|---|---|
| Channel voice | `0x8`...`0xE` depending on type |
| System Common, 2 bytes | `0x02` |
| System Common, 3 bytes | `0x03` |
| System Common, 1 byte | `0x05` |
| SysEx | `0x04`...`0x07` |
| System Real-Time, 1 byte | `0x0F` |

The released USB-MIDI library (1.1.2) sets CIN `0x04` for all system messages.
The current code therefore sends real-time and System Common messages as raw
USB-MIDI packets with the correct CIN:

```c
static inline void usb_send_packet(uint8_t cin, uint8_t b1, uint8_t b2, uint8_t b3)
{
  midiEventPacket_t pkt;
  pkt.header = (uint8_t)((USB_MIDI_CABLE << 4) | (cin & 0x0F));
  pkt.byte1  = b1;
  pkt.byte2  = b2;
  pkt.byte3  = b3;
  MidiUSB.sendMIDI(pkt);
  MidiUSB.flush();
}
```

The upstream master branch (USB-MIDI 1.1.3) fixes the general CIN handling, but
**Tick (0xF9)** is still not in `system2cin` and gets CIN `0x00`; the host drops
it. The raw `0x0F` path remains necessary for Tick. Reported upstream as
[lathoub/Arduino-USBMIDI#29](https://github.com/lathoub/Arduino-USBMIDI/issues/29).

---

## 5. Library versions

The project is built and tested with:

| Library | Version |
|---|---|
| MIDI Library (47effects) | 5.0.2 |
| USB-MIDI (lathoub) | 1.1.2 |
| MIDIUSB | 1.0.5 |

The upstream master branches were tested separately. They fix the
`MidiMessage::length` and USB system-CIN issues, but Tick over USB is still
broken. The v0.20 code works with both the released and master libraries; the
repository stays on the released versions for reproducible installs.

See [`measurements/2026-10-23-samd21-upstream-lib-test.txt`](measurements/2026-10-23-samd21-upstream-lib-test.txt).

---

## 6. Verification

A temporary USB loopback build (`USB in -> forward_message -> USB out`) was
tested with `amidi` on `hw:2,0,0`. Every message category round-tripped
byte-for-byte:

```
90 24 7F / 80 24 00      Note On/Off ch1
9F 30 7F / 8F 30 00      Note On/Off ch16
B0 07 64 / BF 07 32      CC ch1/ch16
C0 05 / CF 0A            Program Change ch1/ch16
D0 40 / DF 20            Channel Pressure ch1/ch16
E0 00 40 / EF 7F 7F      Pitch Bend ch1/ch16
F8 FA F9 FC FE           Clock, Start, Tick, Stop, Active Sensing
F1 12                    MTC Quarter Frame
F2 01 02                 Song Position
F3 05                    Song Select
F6                       Tune Request
F0 7E 00 06 01 F7        SysEx identity request
```

The raw capture is in
[`measurements/2026-10-23-samd21-passthrough-loopback.txt`](measurements/2026-10-23-samd21-passthrough-loopback.txt).

The **DIN output path** uses the 47effects library's standard senders plus a
direct `Serial1.write()` for real-time bytes. It was not captured on the bench
(no MIDI receiver/adapter); test DIN -> USB and USB -> DIN on the organ with a
MIDI monitor.

---

## 7. Principles

- **Preserve the channel.** Never apply a local channel offset to a forwarded
  message.
- **Dispatch by type.** Do not assume a fixed message length.
- **Use the library's dedicated sender** for System Common and SysEx.
- **Use the 4-argument send** for channel messages on the released library.
- **Encode USB-MIDI CIN correctly** for real-time and System Common; bypass the
  release library where necessary.
- **Keep the SysEx boundary convention explicit** (`sendSysEx(..., true)`).
- **Forward Active Sensing** for transparent pass-through.
- **Verify with a loopback test**, not only by reading the code.
