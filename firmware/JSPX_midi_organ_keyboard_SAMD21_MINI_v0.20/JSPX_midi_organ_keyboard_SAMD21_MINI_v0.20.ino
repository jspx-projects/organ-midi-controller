/*
   Midi organ keyboard scanner - JSPX
   Versions:
  0.11 20201216 Initial port to SAMD21 M0 mini board from Aliexpress. Compiles and runs.
  0.12 - Optimized reading analog signals by using register functions instead of analog_read. Read all analog inputs in one call of read_an_input().
       - MIDI in (pass-through) from UART to USB (USB to UART commented out for now)
  0.13 changed board select pins to allow for 2 adjoining analog inputs. Deleted board select enable, I do not plan to use it.
    TODO:
   - remove my own LED handling, it it done already by the board. OR use PA22 and PA23 for my own rcv and tx leds, which can be a distance away from the main board.
   - Find out why keyboard 0 transmits on MIDI channel 16
 0.14 Switched to using library MIDIUSB, since USB-MIDI does not work. Tested everything, only thing missing is pass-through from USB to DIN5 out
 0.16 20210102 First complete version. Forward from / to Serial and USB MIDI implemented. RX and TX led controlled separately. 
      Use definitions from midi library. Remove superfluous checks on valid MIDI message from midi_send(). General cleanup.
 0.17 20261023 Fix note map: use NUM_ROWS (8) as the column stride instead of NUM_COLS (7). With NUM_COLS=7 the
      columns overlapped by one note (note 43 twice) and the top six notes (86..91) were never generated.
      Disable TEST_MIDI_SEND by default (it adds serial output to every MIDI message and skews loop-rate measurements).
 0.18 20261023 Scan robustness and speed:
      - Step 1: direct-register board select, single atomic column write, and break-before-make (board 7) around
        every column change. The old CLR-then-SET wrote column 0 for a moment.
      - Step 2: replace the fixed 6 us delay with an adaptive read-until-stable loop. Unchanged columns need
        2 identical samples; changed columns need 3. Timeout after 20 polls.
      - Step 3: two-scan confirmation. A key change is only accepted (and MIDI sent) when it is seen on two
        consecutive scans of the same column. This rejects settling glitches without adding a 40 ms delay.
      - Step 4: the ADC conversion is started before the keyboard scan and read afterwards, so it overlaps the
        scan instead of blocking. One analog input is updated per low-priority cycle.
      - Fix the RX LED initialisation (it cleared the TX flag) and the TEST_COL_SEL pin names.
 0.19 20261023 Production build: JSPX_DEBUG disabled. No 2 s startup delay, no "Loops per mS" report and no
      per-message serial output. Enable JSPX_DEBUG again if you need the loop-rate/diagnostic counters.
 0.20 20261023 Complete MIDI pass-through:
      - Use the MIDI library's message callback instead of rebuilding messages from getData1/getData2.
      - Channel messages are forwarded with the incoming 1-based channel, fixing the old +1 shift.
      - System real-time (Clock/Start/Continue/Stop/Tick/ActiveSensing/SystemReset) sent as 1-byte messages;
        USB uses a raw USB-MIDI packet with CIN 0x0F (the USB-MIDI 1.1.2 transport uses the wrong CIN).
      - System Common (MTC Quarter Frame, Song Position, Song Select, Tune Request) forwarded explicitly;
        USB uses raw packets with CIN 0x02/0x03/0x05.
      - SysEx forwarded with sendSysEx(..., true); the library buffer is 128 bytes, longer dumps are split.
      - Active Sensing is now forwarded instead of dropped.
*/
#include <stdbool.h>
#include <USB-MIDI.h>   
USING_NAMESPACE_MIDI                      // so we can access the enum MidiType, see Arduino/libraries/MIDI_library/src/midi_Defs.h

// global configuration
#define VERSION               0.20
//#define JSPX_DEBUG                      // enable for loop-rate report and serial diagnostics
                                          // Some testing possibilities
#ifdef JSPX_DEBUG
  #define REPORT_INTERVAL 5000            // interval between reports of main loop speed
  // #define TEST_BOARD_SEL
  // #define TEST_COL_SEL
  // #define TEST_MIDI_SEND
