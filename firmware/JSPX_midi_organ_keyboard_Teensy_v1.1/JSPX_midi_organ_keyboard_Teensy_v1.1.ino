/*
   Midi organ keyboard scanner - JSPX
   Versions:

  1.0:   20250110 Implement first order digital filter for analog smoothing. Uses 9 times less memory and smoothes just as well.
  1.1    20250122 Remove excessive comments from this block, send MIDI on any changed MIDI value (remove hysteresis)

*/
#include <stdbool.h>
//
// Global configuration
#define VERSION 1.1
#define MIDI_USB  // send MIDI over USB
//#define JSPX_DEBUG
#ifdef JSPX_DEBUG
#define REPORT_INTERVAL 5000  // interval between reports of main loop speed
//  #define TEST_BOARD_SEL
//  #define TEST_COL_SEL
#endif

// Hardware config
#define NUM_BOARDS 4     // Number of km-boards to scan (0 < NUM_BOARDS < 8).
#define NUM_ROWS 8       // This must be 8, since 8 rows are stored in one byte. Do not change.
#define NUM_COLS 8       // This must be 8. Changing this (for example to read more than 64 keys per keyboard) needs code changes.
#define NUM_AN_INPUTS 2  // Can be 1...4. Our HW supports max 4. Our HW has 22k pullup resistors on each analog input. \
                         // These ensure unused analog inputs have maximum volume and do not create random values.
#define LED_ON HIGH
#define LED_OFF LOW

// Timing config
#define DEBOUNCE_TIME 40       // Debounce each key for this amount of uS. 40uS is used by debounce chips, apparently.
#define AN_SHIFT 3             // (1/2)^AN_SHIFT is the weight of the current analog reading wrt computed average
#define LED_BLINK_INTERVAL 50  // Blink LED for this number of mS
#define SETTLE_CYCLES 100      // number of cycles we wait after selecting a new column \
                               // 16 cycles is 1uS, since clock speed is 16MHz
#define LOW_PRIO_LOOPS 10      // Evaluate low prio tasks once per this many main loops

// Midi config
#define MIDI_NOTEON 0x90          // MIDI Message type: NoteOn
#define MIDI_NOTEOFF 0x80         // MIDI Message type: NoteOff
#define MIDI_CC 0xB0              // MIDI Message type: Continous Control
#define MIDI_LOWEST_NOTE 0x24     // key 24, c2, the left-most note on an organ keyboard (8 foot register).
#define MIDI_DEFAULT_VELOCITY 60  // Key velocity is not read by us. 60 is not too extreme for a piano. Do not set to 0.
#define MIDI_SERIAL Serial1       // MIDI Hardware port
#define MIDI_SERIAL_SPEED 31250   // As defined by the MIDI standard
#define MIDI_CONTROLLER 7         // 7 = "channel volume", used as type of controller in Continuous Control messages (for potmeter values).

// Definition of ports/pins for Teensy++ 2.0. NOTE: row pins must be the 8 least significant bits of a port
#define BOARD_SEL_PORT B
#define COL_SEL_PORT D
#define ROW_PORT C
#define AN_PORT F
#define LED_PORT D
#define MIDI_PORT D
#define TEST_PIN_PORT B
#define BOARD_SEL_EN 3  // HIGH on this pin disables the 74hc138 on the mainboard, and thus indirectly the 74HC138 on each daughterboard
#define BOARD_SEL0 4    //LSB
#define BOARD_SEL1 5
#define BOARD_SEL2 6
#define COL_SEL0 4  //LSB
#define COL_SEL1 5
#define COL_SEL2 7
// Used to address all column bits at once
#define COL_SEL_MASK ((0x01 << COL_SEL0) | (0x01 << COL_SEL1) | (0x01 << COL_SEL2))
#define AN0 0
#define AN1 1
#define AN2 2
#define AN3 3
#define LED 6
#define MIDI_RX 2  // RXD1, reserved for MIDI library, not used by us
#define MIDI_TX 3  // TXD1, reserved for MIDI library, not used by us
#define TEST 2
//
// Define concatenation macro with two lines to allow nested use
// second def. must not include ##, hence the first def. - See Kerninghan Richie, last part of Appendix A.12.3
#define _CAT(x, y) x##y
#define CAT(x, y) _CAT(x, y)
// determine the value of bit _bit (1 or 0). (_bit=0 selects lsb etc.)
#define BIT(_word, _bit) (((_word) >> (_bit)) & 0x01)

