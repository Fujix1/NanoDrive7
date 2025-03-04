/*
  A0 IO18
  A1 IO8

  D0 IO9
  D1 IO10
  D2 IO11
  D3 IO12
  D4 IO13
  D5 IO14
  D6 IO15
  D7 IO16
  IC IO48

  IO38 CS0
  IO39 CS1
  IO40 WR
  IO43 CS2 (TXD0)
*/

#ifndef COMMON_H
#define COMMON_H

// Current hardware setup
#define CHIP0 CHIP_YM2203_0
#define CHIP1 CHIP_YM3812
#define CHIP2 CHIP_NONE

#define USE_YM2203_0
#define USE_YM2413
#define USE_AY8910
#define USE_YM3812

#define CHIP0_CLOCK CLK_0
#define CHIP1_CLOCK CLK_1
#define CHIP2_CLOCK CLK_0

// LCD
#define LCD_W 170
#define LCD_H 320
#define LCD_CLK 2
#define LCD_MOSI 44
#define LCD_RST 42
#define LCD_DC 41

// DISPLAY
#define TITLE_DEVIDER " *** "
#define SCROLL_SPEED_TITLE .8F   // 文字スクロール速度 px
#define SCROLL_SPEED_GAME .6F    // 文字スクロール速度 px
#define SCROLL_SPEED_AUTHOR .4F  // 文字スクロール速度 px
#define SCROLL_DELAY 2000        // スクロール開始までのディレイ ms
#define DISP_TIMER_INTERVAL 20   // ms 表示更新タイマー間隔

// SD Card
#define SD_CS 7
#define SD_MOSI 6
#define SD_CLK 5
#define SD_MISO 4

#define MAX_PNG_WIDTH 640
#define MAX_FILE_SIZE 7000000  // 6613100

// I2C
#define I2C_SDA 16
#define I2C_SCL 15

// NJU72341
#define NJU72341_MUTE_PIN 17

#endif
