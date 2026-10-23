# Documentation index

| Document | Contents |
|---|---|
| [`key-scanning-and-settling.md`](key-scanning-and-settling.md) | Current scan algorithm, contact resistance, capacitance, settling, tuning and hardware options |
| [`scan-performance.md`](scan-performance.md) | Measured loop rate of the current SAMD21 scan |
| [`samd21-build-notes.md`](samd21-build-notes.md) | SAMD21 build steps, configuration, pin map, remaining issues |
| [`midi-protocol.md`](midi-protocol.md) | MIDI messages, channels, note map and examples |
| [`midi-passthrough.md`](midi-passthrough.md) | DIN/USB pass-through design, message dispatch, USB-MIDI encoding and verification |
| [`gino-bus-pinout.md`](gino-bus-pinout.md) | 20-pin GINO bus pinout and JSPX signal names |
| [`linux-debug-notes.md`](linux-debug-notes.md) | USB/serial/MIDI debugging on Linux and the MIDI test sketches |
| [`licensing.md`](licensing.md) | License scope, copyleft summary, third-party exceptions |
| [`switch-matrix-explained.pdf`](switch-matrix-explained.pdf) | Illustrated explanation of matrix scanning and diode requirements |
| [`reference/`](reference/) | GINO manuals and datasheets |

## Reference material

### GINO-MIDI

* `reference/gino-midi/BIG-MIDI-mainboard-manual.pdf` — main board, GINO bus pinout
* `reference/gino-midi/BIG-MIDI-user-manual.pdf` — channel/note/register configuration
* `reference/gino-midi/decoder-matrix-board-manual.pdf` — keyboard/decoder board
* `reference/gino-midi/LCD-switchboard-manual.pdf` — LCD/switch board
* `reference/gino-midi/hallsensors2-*.pdf` — hall-sensor input board (older GINO accessory)

### Datasheets / application notes

* `reference/datasheets/74HC138-datasheet.pdf`
* `reference/datasheets/AN4373-SAR-ADC-cookbook.pdf` — SAR ADC input impedance and
  sampling time
