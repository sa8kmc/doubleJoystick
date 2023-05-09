#include <usbhid.h>
#include <hiduniversal.h>
#include <usbhub.h>

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

#include "hidjoystickrptparser.h"

USB Usb;
USBHub Hub(&Usb);
USBHub Hub2(&Usb);
HIDUniversal Hid1(&Usb);  // first Joystick
HIDUniversal Hid2(&Usb);  // second Joystick

JoystickEvents Joy1Events;
JoystickEvents Joy2Events;

JoystickReportParser Joy1(&Joy1Events);
JoystickReportParser Joy2(&Joy2Events);

bool ready1 = false;
bool ready2 = false;

uint8_t PIN_PENDING1 = 34;
uint8_t PIN_PENDING2 = 35;

#pragma region txbolt_protocol
boolean key_any = false;
byte val0 = 0;
byte val1 = 0;
byte val2 = 0;
byte val3 = 0;
#pragma endregion

void setup() {
  for (uint8_t pin = 20; pin < 53; pin++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  Serial.begin(9600);
#if !defined(__MIPSEL__)
  while (!Serial)
    ;  // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
  USBTRACE("Start...");

  if (Usb.Init() == -1)
    USBTRACE("OSC did not start.");
  else
    USBTRACE("OSC started.");

  //pilot lamp
  digitalWrite(PIN_PENDING1, LOW);
  digitalWrite(PIN_PENDING2, LOW);
  delay(200);

  if (!Hid1.SetReportParser(0, &Joy1))
    ErrorMessage<uint8_t >(PSTR("SetReportParser1"), 1);
  USBTRACE("OSC1 ready.");
  if (!Hid2.SetReportParser(0, &Joy2))
    ErrorMessage<uint8_t >(PSTR("SetReportParser2"), 1);
  USBTRACE("OSC2 ready.");
}

bool phase_input = false;

//TX-Bolt protocol(LSB):
//00HWPKTS|01UE*OAR|10GLBPRF|110#ZDST
//bit index(hex,LSB):
//0      0|0      0|1      1|1      1
//7      0|F      8|7      0|F      8

//In stenoword configure,
//#PTHKSLM|NRUAOIE$

//in spatial arrangement of BMS controller,
//#PLMTSKH|OAUINRE$
//L---------------R

//plover configuration from TX Bolt signal to Stenoword-JP logic in tape coordinate:
//#  S- T- P- H- *  A- O- | -E -U -F -P -L -T -D  $

//plover configuration from TX Bolt signal to Stenoword-JP logic in BMS coordinate:
//#  S- A- O- T- *  H- P- | -L -P -F -T -E -U -D  $

//corresponding bit index(hex):
//1C 00 09 0A 01 0B 05 03 | 14 12 10 18 0C 0D 1A 1B
//     02 04              |               11 13

uint32_t bolt = 0;
uint16_t state1 = 0, state2 = 0;
uint16_t old_state1 = 0, old_state2 = 0;

void send_bolt() {
  val0 = bolt & 0xFF;
  val1 = ((bolt >> 8) & 0xFF) | 0x40;
  val2 = ((bolt >> 16) & 0xFF) | 0x80;
  val3 = ((bolt >> 24) & 0xFF) | 0xC0;
  Serial.write(val0);
  Serial.write(val1);
  Serial.write(val2);
  Serial.write(val3);
}

void erase_bolt() {
  val0 = 0;
  val1 = 0;
  val2 = 0;
  val3 = 0;
}

bool written = false;

//SSXXXXOOXKKKKKKk|SSXXXXOOXKKKKKKk
uint8_t bolt_bit_indices[32] = {
  0x00, 0X09, 0X0A, 0X01, 0X0B, 0X05, 0X03, 0X1F,  //
  0X02, 0X04, 0X1F, 0X1F, 0X1F, 0X1F, 0X1C, 0X1C,  //
  0X14, 0X12, 0X10, 0X18, 0X0C, 0X0D, 0X1A, 0X1F,  //
  0X11, 0X13, 0X1F, 0X1F, 0X1F, 0X1F, 0X1B, 0X1B,  //
};



void loop() {
  Usb.Task();

  //pilot lamp
  if (!ready1 && Hid1.isReady()) {
    ready1 = true;
    digitalWrite(PIN_PENDING1, HIGH);
  }
  if (!ready2 && Hid2.isReady()) {
    ready2 = true;
    digitalWrite(PIN_PENDING2, HIGH);
  }

  state1 = Joy1.state;
  state2 = Joy2.state;
  if ((state1 | state2) != 0) {
    written = true;
    for (uint8_t i = 0; i < 16; i++) {
      bolt |= (uint32_t)((state1 >> i) & 1) << bolt_bit_indices[i];
    }
    for (uint8_t i = 0; i < 16; i++) {
      bolt |= (uint32_t)((state2 >> i) & 1) << bolt_bit_indices[i + 16];
    }
    digitalWrite(13, HIGH);
    if (((state1 ^ old_state1) >> 14 == 3) || ((state2 ^ old_state2) >> 14 == 3)) {
      send_bolt();
      erase_bolt();
      digitalWrite(13, LOW);
    }
  } else if (written) {
    written = false;
    send_bolt();
    erase_bolt();
    digitalWrite(13, LOW);
    bolt = 0;
  }
  old_state1 = state1;
  old_state2 = state2;
}