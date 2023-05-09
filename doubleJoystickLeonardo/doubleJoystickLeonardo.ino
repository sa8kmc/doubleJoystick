#include <usbhid.h>
#include <hiduniversal.h>
#include <usbhub.h>
#include "U8glib.h"

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

U8GLIB_SH1106_128X64 u8g(5, 4, 1, 2, 3); // SW SPI Com: sck, mosi, cs, a0, reset

#include "hidjoystickrptparser.h"

USB Usb;
USBHub Hub(&Usb);
USBHub Hub2(&Usb);
HIDUniversal Hid1(&Usb); // first Joystick
HIDUniversal Hid2(&Usb); // second Joystick

JoystickEvents Joy1Events;
JoystickEvents Joy2Events;

JoystickReportParser Joy1(&Joy1Events);
JoystickReportParser Joy2(&Joy2Events);

bool ready1 = false;
bool ready2 = false;

void setup()
{
  // Power source to OLED display.
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);
  pinMode(7, OUTPUT);
  digitalWrite(7, LOW);

  // start serial connection
  Serial.begin(9600);
#if !defined(__MIPSEL__)
  while (!Serial)
    ; // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
  USBTRACE("Start...");

  if (Usb.Init() == -1)
    USBTRACE("OSC did not start.");
  else
    USBTRACE("OSC started.");

  delay(200);

  if (!Hid1.SetReportParser(0, &Joy1))
    ErrorMessage<uint8_t>(PSTR("SetReportParser1"), 1);
  USBTRACE("OSC1 ready.");
  if (!Hid2.SetReportParser(0, &Joy2))
    ErrorMessage<uint8_t>(PSTR("SetReportParser2"), 1);
  USBTRACE("OSC2 ready.");
}

#pragma region TX Bolt protocol
// TX-Bolt protocol(LSB):
// 00HWPKTS|01UE*OAR|10GLBPRF|110#ZDST
// bit index(hex,LSB):
// 0      0|0      0|1      1|1      1
// 7      0|F      8|7      0|F      8

// tape of Stenoword actions:
// #PTHKSLM|NRUAOIE$

// in spatial order of gamepad:
// #PLMTSKH|OAUINRE$
// L---------------R

// plover configuration of keymap from TX Bolt keys to Stenoword actions in tape order:
// #  S- T- P- H- *  A- O- | -E -U -F -P -L -T -D  $

// plover configuration of keymap from TX Bolt keys to Stenoword actions in gamepad order
// (same as the keymap from controller keys to Stenoword-JP actions; including function keys):
// #  S- A- O- T- *  H- P- | -L -P -F -T -E -U -D -Z
//      K- W-              |               -R -B

// corresponding bit index(hex):
// 1C 00 09 0A 01 0B 05 03 | 14 12 10 18 0C 0D 1A 1B
//      02 04              |               11 13

// unused keys in TX Bolt: R- -B -L -G -S

// machine signals converted from keystrokes
uint32_t bolt = 0;
// logical OR of keystrokes
uint16_t key_been = 0;
// current keystrokes from each gamepad
uint16_t state1 = 0, state2 = 0;
// former value of stateN (Used to detect changes in direction of rotation of the turntables)
uint16_t old_state1 = 0, old_state2 = 0;
// actual packets sent to steno software
byte val0 = 0;
byte val1 = 0;
byte val2 = 0;
byte val3 = 0;

// send TX Bolt packets to steno software
void send_bolt()
{
  val0 = bolt & 0xFF;
  val1 = ((bolt >> 8) & 0xFF) | 0x40;
  val2 = ((bolt >> 16) & 0xFF) | 0x80;
  val3 = ((bolt >> 24) & 0xFF) | 0xC0;
  Serial.write(val0);
  Serial.write(val1);
  Serial.write(val2);
  Serial.write(val3);
}

// reset TX Bolt packets
void erase_bolt()
{
  val0 = 0;
  val1 = 0;
  val2 = 0;
  val3 = 0;
}

// Indicates any key pressed.
// Send TX Bolt signal at falling edge of this variable.
bool writing = false;

// map from each bit of stateN to the bit of TX Bolt keys
// SSXXXXOOXKKKKKKk|SSXXXXOOXKKKKKKk
uint8_t bolt_bit_indices[32] = {
    0x00, 0X09, 0X0A, 0X01, 0X0B, 0X05, 0X03, 0X1F, //
    0X02, 0X04, 0X1F, 0X1F, 0X1F, 0X1F, 0X1C, 0X1C, //
    0X14, 0X12, 0X10, 0X18, 0X0C, 0X0D, 0X1A, 0X1F, //
    0X11, 0X13, 0X1F, 0X1F, 0X1F, 0X1F, 0X1B, 0X1B, //
};
#pragma endregion