#define PIN(_port, _bit) CAT(CAT(PIN_, _port), _bit)  // e.g. PIN(A,3) -> PIN_A3
#define IN_REG(port) CAT(PIN, port)                   // e.g. IN_REG(A) ->PINA (the relevant pins of port A must be configured as input)
#define OUT_REG(port) CAT(PORT, port)                 // e.g. OUT_REG(F) ->PORTF (the relevant pins of port F must be configured as output)
#define DDR_REG(port) CAT(DDR, port)                  // e.g. DDR_REG(F) ->DDRF (port configuration: 0 bits are inputs, 1 bits are outputs)

// Global variables, prefixed with g_
unsigned long g_time;                // time in mS (since switch-on), updated once per main loop
bool g_led_blink_requested = false;  // true if something requested the LED to blink
#ifdef JSPX_DEBUG
#define TEST_PIN PIN(TEST_PIN_PORT, TEST)
#endif

#define SET(port, _bit) ((port) |= 0x1 << (_bit))
#define RESET(port, _bit) ((port) &= ~(0x1 << (_bit)))
#define READ(port, _bit) ((port)&0x1 << _bit)

/* Select dm-board 0...NUM_BOARDS-1 by writing a 3 bit binary code to the board select pins.
   Decoder on main board translats to 7 bits "floating 0" to send over gino bus to daughter board. */
void inline select_board(uint8_t board) {
  static const int board_sel_pin[3] = { PIN(BOARD_SEL_PORT, BOARD_SEL0), PIN(BOARD_SEL_PORT, BOARD_SEL1), PIN(BOARD_SEL_PORT, BOARD_SEL2) };
  digitalWrite(board_sel_pin[0], BIT(board, 0));  // lsb
  digitalWrite(board_sel_pin[1], BIT(board, 1));
  digitalWrite(board_sel_pin[2], BIT(board, 2));
  return;
}

/* To select column 0...7 we send the 3-bit binary code for 'col' to the 3 to 8 decoders on the km-boards
   from which the 3-to-8 decoder generates a floating 1. It changes if any column pin changes!
   D7 D6 D5 D4 D3 D2 D1 D0 | Byte with which to OR port D to select a column. This leaves bits other than D7, D5, D4 unchanged.
   0  x  0  0  x  x  x  x  | 0x00
   0  x  0  1  x  x  x  x  | 0x10
   0  x  1  0  x  x  x  x  | 0x20
   0  x  1  1  x  x  x  x  | 0x30
   1  x  0  0  x  x  x  x  | 0x80
   1  x  0  1  x  x  x  x  | 0x90
   1  x  1  0  x  x  x  x  | 0xA0
   1  x  1  1  x  x  x  x  | 0xB0 -> This is also COL_SEL_MASK, used to reset only bits D7, D5 and D4

   We cast the register to 8 or 16 bits to let the compiler know that only the least significant register bits chang.
   The compiler will then generate the correct instruction that leaves the rest of the register intact.
*/
void inline select_column(uint8_t col) {
  static const uint8_t col_sel_code[NUM_COLS] = { 0x00, 0x10, 0x20, 0x30, 0x80, 0x90, 0xA0, 0xB0 };

  OUT_REG(COL_SEL_PORT) &= ~COL_SEL_MASK;      // clear previous column select code
  OUT_REG(COL_SEL_PORT) |= col_sel_code[col];  // Set current column select code
  return;
}

