#ifndef DISP_H
#define DISP_H

#include <LovyanGFX.h>
#include <PNGdec.h>
#include <SPI.h>

#include "OpenFontRender.h"
#include "common.h"
#include "config.h"
#include "file.h"
#include "fonts.h"
#include "input.h"

#define C_BASEBG TFT_BLACK
#define C_BASEFG TFT_WHITE
#define C_ORANGE 0xfdc7
#define C_YELLOW 0xfee0

#define C_ACCENT_LIGHT 0x26df
#define C_ACCENT_DARK 0x1396

#define C_LIGHTGRAY 0xef7d
#define C_GRAY 0xad55
#define C_MID 0x73ae
#define C_DARK 0x10c4

#define C_HEADER 0x4228           // 0x444444
#define C_BORDER 0xad55           // 0xadaaad
#define C_FOOTER_ACTIVE 0x530c    // 0x506065
#define C_FOOTER_INACTIVE 0xbe1a  // 0xbbc0d0

#define ITEM_HEIGHT 32

class LGFX : public lgfx::LGFX_Device {
 private:
  lgfx::Panel_ST7789 _panel_instance;

 public:
  lgfx::Bus_SPI _bus_instance;
  LGFX(void);
};

extern LGFX lcd;

typedef struct {
  String trackEn, trackJp, gameEn, gameJp, systemEn, systemJp, authorEn, authorJp, date;
  String chip0, chip1, type;
  uint64_t time;
  uint32_t no, maxFiles;
} tDispData;

// void redrawOnCore0();
bool openPNG(String dirName, String fileName, bool AA, bool sprite);

// 現在の画面表示モード
enum class ViewMode { Player, Config, Visual };

// 画面 クラス
class Disp {
 private:
  LGFX_Sprite _sprHeader;  // ヘッダ部分のスプライト

 public:
  bool init();                      // 初期化
  void drawBG();                    // 背景描画
  void redraw();                    // プレーヤー描画
  void updateDisp(tDispData data);  // 表示内容更新
  void serialModeDraw();            // シリアルモード画面描画
  void startTimer();                // 描画タイマー開始
  void stopTimer();                 // 描画タイマー停止
  void updateHeader(uint64_t sec);  // ヘッダ更新

  tDispData dispData;                       // 各種表示テキストなど
  ViewMode currentView = ViewMode::Player;  // デフォルトはプレーヤー画面
  OpenFontRender render;                    // OpenFontRenderer
  Disp() : _sprHeader(&lcd) {}
};

extern Disp disp;

// Label クラス
class Label {
 public:
  Label(const int16_t x,  // 座標
        const int16_t y,
        const int16_t w,  // 幅
        const uint16_t fontColor, const uint16_t bgColor, const uint16_t fontSize, const float scrollSpeed,
        const Align textAlign);
  void setCaption(String newCaption);
  void update();
  void setEnabled(bool state);

 private:
  float _n = 0;  // ラベルスクロール量
  uint32_t _x = 0, _y = 0, _labelWidth = 0, _textWidth = 0, _devWidth = 0, _startTick = 0;
  uint16_t _fontColor;
  uint16_t _fontSize;
  uint16_t _bgColor;
  String _caption;
  LGFX_Sprite _sprite;
  Align _textAlign;
  int _scrollCount;  // スクロール済み回数
  bool _isScrolling = false;
  float _scrollSpeed;
  bool _enabled = false;
};

// 設定画面クラス
class CFGWindow {
 private:
  LGFX_Sprite _sprite;
  LGFX_Sprite _sprFooter;
  // 言語別ヘッダー用スプライト
  LGFX_Sprite _sprHeaderJP;
  LGFX_Sprite _sprHeaderEN;

  // ヘッダースプライトの初期化
  void initHeaders();

 public:
  bool isVisible = false;
  int currentItemIndex = 0;

  void init();
  void show();
  void close();
  void draw();
  void drawItem(int index, bool toFrameBuffer);
  void drawFooter(bool toFrameBuffer);
  void up();
  void down();
  void left();
  void right();
};

extern CFGWindow cfgWindow;

#endif