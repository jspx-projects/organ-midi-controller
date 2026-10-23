// Simple test program to test MIDI library on SAMD21 board

#include <MIDI.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midi1);
#define LED_BUILTIN   26      //TX led
#define LED_ON       LOW
#define LED_OFF      HIGH

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_OFF);
  midi1.begin(MIDI_CHANNEL_OMNI);
}

void loop() {
  SerialUSB.println("Sending note on");
  digitalWrite(LED_BUILTIN, LED_ON);
  midi1.sendNoteOn(24, 64, 1);   // Channel 0, middle C, normal velocity
  delay(500);
  SerialUSB.println("Sending note off");
  digitalWrite(LED_BUILTIN, LED_OFF);
  midi1.sendNoteOff(24, 64, 1);   // Channel 0, middle C, normal velocity
  delay(2000);
}
