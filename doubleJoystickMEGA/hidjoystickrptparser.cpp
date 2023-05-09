#include "Arduino.h"
#include "hidjoystickrptparser.h"

JoystickReportParser::JoystickReportParser(JoystickEvents *evt)
  : joyEvents(evt),
    oldHat(0xDE),
    oldButtons(0),
    oldPad(0),
    state(0) {}

#define SIDE_L 10
#define SIDE_R 12
#define TURNTABLE_TIMEOUT 200  //[msec]

char tmp[256];

bool side(uint8_t address) {
  switch (address) {
    case SIDE_L: return 1;
    case SIDE_R: return 0;
    default: return 1;
  }
}

int8_t pin_assign[17] = {
  0, 8, 0, 0,
  9, 0, 0, 0,
  7, 6, 0, 2,
  4, 0, 5, 3,
  1
};  //input is 1-origin

void JoystickReportParser::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
  for (uint8_t i = 0; i < BUF_OFFSET; i++) {
    PrintHex(i, 0x81);
    PrintHex(buf[i], 0x81);
    USBTRACE1("\t", 0x81);
  }
  USBTRACE1("\n", 0x81);
#pragma region turntable
  bool match = true;
  bool turn_event_occurs = false;
  unsigned long time = millis();
  // Checking if there are changes in report since the method was last called
  if (buf[BUF_OFFSET] != oldPad) {
    match = false;
  }
  // Calling Game Pad event handler
  if (!match && joyEvents) {
    signed char part = (buf[BUF_OFFSET] - oldPad) & 0xFF;
    signed char signbit = (part > 0) - (part < 0);
    joyEvents->OnGamePadChanged(side(hid->GetAddress()), signbit, (const GamePadEventData *)buf);
    oldPad = buf[BUF_OFFSET];
    turntable_turns = true;
    last_turntable = time;
    state = (state & ~(3 << 14)) | (((signbit + 3) / 2) << 14);
    turn_event_occurs = true;
  } else {
    if (turntable_turns && time - last_turntable >= TURNTABLE_TIMEOUT) {
      joyEvents->OnGamePadChanged(side(hid->GetAddress()), 0, (const GamePadEventData *)buf);
      turntable_turns = false;
      state = (state & ~(3 << 14));
      turn_event_occurs = true;
    }
  }
#pragma endregion
#pragma region buttons
  uint16_t buttons = (0x0000 | buf[3]);
  buttons <<= 8;
  buttons |= buf[2];
  uint16_t changes = (buttons ^ oldButtons);

  // Calling Button Event Handler for every button changed
  if (changes) {
    PrintHex<uint8_t>(side(hid->GetAddress()), 0x80);
    USBTRACE(" -- ");
    PrintHex<uint16_t>(buttons, 0x80);
    USBTRACE("\n");
    for (uint8_t i = 0; i < 0x10; i++) {
      uint16_t mask = (0x0001 << i);
      if (((mask & changes) > 0) && joyEvents) {
        if ((buttons & mask) > 0) {
          joyEvents->OnButtonDn(side(hid->GetAddress()), i + 1);
          //state
          uint8_t pin = pin_assign[i + 1];
          state |= (1 << (pin / 8 + (pin - 1)));
        } else {
          joyEvents->OnButtonUp(side(hid->GetAddress()), i + 1);
          //state
          uint8_t pin = pin_assign[i + 1];
          state &= ~(1 << (pin / 8 + (pin - 1)));
        }
      }
    }
    oldButtons = buttons;
  }
  if (turn_event_occurs || changes) {
    PrintHex<uint16_t>(state, 0x7f);
    USBTRACE1("\n", 0x7f);
  }
#pragma endregion
}

#define PIN_TURN_LEFT_CCW 20
#define PIN_TURN_LEFT_CW 21
#define PIN_TURN_RIGHT_CCW 48
#define PIN_TURN_RIGHT_CW 49

void JoystickEvents::OnGamePadChanged(bool leftside, const signed char direction, const GamePadEventData *evt) {
  PrintHex<uint8_t>(leftside, 0x80);
  USBTRACE("X1: ");
  switch (direction) {
    case 1:
      USBTRACE("-> ");
      break;
    case -1:
      USBTRACE("<- ");
      break;
    case 0: break;
  }
  uint8_t ccwState, cwState;
  switch (direction) {
    case 1:
      ccwState = HIGH;
      cwState = LOW;
      break;
    case -1:
      ccwState = LOW;
      cwState = HIGH;
      break;
    default:
      ccwState = HIGH;
      cwState = HIGH;
      break;
  }
  if (leftside) {
    digitalWrite(PIN_TURN_LEFT_CCW, ccwState);
    digitalWrite(PIN_TURN_LEFT_CW, cwState);
  } else {
    digitalWrite(PIN_TURN_RIGHT_CCW, ccwState);
    digitalWrite(PIN_TURN_RIGHT_CW, cwState);
  }
  PrintHex<uint8_t >(evt->A, 0x80);
  USBTRACE("\n");
}

void JoystickEvents::OnHatSwitch(bool side, uint8_t hat) {
  USBTRACE("Hat Switch: ");
  PrintHex<uint8_t >(hat, 0x80);
  USBTRACE("\n");
}


#define PIN_LEFT_BTN_OFFSET 22
#define PIN_LEFT_FN_OFFSET 22
#define PIN_RIGHT_BTN_OFFSET 39
#define PIN_RIGHT_FN_OFFSET 30


int8_t button_memo[9] = { 16, 11, 15, 12, 14, 9, 8, 1, 4 };  // 1-origin

void JoystickEvents::OnButtonUp(bool leftside, uint8_t but_id) {
  USBTRACE("Up: ");
  PrintHex<uint8_t >(but_id, 0x80);
  USBTRACE("\n");
  uint8_t zeroth_pin;
  if (leftside) {
    if (but_id >= 8)
      zeroth_pin = PIN_LEFT_BTN_OFFSET;
    else
      zeroth_pin = PIN_LEFT_FN_OFFSET;
  } else {
    if (but_id >= 8)
      zeroth_pin = PIN_RIGHT_BTN_OFFSET;
    else
      zeroth_pin = PIN_RIGHT_FN_OFFSET;
  }
  uint8_t pin = pin_assign[but_id];
  digitalWrite(zeroth_pin + pin, HIGH);
}

void JoystickEvents::OnButtonDn(bool leftside, uint8_t but_id) {
  USBTRACE("Dn: ");
  PrintHex<uint8_t >(but_id, 0x80);
  USBTRACE("\n");
  uint8_t zeroth_pin;
  if (leftside) {
    if (but_id >= 8)
      zeroth_pin = PIN_LEFT_BTN_OFFSET;
    else
      zeroth_pin = PIN_LEFT_FN_OFFSET;
  } else {
    if (but_id >= 8)
      zeroth_pin = PIN_RIGHT_BTN_OFFSET;
    else
      zeroth_pin = PIN_RIGHT_FN_OFFSET;
  }
  uint8_t pin = pin_assign[but_id];
  digitalWrite(zeroth_pin + pin, LOW);
}