#pragma region OLED display
void u8g_prepare(void)
{
  u8g.setFont(u8g_font_6x10);
  u8g.setFontRefHeightExtendedText();
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();
}
// whether i-th bit of 1st gamepad input stands
bool stands1(uint8_t i)
{
  return (state1 >> i) & 1;
}
// whether i-th bit of 2nd gamepad input stands
bool stands2(uint8_t i)
{
  return (state2 >> i) & 1;
}
// display rectangle; filled shape if pressed, outline of shape if not
void drawRect(bool pressed, u8g_uint_t x, u8g_uint_t y, u8g_uint_t w, u8g_uint_t h)
{
  if (pressed)
    u8g.drawBox(x, y, w, h);
  else
    u8g.drawFrame(x, y, w, h);
}
// upmost position of displaying the keystroke;
#define IND_TOP 44
// display current keystroke of gamepads
void draw(void)
{
  u8g_prepare();
  for (uint8_t i = 14; i < 16; i++)
  {
    drawRect(stands1(i), 0, IND_TOP + 10 * (i - 14), 14, 10);
    drawRect(stands2(i), 128 - 14, IND_TOP + 10 * (i - 14), 14, 10);
  }
  for (uint8_t i = 8; i < 10; i++)
  {
    drawRect(stands1(i), 64 - 6 - 18 + 10 * (i - 8), IND_TOP + 15, 8, 5);
    drawRect(stands2(i), 64 + 6 - 0 + 10 * (i - 8), IND_TOP + 15, 8, 5);
  }
  for (uint8_t i = 0; i < 7; i++)
  {
    drawRect(stands1(i), 64 - 3 - 42 + 6 * i, IND_TOP + 3 - 1 * (i % 2), 6, 10);
    drawRect(stands2(i), 64 + 3 - 0 + 6 * i, IND_TOP + 3 - 1 * (i % 2), 6, 10);
  }
}
// display a message if USB shield is not prepared
void draw_of_standby(void)
{
  u8g_prepare();
  u8g.drawStr(34, 27, "Standby...");
}

// tape of stenoword actions
uint8_t chars[14] = {'P', 'T', 'H', 'K', 'S', 'L', 'M', 'N', 'R', 'U', 'A', 'O', 'I', 'E'};
// map each keystroke to stenoword tape
uint8_t key_to_steno_pos[14] = {0, 5, 6, 1, 4, 3, 2, 11, 10, 9, 12, 7, 8, 13};
// display which string in tape will be sent
void steno_indicate(void)
{
  char res[15] = "______________";
  for (uint8_t i = 0; i < 15; i++)
  {
    if (i == 7)
      continue;
    uint8_t j = key_to_steno_pos[i - (i >> 3)];
    if ((key_been >> i) & 1)
      res[j] = chars[j];
  }
  u8g.drawStr(64 - 42, 19, res);
}
#pragma endregion

void loop()
{
  // USB input
  Usb.Task();
#pragma region OLED update
  u8g.firstPage();
  if (!ready1 && Hid1.isReady())
    ready1 = true;
  if (!ready2 && Hid2.isReady())
    ready2 = true;
  if (ready1 && ready2)
  {
    do
    {
      draw();
      steno_indicate();
    } while (u8g.nextPage());
  }
  else
  {
    do
    {
      draw_of_standby();
    } while (u8g.nextPage());
  }
#pragma endregion
#pragma region state conversion
  state1 = Joy1.state;
  state2 = Joy2.state;
  key_been |= (state1 & 0xFF) << 0;
  key_been |= (state2 & 0xFF) << 8;

  if ((state1 | state2) != 0)
  {
    writing = true;
    for (uint8_t i = 0; i < 16; i++)
    {
      bolt |= (uint32_t)((state1 >> i) & 1) << bolt_bit_indices[i];
    }
    for (uint8_t i = 0; i < 16; i++)
    {
      bolt |= (uint32_t)((state2 >> i) & 1) << bolt_bit_indices[i + 16];
    }
    digitalWrite(13, HIGH);
    if (((state1 ^ old_state1) >> 14 == 3) || ((state2 ^ old_state2) >> 14 == 3))
    {
      send_bolt();
      erase_bolt();
      digitalWrite(13, LOW);
    }
  }
  else if (writing)
  {
    writing = false;
    send_bolt();
    erase_bolt();
    digitalWrite(13, LOW);
    bolt = 0;
    key_been = 0;
  }
  old_state1 = state1;
  old_state2 = state2;
#pragma endregion
}