#endif
// Hardware config
#define NUM_BOARDS            4           // Number of daughter boards to scan (0 < NUM_BOARDS < 7). 
#define NUM_ROWS              8           // Do not change without looking at the code.
#define NUM_COLS              7           // Set to 7 for 56 key organ keyboard, and to 8 for keyboards with 57 to 64 keys
#define NUM_AN_INPUTS         2           // Can be changed from 0 to 3. Proto 2 supports max 2.
#define NOTE_STRIDE           NUM_ROWS    // Number of notes between the first key of two adjacent columns. Must be NUM_ROWS (8), not NUM_COLS!
#define LED_ON                LOW
#define LED_OFF               HIGH
#define HW_SERIAL             Serial1     // Hardware Serial on SAMD21 MINI
#define Serial                SerialUSB   // Serial on Arduino zero refers to (for us unconnected) HW serial, we need SerialUSB for the USB

// Timing config
#define AN_SHIFT              3           // (1/2)^AN_SHIFT is the weight of the current analog reading wrt computed average
#define DEBOUNCE_TIME         40          // Debounce each key for this amount of mS. 40mS is used by debounce chips, apparently.
#define LED_BLINK_INTERVAL    50          // Blink LED for this number of mS.
/* Adaptive column settling (replaces the old fixed SETTLE_uS).
   Fast path: read once immediately. If the value equals the accepted state, accept it.
   There is no key event to report, so even a premature "still high" read is harmless; a real
   key press in this column will be seen on the next scan.
   Slow path: the value differs, so it may be a key event or a settling glitch. Wait and require
   SETTLE_SLOW_SAMPLES identical samples before accepting it.
   delay_cycles() is a tight NOP loop; at 48 MHz, 16 iterations is about 1 us. */
#define SETTLE_INITIAL_CYCLES 16          // ~1 us before the first slow-path sample
#define SETTLE_POLL_CYCLES    16          // ~1 us between slow-path samples
#define SETTLE_SLOW_SAMPLES   3           // changed column: 3 identical samples
#define SETTLE_MAX_POLLS      20          // timeout after about 20 us
#define LOW_PRIO_LOOPS        10          // Evaluate low prio tasks once per this many main loops

//Midi config
#define MIDI_LOWEST_NOTE      0x24        // key 24, c2, the left-most note on an organ keyboard (8 foot register).
#define MIDI_DEFAULT_VELOCITY 64          // 64 is between mp and mf, specified by MIDI standard for non-velocity sensitive keyboards.
#define MIDI_CONTROLLER       7           // 7 = "channel volume", used as type of controller in Continuous Control messages (for potmeter values).
#define MIDI_TR_SER           0x1         // Transports over which we send MIDI signals
#define MIDI_TR_USB           0x2
#define MIDI_TR_SER_AND_USB   0x3         // MIDI_TR_SER | MIDI_TR_USB
#define USB_MIDI_CABLE        0           // USB-MIDI cable number (0 for the normal single-cable device)

// Port and bit definitions for fast direct IO access
#define ROW_PORT        REG_PORT_IN0
#define ROW7             8                // the bit in the row register
#define ROW6             9
#define ROW5            14
#define ROW4            15
#define ROW3            20
#define ROW2            21
#define ROW1             6
#define ROW0             7
#define CS2             23
#define CS1             22
#define CS0              3                //LSB

// Pin definitions to use with Arduino functions (not time critical)
#define BOARD_SEL2_PIN  19
#define BOARD_SEL1_PIN  18
#define BOARD_SEL0_PIN  17                //LSB
#define AN2_PIN         16
#define AN1_PIN         15
#define AN0_PIN         14
#define LED_TX_PIN      26                // flash this LED when we send MIDI
#define LED_RX_PIN      25                // TODO: flash this LED when we receive MIDI

// Create MIDI objects to send and receive MIDI signals over serial and USB
MIDI_CREATE_INSTANCE(HardwareSerial, HW_SERIAL, MIDI);
USBMIDI_CREATE_INSTANCE(0, usbMIDI);

// Both interfaces use the same message type (Settings::SysExMaxSize = 128 by default).
typedef decltype(MIDI)::MidiMessage JspxMidiMessage;

// Row pins in ROW0..ROW7 order: D9, D8, D7, D6, D5, D2, D3, D4.
// This array is used only for pinMode(); all eight pins must be configured as inputs with pull-ups.
static const uint8_t row_pin[NUM_ROWS]={9,8,7,6,5,2,3,4};
static const uint32_t CS_MASK =  (1<<CS0) | (1<<CS1) | (1<<CS2);
static const uint32_t ROW_MASK = (1ul<<ROW0) | (1ul<<ROW1) | (1ul<<ROW2) | (1ul<<ROW3) |
                                 (1ul<<ROW4) | (1ul<<ROW5) | (1ul<<ROW6) | (1ul<<ROW7);