/*  Process all rows for column 'col' of km-board 'board'.
    Def.: Each key has a "key state": LOW (key pressed) or HIGH (key not pressed).
    Def.: "debouncing" (per key): after processing a key state change we start a debounce interval during which we ignore key state changes.
    We play MIDI directly after we read a new key state change and then start debouncing the key.
  void inline __attribute__((section (".ccm"))) process_rows(int board, int col) {
*/
static void inline process_rows(uint8_t board, uint8_t col) {
  static uint8_t col_state[NUM_BOARDS][NUM_COLS];                            // if bit n is HIGH then key n of 'col' is not pressed
  static uint8_t debouncing[NUM_BOARDS][NUM_COLS];                           // if bit n is HIGH then key n of 'col' is being debounced
  static unsigned long debounce_start_time[NUM_BOARDS][NUM_ROWS][NUM_COLS];  // timestamps for keys that are being debounced
  static bool first_time = true;

  // TODO use memset( ... sizeof()) here instead of iterating
  if (first_time) {  // Initialize our internal administration
    for (uint8_t i = 0; i < NUM_BOARDS; i++)
      for (uint8_t j = 0; j < NUM_COLS; j++) {
        col_state[i][j] = 0xFF;  // all 8 keys not pressed
        debouncing[i][j] = 0;    // all 8 keys not debouncing
      }
    first_time = false;
  }

  uint8_t new_col_state = IN_REG(ROW_PORT);
  //  int8_t new_col_state = (uint8_t) PINC;
  // bit 0..7 of new_col_state contain the actual state of key 0..7 for column 'col'. 1-bit = key not pressed.

  uint8_t current_key_state, row_mask;
  for (uint8_t row = 0; row < NUM_ROWS; row++) {
    row_mask = (0x01 << row);

    if (debouncing[board][col] & row_mask) {  // This key is being debounced
      if (g_time - debounce_start_time[board][row][col] < DEBOUNCE_TIME)
        continue;                           // Debounce interval not expired: ignore this key
      debouncing[board][col] &= ~row_mask;  // Debounce interval expired, reset debounce admin for this key
    }

    if (col_state[board][col] == new_col_state) {  // No key pressed in 'col'
      if (debouncing[board][col] == 0) return;     // And no key being debounced in 'col': we are completely done
      continue;                                    // >=1 keys being debounced in 'col': iterate over next rows to update debounce admin
    }

    // A key in this column changed state, is it in row 'row'?

    current_key_state = new_col_state & row_mask;  // '1' if key not pressed

    if ((col_state[board][col] & row_mask) == current_key_state)
      continue;  // No state change for this key

    // No debounce in progress for this key AND its key state changed
    midi_send(current_key_state == 0 ? MIDI_NOTEON : MIDI_NOTEOFF, board, MIDI_LOWEST_NOTE + NUM_COLS * col + row, MIDI_DEFAULT_VELOCITY);

    col_state[board][col] ^= row_mask;              // flip key state
    debounce_start_time[board][row][col] = g_time;  // Start debounce interval
    debouncing[board][col] |= row_mask;             // this key is now debouncing
  }
  return;
}

// Call this function about once per 10 miliseconds in the main loop to get the MIDI value (0...127) for an analog input.
// "num_input" must be between 0 and NUM_AN_INPUTS
// *midi_val will always be assigned the computed MIDI value, or 0 if NUM_ANALOG_INPUTS=0
// Returns: true if *midi_val is different from last time, false otherwise
bool read_analog_input(uint8_t num_input, uint8_t *midi_val) {
  static uint16_t filtered_values[NUM_AN_INPUTS] = { 0 };  // raw values per input (used for averaging).
  static uint8_t prev_midi_values[NUM_AN_INPUTS] = { 0 };
  static uint16_t new_reading = 0;  //The new analog reading read from analog input num_input
  static bool first_time = true;
  static const int an_pin[4] = { PIN(AN_PORT, AN0), PIN(AN_PORT, AN1), PIN(AN_PORT, AN2), PIN(AN_PORT, AN3) };
  static int adc_diff = 0;  // difference between new and old analog reading, can be positive or negative
  static int16_t update = 0;

  if (NUM_AN_INPUTS < 1) {  // For safety: prevent analog reads if no analog ports are defined
    *midi_val = 0;
    return (false);
  }

  if (first_time) {
    // set DAC prescale division factor to 16 to set DAC clock to 16Mhz/16=1MHz. A conversion takes 13 ADC clock cycles.
    // So 13uS for one conversion. Resolution will be less than 10bits, but we only need 7 bits.
    SET(ADCSRA, ADPS2);    // ADPS2 = 1, Set prescaler division factor to 16
    RESET(ADCSRA, ADPS1);  // ADPS1 = 0, ADC clock = 16MHz/16 = 1MHz
    SET(ADCSRA, ADPS0);    // ADPS0 = 0, Conversion takes 13 ADC clock cycles

    // All analog values will be 10bits, so between 0 and 1023
    // Initialize filters with current readings
    for (int i = 0; i < NUM_AN_INPUTS; i++)
      filtered_values[i] = analogRead(an_pin[i]);
    // Initialize midi values for all inputs
    for (int i = 0; i < NUM_AN_INPUTS; i++)
      prev_midi_values[i] = uint16_t(filtered_values[i]) >> 3;  // Shift creates 7 bit MIDI value, compiler will cast result to uint8_t

    first_time = false;
  }  // if( first_time )

  // Store one new filtered analog reading
  new_reading = analogRead(an_pin[num_input]);

  adc_diff = new_reading - filtered_values[num_input];
  update = abs(adc_diff) >> AN_SHIFT;
  if (adc_diff < 0) update = -update;
  filtered_values[num_input] += update;

  /* 
  //debug code
  Serial.print(new_reading, DEC);
  Serial.print(" ");
  Serial.print(update,DEC);
  Serial.print("    ");
    Serial.println(filtered_values[num_input], DEC); 
  delay(500);    
 */

  // Convert to 7-bit MIDI value (0-127), add 4 to get a rounded (not truncated) value
  *midi_val = (uint16_t)(filtered_values[num_input] + 4) >> 3;

  // Possible also to add hysteresis: only report changes greater than 1
  if (abs(*midi_val - prev_midi_values[num_input]) > 0) {
    prev_midi_values[num_input] = *midi_val;
    return (true);
  }
  *midi_val = prev_midi_values[num_input];
  return (false);
}

