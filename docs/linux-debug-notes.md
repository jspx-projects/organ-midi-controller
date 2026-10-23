# Debugging MIDI and USB on Linux

Notes on USB, serial and MIDI debugging on Linux.

---

## USB and serial ports

```bash
dmesg -w                         # watch USB events while plugging in
amidi -l                         # list ALSA MIDI ports
```

### Raw USB serial output (debug messages)

```bash
cat /dev/ttyACM0                 # show debug output from the USB serial port
```

### FTDI / UART adapter

```bash
stty -F /dev/ttyUSB0 115200      # set the serial speed of the USB-UART adapter
cat /dev/ttyUSB0 | hexdump -Cv   # show the raw MIDI bytes in hex + ASCII
```

`hexdump` is needed because MIDI is binary; a terminal would show unprintable
characters.

### ALSA MIDI monitor

```bash
amidi -l                         # list ports, e.g. hw:3,0,0
amidi -p hw:3,0,0 --dump         # dump received MIDI bytes in hex
```

### Useful one-liners

```bash
# 3-byte MIDI messages as hex, one per line (requires midisnoop/amidi or a small script)
amidi -p hw:3,0,0 --dump | xxd

# check that the Teensy/SAMD21 enumerates as a MIDI device
aconnect -l
```

---

## MIDI test sketches

`firmware/test-sketches/` contains the small sketches used to bring up the MIDI
libraries:

| Sketch | Purpose |
|---|---|
| `raw_serial_midi_test.ino` | send raw MIDI bytes on `Serial1` (SAMD21) |
| `midi_test_basic.ino` | minimal MIDI library test on `Serial1` |
| `AllEvents_SAMD21.ino` | exercise all MIDI message types over USB |
| `NoteOnOffEverySec_modified.ino` | periodic note on/off over USB |

These are diagnostic tools, not part of the production firmware.