static const uint32_t ROW_BIT[NUM_ROWS] = {1ul<<ROW0, 1ul<<ROW1, 1ul<<ROW2, 1ul<<ROW3,
                                           1ul<<ROW4, 1ul<<ROW5, 1ul<<ROW6, 1ul<<ROW7};
static const uint_fast8_t an_pin[3] =  {AN0_PIN, AN1_PIN, AN2_PIN};

#if NUM_BOARDS >= 7
#error "Board code 7 is used as the break-before-make deselect. Use NUM_BOARDS <= 6, or add a real decoder enable."
#endif

// Global variables, prefixed with g_
unsigned long g_time;                     // time in mS (since switch-on), updated once per main loop
bool          g_tx_led_blink_requested;   // true if something requested the TX LED to blink
bool          g_rx_led_blink_requested;   // true if something requested the RX LED to blink

// Keyboard scan state. File scope so the adaptive read can compare against the accepted state.
static uint32_t      col_state[NUM_BOARDS][NUM_COLS];    // bit set = key not pressed
static uint8_t       debouncing[NUM_BOARDS][NUM_COLS];   // bit set = key is being debounced
static unsigned long debounce_start_time[NUM_BOARDS][NUM_ROWS][NUM_COLS];
static uint8_t       candidate_mask[NUM_BOARDS][NUM_COLS];   // bit set = change awaiting confirmation
static uint8_t       candidate_value[NUM_BOARDS][NUM_COLS];  // bit set = candidate state is "not pressed"

// Diagnostics (printed with the loop-rate report)
uint32_t g_candidates_confirmed = 0;
uint32_t g_candidates_rejected  = 0;
uint32_t g_settle_timeouts      = 0;

// Local function declarations
void midi_send( const MidiType message_type, uint8_t data1, uint8_t data2, uint8_t channel, const uint8_t transport);

/* Select dm-board 0...NUM_BOARDS-1 by writing a 3 bit binary code to the board select pins.
   Decoder on main board translates this to the 7 active-LOW board-select lines on the GINO bus.
   Board-select pins: b0=PA04 (D17), b1=PA05 (D18), b2=PB02 (D19).
   Direct register writes are used because this is inside the scan loop. */
static inline void select_board(uint8_t board) {
  uint32_t setA = 0, clrA = 0;
  if (board & 0x01) setA |= (1ul << 4); else clrA |= (1ul << 4);
  if (board & 0x02) setA |= (1ul << 5); else clrA |= (1ul << 5);
  PORT->Group[0].OUTSET.reg = setA;
  PORT->Group[0].OUTCLR.reg = clrA;
  if (board & 0x04) PORT->Group[1].OUTSET.reg = (1ul << 2);
  else              PORT->Group[1].OUTCLR.reg = (1ul << 2);
}

/* To select column 0...7 we send the 3-bit binary code for 'col' to the 3-to-8 decoders on the
   keyboard boards. The old code did OUTCLR then OUTSET, which made the decoder see code 0 for a
   moment. This version changes all three bits in one store. The read-modify-write preserves the
   board-select bits PA04/PA05, which are on the same port. */
static inline void select_column(uint8_t col) {
  static const uint32_t col_sel_code[8] = {0, 1<<CS0, 1<<CS1, (1<<CS0)|(1<<CS1), 1<<CS2, (1<<CS2)|(1<<CS0), (1<<CS2)|(1<<CS1), CS_MASK};
  uint32_t v = REG_PORT_OUT0;
  v = (v & ~CS_MASK) | col_sel_code[col];
  REG_PORT_OUT0 = v;
}

/* Tight ~1 us delay at 48 MHz. The loop body is a few cycles per iteration. */
static inline void delay_cycles(uint32_t cycles) {
  while (cycles--) __NOP();
}

/* Read the row port, with an adaptive settling strategy.
   - Fast path: the first sample equals 'expected' (the accepted state). Accept immediately;
     there is no event to report, and a real key press will be caught on the next scan.
   - Slow path: the first sample differs. Wait ~1 us, then require SETTLE_SLOW_SAMPLES identical
     samples before returning. This filters the column-change/settling glitch.
   The timeout flag lets the caller count columns that never settled. */
