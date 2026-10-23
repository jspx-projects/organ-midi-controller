# Test sketches

Small Arduino sketches used while bringing up the MIDI libraries and the
hardware. They are diagnostic tools, not production firmware.

| Sketch | Target | Purpose |
|---|---|---|
| `AllEvents_SAMD21.ino` | SAMD21 | Send/exercise all MIDI message types over USB-MIDI |
| `midi_test_basic.ino` | SAMD21 | Minimal MIDI library test on `Serial1` |
| `raw_serial_midi_test.ino` | SAMD21 | Send raw MIDI bytes on `Serial1` (no library) |
| `NoteOnOffEverySec_modified.ino` | SAMD21 | Periodic Note On/Off over USB-MIDI |

The corresponding test logs from 2021 are not included; they were large
captures of raw USB MIDI bytes and serial monitor output.