/*  Create a valid MIDI message, send it to port MIDI_SERIAL and to USB MIDI, flash the LED.
    Serial port and USB port must be initialized before calling this function.
    message_type: one of the predefined MIDI_* message types
    channel:      MIDI channel, between 0 and 15
    data1, data2: first and second databytes (a number between 0 and 127)
             message_type      | data1       | data2
   MIDI_NOTEON or MIDI_NOTEOFF | note        | velocity
   MIDI_CC                     | controller  | controller value
   See https://tttapa.github.io/Arduino/MIDI/Chap03-MIDI-over-Serial.html
*/
void midi_send(const uint8_t message_type, uint8_t channel, uint8_t data1, uint8_t data2) {
  uint8_t status_byte;  // First nibble: message type, second nibble: channel

  if (channel > 15) channel = 15;  // ensure that channel is in the range 0...15
  status_byte = message_type | channel;
  status_byte |= 0x80;  // Extra insurance: set msb of status byte
  data1 &= 0x7F;        // and clear msb of data bytes
  data2 &= 0x7F;

  // The three MIDI message bytes are complete, and comply with the MIDI specification

  MIDI_SERIAL.write(status_byte);
  MIDI_SERIAL.write(data1);
  MIDI_SERIAL.write(data2);

#ifdef MIDI_USB
  switch (message_type) {
    case MIDI_NOTEON:
      usbMIDI.sendNoteOn(data1, data2, channel);
      break;
    case MIDI_NOTEOFF:
      usbMIDI.sendNoteOff(data1, data2, channel);
      break;
    case MIDI_CC:
      usbMIDI.sendControlChange(data1, data2, channel);
      break;
  }
#endif

  g_led_blink_requested = true;
#ifdef JSPX_DEBUG
  Serial.print("MIDI ");
  Serial.print(status_byte, HEX);
  Serial.print(" ");
  Serial.print(data1, HEX);
  Serial.print(" ");
  Serial.println(data2, HEX);
#endif
  return;
}

/* Blink the LED if requested.
   If we detect a blink request when the LED is already on we still set the blink timer
   so it remains on for the required time starting from the last received blink request. */
void handle_led_state() {
  static unsigned long switch_on_time = 0;  // time at which the LED was switched on.
  static bool led_on;
  static bool first_time = true;
  static const int led_pin = PIN(LED_PORT, LED);

  if (first_time) {  // only do the initialization
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, LED_OFF);
    g_led_blink_requested = false;
    led_on = false;
    first_time = false;
    return;
  }

  if (g_led_blink_requested) {  // switch on LED if necessary but always set blink timer
    if (!led_on) {
      digitalWrite(led_pin, LED_ON);
      led_on = true;
    }
    switch_on_time = g_time;
    g_led_blink_requested = false;
    return;
  }

  // No led blink requested. Check if LED must be switched off

  if (led_on && (g_time - switch_on_time > LED_BLINK_INTERVAL)) {
    digitalWrite(led_pin, LED_OFF);  // switch off LED
    led_on = false;
    return;
  }

  // On function entry, (LED was off) OR (LED was on AND blink timer did not expire yet)
  return;
}

/* Report amount of main loops / msec, also toggle test_pin each main loop.
   Call at the END of the main loop for a correct count. */
#ifdef JSPX_DEBUG
void inline report_speed(void) {
  static unsigned long last_report_time = g_time;
  static uint32_t count = 0;
  static int test_pin_state = 0;

  test_pin_state = !test_pin_state;
  digitalWrite(TEST_PIN, test_pin_state);

  if (g_time - last_report_time < REPORT_INTERVAL) {
    count++;
    return;
  }

  // Interval between reports has passed

  Serial.print("Loops per mS = ");
  Serial.println((float)count / (float)REPORT_INTERVAL, 2);
  last_report_time = g_time;
  count = 0;
  return;
}
#endif