static inline uint32_t read_rows_settled(uint32_t expected, bool *timeout) {
  uint32_t first = ROW_PORT & ROW_MASK;
  if (first == expected) {
    *timeout = false;
    return first;
  }

  // Possible key event or settling glitch: require a stable new value.
  delay_cycles(SETTLE_INITIAL_CYCLES);
  uint32_t prev = ROW_PORT & ROW_MASK;
  uint8_t  same = 0;

  for (uint8_t i = 0; i < SETTLE_MAX_POLLS; i++) {
    delay_cycles(SETTLE_POLL_CYCLES);
    uint32_t now = ROW_PORT & ROW_MASK;
    if (now == prev) {
      if (++same >= (uint8_t)(SETTLE_SLOW_SAMPLES - 1)) {
        *timeout = false;
        return now;
      }
    } else {
      same = 0;
      prev = now;
    }
  }
  *timeout = true;
  return prev;
}

/* Initialise the keyboard state once, before the first scan. */
static void scan_init(void) {
  for (uint8_t b = 0; b < NUM_BOARDS; b++) {
    for (uint8_t c = 0; c < NUM_COLS; c++) {
      col_state[b][c]       = ROW_MASK;  // no keys pressed
      debouncing[b][c]      = 0;
      candidate_mask[b][c]  = 0;
      candidate_value[b][c] = 0;
    }
  }
}

/*  Process all rows for column 'col' of km-board 'board'.
    Def.: Each key has a "key state": LOW (key pressed) or HIGH (key not pressed).
    Def.: "debouncing" (per key): after a confirmed key state change we start a debounce interval
          during which we ignore further changes.
    NEW (v0.18): two-scan confirmation. The first time a key differs from the accepted state we
    only remember it as a candidate. MIDI is sent, and the 40 ms debounce starts, only when the
    same new state is seen again on the next scan of this column. This rejects settling glitches
    without adding a 40 ms pre-send delay.
*/
static void inline process_rows(uint8_t board, uint8_t col, uint32_t new_col_state) {
  uint8_t  row_mask8;
  uint32_t row_bit;
  uint8_t  raw_bit, old_bit;
  uint8_t  c_mask = candidate_mask[board][col];
  uint8_t  c_val  = candidate_value[board][col];
  uint32_t diff   = col_state[board][col] ^ new_col_state;

  // Fast exit: nothing changed, nothing debouncing, no pending candidate.
  if (diff == 0 && debouncing[board][col] == 0 && c_mask == 0)
    return;

  for (uint_fast8_t row = 0; row < NUM_ROWS; row++) {
    row_mask8 = 0x01 << row;
    row_bit   = ROW_BIT[row];

    if (debouncing[board][col] & row_mask8) {              // This key is being debounced
      if (g_time - debounce_start_time[board][row][col] < DEBOUNCE_TIME)
        continue;                                          // Debounce interval not expired: ignore this key
      debouncing[board][col] &= ~row_mask8;                // Debounce interval expired
    }

    raw_bit = (new_col_state & row_bit) ? 1 : 0;           // 1 = not pressed
    old_bit = (col_state[board][col] & row_bit) ? 1 : 0;

    if (raw_bit == old_bit) {                              // No state change for this key
      if (c_mask & row_mask8) {                            // A pending change has disappeared
        c_mask &= ~row_mask8;                              // -> reject the candidate
        g_candidates_rejected++;
      }
      continue;
    }

    if (c_mask & row_mask8) {                              // This key already has a candidate
      if (((c_val >> row) & 0x01) == raw_bit) {            // Same new state seen twice: confirm
        midi_send( raw_bit == 0 ? NoteOn : NoteOff,
                   MIDI_LOWEST_NOTE + NOTE_STRIDE * col + row,
                   MIDI_DEFAULT_VELOCITY, board, MIDI_TR_SER_AND_USB );
        col_state[board][col] ^= row_bit;                  // Accept the new state
        debounce_start_time[board][row][col] = g_time;     // Start the 40 ms debounce
        debouncing[board][col] |= row_mask8;
        c_mask &= ~row_mask8;
        g_candidates_confirmed++;
      } else {                                             // Changed again: replace the candidate
        c_val = (c_val & ~row_mask8) | (raw_bit << row);
        g_candidates_rejected++;
      }
    } else {                                               // First sighting: remember, do not send
      c_mask |= row_mask8;
      c_val = (c_val & ~row_mask8) | (raw_bit << row);
    }
  }
  candidate_mask[board][col]  = c_mask;
  candidate_value[board][col] = c_val;
}

