#ifndef INPUT_H
#define INPUT_H
#include <Arduino.h>

#define INPUT_PIN 1
#define INPUT_CAPTURE_INTERVAL 60  // ms キャプチャインターバル
#define INPUT_REPEAT_DELAY 300     // ms リピート開始までの時間

typedef enum { btnNONE, btnRIGHT, btnUP, btnDOWN, btnLEFT, btnSELECT, btnFUNC } Button;

// ND6 の共通イベント。ND6.1 のブラウザ用イベントは含めない。
enum class event { None, Right, Up, Down, Left, Option, Close, SwitchView };

#define VAL_0 0
#define VAL_1 500
#define VAL_2 1250
#define VAL_3 1960
#define VAL_4 2900
#define VAL_NONE 4095

class Input {
 public:
  bool init();
  void inputHandler();
  void setEnabled(bool state);
  bool isEnabled() const;
  Button checkButton();

 private:
  volatile bool _enabled = false;
  uint32_t _buttonRepeatStarted = 0;
  Button _lastButton = btnNONE;
  Button _readButton();
};

extern Input input;

#endif