void setup() {
  /*  Set MIDI baud rate (only used for tranmission over serial connector, not for USB) */
  Serial.begin(9600);                    // Initialize UART USB object. Teensy ignores this number and always uses maximum USB speed
  MIDI_SERIAL.begin(MIDI_SERIAL_SPEED);  // Initialize physical MIDI in / out port */

  // Configure digital inputs and outputs, do not initialize analog inputs.
  // Board selection (and enable) outputs
  DDR_REG(BOARD_SEL_PORT) |= (0x1 << BOARD_SEL_EN | 0x1 << BOARD_SEL0 | 0x1 << BOARD_SEL1 | 0x1 << BOARD_SEL2);
  DDR_REG(COL_SEL_PORT) |= COL_SEL_MASK;  // Column selection outputs

  DDR_REG(ROW_PORT) &= 0x00;  // Row inputs (setting a bit to 0 makes the corresponding pin an input
  OUT_REG(ROW_PORT) |= 0xFF;  // Enable pullup resistors on the 8 row bits by writing an 1 to each one


  handle_led_state();  // Does its own initialization

#ifdef JSPX_DEBUG
  delay(2000);  // Give the Serial port a chance to become available
  //  while(!Serial.available());
  pinMode(TEST_PIN, OUTPUT);
  Serial.print("Version ");
  Serial.println(VERSION, 4);
  Serial.print("You can measure half the frequency of the main loop at pin ");
  Serial.println(TEST_PIN);
  //
#ifdef TEST_BOARD_SEL
  Serial.print("board_sel_pins (msb first): ");
  Serial.print(PIN(BOARD_SEL_PORT, BOARD_SEL2));
  Serial.print(" ");
  Serial.print(PIN(BOARD_SEL_PORT, BOARD_SEL1));
  Serial.print(" ");
  Serial.print(PIN(BOARD_SEL_PORT, BOARD_SEL0));
  Serial.print(" ");
  Serial.println("");
  for (uint8_t i = 0; i < NUM_BOARDS; i++) {
    select_board(i);
    Serial.print("Selected board ");
    Serial.println(i);
    delay(10000);
  }
#endif

#ifdef TEST_COL_SEL
  Serial.print("col_sel_pins (msb first): ");
  Serial.print(PIN(COL_SEL_PORT, COL_SEL2));
  Serial.print(" ");
  Serial.print(PIN(COL_SEL_PORT, COL_SEL1));
  Serial.print(" ");
  Serial.print(PIN(COL_SEL_PORT, COL_SEL0));
  Serial.print(" ");
  Serial.println("");
  for (uint8_t i = 0; i < NUM_COLS; i++) {
    select_column(i);
    Serial.print("Selected column ");
    Serial.println(i);
    delay(10000);
  }
#endif

#endif
}

void loop() {
  static uint8_t board, col;
  static uint8_t loop_count = 0;
  static uint8_t num_an_input = 0;  // The analog input to be read next
  static uint8_t midi_val;          // MIDI value of analog input (0..127)

  g_time = millis();  // set the global time (only done here!)
  loop_count++;
  // Read the keyboards

  for (board = 0; board < NUM_BOARDS; board++) {
    select_board(board);
    for (col = 0; col < NUM_COLS; col++) {
      OUT_REG(BOARD_SEL_PORT) |= (0x1 << BOARD_SEL_EN);  // De-activate all daughterboards
      select_column(col);
      OUT_REG(BOARD_SEL_PORT) &= ~(0x1 << BOARD_SEL_EN);  // Activate all daughterboards
      __builtin_avr_delay_cycles(SETTLE_CYCLES);          // Allow column pattern to stabilize before reading
                                                          // 50 cycles still creates a problem for key 32, it plays also one column above.
                                                          // 100 cycles is OK.
      process_rows(board, col);
    }
  }

  if (loop_count > LOW_PRIO_LOOPS)  // Handle low prio tasks
  {
    if (read_analog_input(num_an_input, &midi_val)) {
      midi_send(MIDI_CC, num_an_input, MIDI_CONTROLLER, midi_val);
      g_led_blink_requested = true;
    }
    num_an_input++;
    if (num_an_input >= NUM_AN_INPUTS) num_an_input = 0;
    handle_led_state();
    loop_count = 0;
  }

#ifdef JSPX_DEBUG
  report_speed();
#endif
}