// Initialize SAMD21 DAC
// Preconditions: All initialization in wiring.c and startup.c is done.
// Due to our high load resistors of 10k and 22k the DAC clock can not be too high.
// We divide the DAC clock from GCLK_ADC, which is 48Mhz. 
// A division by 16 gives a clock of 3MHz. Also we use extra sample time, 8 clock cycles instead of 1.
// Like this we get stable analog readings even without a 1nF cap on each analog input.
// A call to analogRead now takes 18uS instead of 840uS...
// Our own fastAnalogRead takes 8uS, giving an effective sample rate of 125 kHz. 
static inline void initADC(void)
{
                                                    // Divide Clock by 16 (default is 512: the maximum); Set 10 bits resolution.
    ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV16 | ADC_CTRLB_RESSEL_10BIT;
                                                    // Sampling Time Length 8 (default is 0x3f: the maximum)
    ADC->SAMPCTRL.reg = 0x08;
    while( ADC->STATUS.bit.SYNCBUSY == 1 );         // Wait for synchronization of registers between the clock domains
    ADC->CTRLA.bit.ENABLE = 0x01;                   // Enable ADC
    while( ADC->STATUS.bit.SYNCBUSY == 1 );
}

// Start a conversion and read the result separately. Splitting these lets the main loop start
// a conversion before the keyboard scan and read it afterwards, so the ADC runs in the background.
// Preconditions: ADC configured and switched on.
static inline void startAnalogRead(uint_fast8_t pin)
{
  ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[pin].ulADCChannelNumber;
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->SWTRIG.bit.START = 1;                     // Start ADC
}

static inline uint16_t readAnalogResult(void)
{
  while (ADC->INTFLAG.bit.RESRDY == 0);         // Wait for conversion to complete
  return (uint16_t) (ADC->RESULT.reg);           // Reading RESULT clears RESRDY
}

// Blocking convenience wrapper, used only during initialisation.
static inline uint16_t analogReadFast(uint_fast8_t pin)
{
  startAnalogRead(pin);
  return readAnalogResult();
}

// Analog input filter state (shared by init and the low-priority update).
static int16_t  an_val[NUM_AN_INPUTS];
static uint8_t  prev_midi_val[NUM_AN_INPUTS];
static uint8_t  g_an_input = 0;
static bool     g_an_pending = false;

// First-order IIR filter, sign-correct on both AVR and ARM.
static inline void filter_update(uint8_t idx, int16_t v)
{
  int16_t diff   = v - an_val[idx];
  int16_t update = abs(diff) >> AN_SHIFT;
  if (diff < 0) update = -update;
  an_val[idx] += update;
}

// Initialise the filter with a few iterations so we do not send a flood of CC messages at startup.
static void analog_init(void)
{
  if (NUM_AN_INPUTS < 1) return;

  for (int i = 0; i < NUM_AN_INPUTS; i++)
    an_val[i] = analogReadFast(an_pin[i]);

  for (int it = 0; it < 7; it++)
    for (int i = 0; i < NUM_AN_INPUTS; i++)
      filter_update(i, analogReadFast(an_pin[i]));

  for (int i = 0; i < NUM_AN_INPUTS; i++)
    prev_midi_val[i] = (uint16_t) an_val[i] >> 3;

  g_an_input = 0;
  g_an_pending = false;
}

// Start one conversion before the keyboard scan. The scan takes much longer than the ~8 us
// conversion, so readAnalogResult() in analog_finish() normally returns immediately.
static inline void analog_start(void)
{
  if (NUM_AN_INPUTS < 1) return;
  startAnalogRead(an_pin[g_an_input]);
  g_an_pending = true;
}

// Read the conversion started by analog_start(), filter it and send CC if the MIDI value changed.
static void analog_finish(void)
{
  if (!g_an_pending) return;

  uint16_t v = readAnalogResult();
  g_an_pending = false;

  filter_update(g_an_input, (int16_t) v);

  uint8_t midi_val = (uint16_t) an_val[g_an_input] >> 3;
  if (midi_val != prev_midi_val[g_an_input]) {
    prev_midi_val[g_an_input] = midi_val;
    midi_send(ControlChange, MIDI_CONTROLLER, midi_val, g_an_input, MIDI_TR_SER_AND_USB);
  }

  if (++g_an_input >= NUM_AN_INPUTS) g_an_input = 0;
}

