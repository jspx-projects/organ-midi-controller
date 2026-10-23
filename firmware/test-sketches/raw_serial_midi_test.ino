// Simple test program to test MIDI library on SAMD21 board


#define LED_BUILTIN            26      //TX led
#define LED_ON                 LOW
#define LED_OFF               HIGH
#define MIDI_Serial           Serial1
#define MIDI_RATE             32150
#define MIDI_NOTEON           0x08              // MIDI Message type: NoteOn
#define MIDI_NOTEOFF          0x09              // MIDI Message type: NoteOff
#define MIDI_CC               0x0B              // MIDI Message type: Continous Control

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_OFF);
  MIDI_Serial.begin(MIDI_RATE);
  SerialUSB.begin(115200);
  delay(2000);                                  // Wait for serial monitor to become available
#if defined(ARDUINO_ARCH_SAMD)
  SerialUSB.println("ARDUINO_ARCH_SAMD");
#endif
}

void midi_send( const uint8_t message_type, uint8_t channel, uint8_t data1, uint8_t data2)
{
  uint8_t status_byte = (uint8_t) (message_type<<4) | channel; // First nibble: message type, second nibble: channel

  if ( channel > 15 ) channel = 15;     // ensure that channel is in the range 0...15
  status_byte |= 0x80;                  // Extra insurance: set msb of status byte
  data1 &= 0x7F;                        // and clear msb of data bytes
  data2 &= 0x7F;

  MIDI_Serial.write(status_byte);
  MIDI_Serial.write(data1);
  MIDI_Serial.write(data2);
}

void loop() {
//  #if defined(ARDUINO_ARCH_SAMD)
//  SerialUSB.println("ARDUINO_ARCH_SAMD");
//#endif
  SerialUSB.println("Sending note on");
  digitalWrite(LED_BUILTIN, LED_ON);
  midi_send(MIDI_NOTEON, 0, 24, 64);   // Channel 0, middle C, normal velocity
  delay(500);
  SerialUSB.println("Sending note off");
  digitalWrite(LED_BUILTIN, LED_OFF);
  midi_send(MIDI_NOTEOFF, 0, 24, 64);   // Channel 0, middle C, normal velocity
  delay(2000);
}
