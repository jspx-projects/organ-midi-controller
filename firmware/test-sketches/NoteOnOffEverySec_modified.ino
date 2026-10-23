#include <USB-MIDI.h>

USBMIDI_CREATE_DEFAULT_INSTANCE();
#define Serial SerialUSB

unsigned long t1 = millis();

void handleNoteOn(byte inChannel, byte inNumber, byte inVelocity)
{
  Serial.print("NoteOn  ");
  Serial.print(inNumber);
  Serial.print("\tvelocity: ");
  Serial.println(inVelocity);
}

void handleNoteOff(byte inChannel, byte inNumber, byte inVelocity)
{
  Serial.print("NoteOff ");
  Serial.print(inNumber);
  Serial.print("\tvelocity: ");
  Serial.println(inVelocity);
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  Serial.println("Arduino ready.");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void loop()
{
  byte input;
  if( MIDI.read() )
  {
      Serial.print("MIDI.read was called");
  }
  if ((millis() - t1) > 500)
  {
    t1 = millis();

    MIDI.sendNoteOn(27, 55, 1);
    MIDI.sendNoteOff(27, 55, 1);
  }
}