/*  Send MIDI message to the selected transports and flash the TX LED.
    All MIDI must be initialized before calling this function.
    message_type: one of the predefined message types in Arduino/libraries/MIDI_Library/src/midiDefs.h
    data1, data2: first and second databytes (a number between 0 and 127) as follows:
         message_type    |   data1     |  data2
       NoteOn or NoteOff | note        | velocity
       ControlChange     | controller  | controller value
 
   channel:          MIDI channel, between 0 and 15
*/
void midi_send( const MidiType message_type, const uint8_t data1, const uint8_t data2, const uint8_t channel, const uint8_t transport)
{
  // Send MIDI using the desired transport
  if(transport & MIDI_TR_SER)    MIDI.send( message_type, data1, data2, channel+1 );
  if(transport & MIDI_TR_USB) usbMIDI.send( message_type, data1, data2, channel+1 );

  g_tx_led_blink_requested = true;
#ifdef TEST_MIDI_SEND
  Serial.print("midi_send(");Serial.print(transport);Serial.print("): ");
  Serial.print(message_type|channel, HEX); Serial.print(" "); Serial.print(data1, HEX); Serial.print(" "); Serial.println(data2, HEX);
#endif
  return;
}

/* Send one raw USB-MIDI event packet.
   This is used for System Real-Time and System Common messages because USB-MIDI 1.1.2's
   transport puts all system messages in CIN 0x04 regardless of their real CIN. */
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

/* Forward one received message to the other transport.
   - Channel messages: use the library's MidiMessage send, which keeps the incoming 1-based
     channel and handles 2-byte (Program Change / Channel Pressure) and 3-byte messages.
   - System Real-Time: one byte; USB uses a raw packet with CIN 0x0F.
   - System Common: explicit send functions on DIN, raw USB packets with the correct CIN.
   - SysEx: sendSysEx(..., true) because the library buffer includes the F0/F7 boundaries.
   The old code rebuilt every message from getData1/getData2/getChannel and added 1 to the
   channel, which shifted every forwarded channel message up by one. */
static void forward_message(const JspxMidiMessage& msg, uint8_t transport)
{
  if (!msg.valid) return;

  g_rx_led_blink_requested = true;

  if (msg.type <= PitchBend) {                          // Channel voice, 2 or 3 bytes
    // Use the 4-argument send, not send(MidiMessage): the parser does not set
    // MidiMessage::length for channel messages, so send(MidiMessage) would write only the status.
    // msg.channel is already 1-based, so no +1 here.
    if (transport & MIDI_TR_SER) MIDI.send(msg.type, msg.data1, msg.data2, msg.channel);
    if (transport & MIDI_TR_USB) usbMIDI.send(msg.type, msg.data1, msg.data2, msg.channel);
    return;
  }

  if (msg.type >= Clock) {                              // System Real-Time, 1 byte
    if (transport & MIDI_TR_SER) HW_SERIAL.write((uint8_t)msg.type);
    if (transport & MIDI_TR_USB) usb_send_packet(0x0F, (uint8_t)msg.type, 0, 0);
    return;
  }

  switch (msg.type) {
    case SystemExclusive:
      // The library SysEx buffer includes the F0 and F7 boundaries.
      if (transport & MIDI_TR_SER) MIDI.sendSysEx(msg.getSysExSize(), msg.sysexArray, true);
      if (transport & MIDI_TR_USB) usbMIDI.sendSysEx(msg.getSysExSize(), msg.sysexArray, true);
      break;

    case TimeCodeQuarterFrame:
      // data1 is the complete data byte (0nnndddd); data2 is 0 for this 2-byte message.
      if (transport & MIDI_TR_SER) MIDI.sendTimeCodeQuarterFrame(msg.data1);
      if (transport & MIDI_TR_USB) usb_send_packet(0x02, (uint8_t)msg.type, msg.data1, 0);
      break;

    case SongPosition:
      // data1 = LSB, data2 = MSB of the 14-bit position.
      if (transport & MIDI_TR_SER) MIDI.sendSongPosition((unsigned)msg.data1 | ((unsigned)msg.data2 << 7));
      if (transport & MIDI_TR_USB) usb_send_packet(0x03, (uint8_t)msg.type, msg.data1, msg.data2);
      break;

    case SongSelect:
      if (transport & MIDI_TR_SER) MIDI.sendSongSelect(msg.data1);
      if (transport & MIDI_TR_USB) usb_send_packet(0x02, (uint8_t)msg.type, msg.data1, 0);
      break;

    case TuneRequest:
      if (transport & MIDI_TR_SER) MIDI.sendTuneRequest();
      if (transport & MIDI_TR_USB) usb_send_packet(0x05, (uint8_t)msg.type, 0, 0);
      break;

    default:
      break;
  }
}

