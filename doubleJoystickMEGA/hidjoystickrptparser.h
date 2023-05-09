#if !defined(__HIDJOYSTICKRPTPARSER_H__)
#define __HIDJOYSTICKRPTPARSER_H__

#include <usbhid.h>

#define BUF_OFFSET 6  //&(info of turntable) - &buf
struct GamePadEventData {
  uint8_t _[BUF_OFFSET];
  uint8_t A;
};

class JoystickEvents {
public:
  virtual void OnGamePadChanged(bool side, const signed char direction, const GamePadEventData *evt);
  virtual void OnHatSwitch(bool side, uint8_t hat);
  virtual void OnButtonUp(bool side, uint8_t but_id);
  virtual void OnButtonDn(bool side, uint8_t but_id);
};

class JoystickReportParser : public HIDReportParser {
  JoystickEvents *joyEvents;

  uint8_t oldPad;
  uint8_t oldHat;
  uint16_t oldButtons;

  bool turntable_turns = false;
  unsigned long last_turntable;

public:
  uint16_t state;
  JoystickReportParser(JoystickEvents *evt);

  virtual void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};

#endif  // __HIDJOYSTICKRPTPARSER_H__