/* Message callbacks: DIN in -> USB out, USB in -> DIN out. */
static void on_din_message(const JspxMidiMessage& msg)
{
  forward_message(msg, MIDI_TR_USB);
}

static void on_usb_message(const JspxMidiMessage& msg)
{
  forward_message(msg, MIDI_TR_SER);
}

/* Blink TX LED if requested.
   If we detect a blink request when the LED is already on we still set the blink timer
   so it remains on for the required time starting from the last received blink request. */
void handle_tx_led_state()
{
  static unsigned long tx_switch_on_time = 0; // time at which TX LED was switched on.
  static bool       tx_led_on;
  static bool       first_time = true;

  if ( first_time ) {                     // only do the initialization
    pinMode(LED_TX_PIN, OUTPUT);
    digitalWrite(LED_TX_PIN, LED_OFF);
    g_tx_led_blink_requested = false;
    tx_led_on = false;
    first_time = false;
    return;
  }

  if ( g_tx_led_blink_requested ) {          // switch on LED if necessary but always set blink timer
    if ( !tx_led_on ) {
      digitalWrite(LED_TX_PIN, LED_ON);
      tx_led_on = true;
    }
    tx_switch_on_time = g_time;
    g_tx_led_blink_requested = false;
    return;
  }

  // No led blink requested. Check if LED must be switched off

  if ( tx_led_on && (g_time - tx_switch_on_time > LED_BLINK_INTERVAL) ) {
    digitalWrite(LED_TX_PIN, LED_OFF);           // switch off LED
    tx_led_on = false;
    return;
  }

  // On function entry, (LED was off) OR (LED was on AND blink timer did not expire yet)
  return;
}
/* Blink RX LED if requested. */
void handle_rx_led_state()
{
  static unsigned long rx_switch_on_time = 0; // time at which TX LED was switched on.
  static bool       rx_led_on;
  static bool       first_time = true;

  if ( first_time ) {                     // only do the initialization
    pinMode(LED_RX_PIN, OUTPUT);
    digitalWrite(LED_RX_PIN, LED_OFF);
    g_rx_led_blink_requested = false;
    rx_led_on = false;
    first_time = false;
    return;
  }

  if ( g_rx_led_blink_requested ) {       // switch on LED if necessary but always set blink timer
    if ( !rx_led_on ) {
      digitalWrite(LED_RX_PIN, LED_ON);
      rx_led_on = true;
    }
    rx_switch_on_time = g_time;
    g_rx_led_blink_requested = false;
    return;
  }

  // No led blink requested. Check if LED must be switched off

  if ( rx_led_on && (g_time - rx_switch_on_time > LED_BLINK_INTERVAL) ) {
    digitalWrite(LED_RX_PIN, LED_OFF);           // switch off LED
    rx_led_on = false;
    return;
  }

  // On function entry, (LED was off) OR (LED was on AND blink timer did not expire yet)
  return;
}

/* Report amount of main loops / msec
   Call at the END of the main loop for a correct count. */
#ifdef JSPX_DEBUG
void inline report_speed(void)
{
  static unsigned long last_report_time = g_time;
  static uint32_t      count = 0;

  if ( g_time - last_report_time < REPORT_INTERVAL ) {
    count++;
    return;
  }

  // Interval between reports has passed

  Serial.print("Loops per mS = "); Serial.println( (float) count / (float) REPORT_INTERVAL, 2);
  Serial.print("Stats: confirm="); Serial.print(g_candidates_confirmed);
  Serial.print(" reject="); Serial.print(g_candidates_rejected);
  Serial.print(" settle_to="); Serial.println(g_settle_timeouts);
  last_report_time = g_time;
  count = 0;
  return;
}
#endif

void pinStr( uint32_t ulPin, unsigned strength) // works like pinMode(), but to set drive strength
{
  // Handle the case the pin isn't usable as PIO
  if ( g_APinDescription[ulPin].ulPinType == PIO_NOT_A_PIN )
  {
    return ;
  }
  if(strength) strength = 1;      // set drive strength to either 0 or 1 copied
  PORT->Group[g_APinDescription[ulPin].ulPort].PINCFG[g_APinDescription[ulPin].ulPin].bit.DRVSTR = strength ;
}

void setup() {
  handle_tx_led_state();              // Initialize
  handle_rx_led_state();              // Initialize
                                   // Initialize board select pins

  pinMode(BOARD_SEL0_PIN, OUTPUT);
  pinMode(BOARD_SEL1_PIN, OUTPUT);
  pinMode(BOARD_SEL2_PIN, OUTPUT);

                                   // Initialize column select pins
  REG_PORT_DIRSET0 = 1<<CS0 | 1<<CS1 | 1<<CS2; // Direction 1=output

// pinStr(BOARD_SEL0_PIN,1);
//  pinStr(BOARD_SEL1_PIN,1);
//  pinStr(BOARD_SEL0_PIN,1);

//  pinStr(42,1);
//  pinStr(20,1);
//  pinStr(21,1);
  
//  pinMode(16, OUTPUT);
//  pinMode(20, OUTPUT);
//  pinMode(21, OUTPUT);
                                       // Initialize rows inputs with pull-up registers
    for(int i=0; i<NUM_ROWS; i++ )
    pinMode(row_pin[i], INPUT_PULLUP);
    
                                       // Analog pins do not need initialization.

    initADC();
    analogReadFast(an_pin[0]);        // The SAMD21 data sheet says to discard the first reading.
    analog_init();                    // Initialise the analog filter and MIDI values
    scan_init();                      // Initialise keyboard state, candidates and debounce admin
    MIDI.begin(MIDI_CHANNEL_OMNI);    // Initialize MIDI port and library, pass through all channels to DIN5
    usbMIDI.begin(MIDI_CHANNEL_OMNI); // Initialize USB-MIDI, listen to all channels
    MIDI.setHandleMessage(on_din_message);     // DIN in -> USB out
    usbMIDI.setHandleMessage(on_usb_message);  // USB in -> DIN out
#ifdef JSPX_DEBUG
  Serial.begin(115200);
  delay(2000);                       // Give the Serial port a chance to become available
  Serial.print("Version "); Serial.println(VERSION, 4);
  //
  #ifdef TEST_BOARD_SEL
    Serial.print("board_sel_pins (msb first): ");
    Serial.print(BOARD_SEL2_PIN); Serial.print(" ");
    Serial.print(BOARD_SEL1_PIN); Serial.print(" ");
    Serial.print(BOARD_SEL0_PIN); Serial.print(" ");
    Serial.println("");
    for (uint8_t i = 0; i < NUM_BOARDS; i++) {
      select_board(i);
      Serial.print("Selected board "); Serial.println(i);
      delay(10000);
    }
  #endif
  
  #ifdef TEST_COL_SEL
    Serial.print("col_sel_pins (msb first): ");
    Serial.print("PA");Serial.print(CS2); Serial.print(" ");
    Serial.print("PA");Serial.print(CS1); Serial.print(" ");
    Serial.print("PA");Serial.print(CS0); Serial.print(" ");
    Serial.println("");
    for (uint8_t i = 0; i < NUM_COLS; i++) {
      select_column(i);
      Serial.print("Selected column "); Serial.println(i);
      delay(10000);
    }
  #endif
#endif
}

void loop() {
  static uint8_t  board, col;
  static uint8_t  loop_count = 0;
  bool low_prio;

  g_time = millis();                    // set the global time
  low_prio = (++loop_count > LOW_PRIO_LOOPS);

  // Step 4: start the ADC conversion before the scan. The scan takes much longer than the
  // conversion, so analog_finish() at the end of the loop normally does not block.
  if (low_prio) analog_start();

  // Read the keys in all keyboards.
  for (board = 0; board < NUM_BOARDS; board++) {
    for (col = 0; col < NUM_COLS; col++) {
      // Step 1: break-before-make. Board code 7 makes all GINO board-select lines HIGH
      // (Y7 of the main-board 74HC138 is not on the bus), so all daughterboards are off
      // while the column code changes. Then enable the real board.
      select_board(7);
      select_column(col);
      select_board(board);

      // Step 2: adaptive settling.
      bool timeout;
      uint32_t rows = read_rows_settled(col_state[board][col], &timeout);
      if (timeout) g_settle_timeouts++;

      // Step 3: two-scan confirmation.
      process_rows(board, col, rows);
    }
  }

  // Read incoming MIDI messages. The message callbacks forward them to the other transport:
  // DIN in -> USB out and USB in -> DIN out (v0.20). No manual getData1/getData2 rebuilding.
  MIDI.read();
  usbMIDI.read();

  if (low_prio) {                       // Handle low priority tasks
    analog_finish();
    handle_tx_led_state();
    handle_rx_led_state();
    loop_count = 0;
  }

#ifdef JSPX_DEBUG
  report_speed();
#endif
}
