/*
 * NJU72341, NJU72342 I2C Volume Controller
 */

#ifndef __NJU72341_H
#define __NJU72341_H

#include <Arduino.h>

#include "../../src/config.h"

#define NJU72341_ADDR 0x44  // I2C address
#define NJU72342_ADDR 0x40  // I2C address

typedef enum { GAIN0 = 0b0000, GAIN3 = 0b0101, GAIN6 = 0b1010, GAIN9 = 0b1111 } tNJU72341_GAIN;

typedef enum {
  FADEOUT_BEFORE,      // フェードアウト前
  FADEOUT_PROCESSING,  // フェードアウト処理中
  FADEOUT_COMPLETED,   // フェードアウト完了
} tFadeOutStatus;

class NJU72341 {
 public:
  tFadeOutStatus fadeOutStatus = FADEOUT_BEFORE;
  void init(u16_t fadeOutDuration, bool NJU72342);
  void reset(u8_t att);
  void setInputGain(u8_t ch, tNJU72341_GAIN newInputGain);
  void setVolume_1B_2B(u8_t newGain);  // ch1と2の音量設定
  void setVolume_3B_4B(u8_t newGain);  // ch3と4の音量設定
  void setVolume_1B(u8_t newGain);     // ch1の音量設定
  void setVolume_2B(u8_t newGain);     // ch2の音量設定
  void setVolume_3B(u8_t newGain);     // ch3の音量設定
  void setVolume_4B(u8_t newGain);     // ch4の音量設定
  void setVolumeAll(u8_t newGain);     // 全チャンネルの音量設定
  void setAVolume(u8_t step);
  void mute();
  void unmute();
  void startFadeout();
  void setFadeoutDuration(u16_t fadeOutDuration);
  void resetFadeout();

 private:
  bool _isMuted = false;
  u8_t _currentVolume[4] = {96, 96, 96, 96};  // 各チャンネルの音量
  u8_t _attenuation = 0;                      // 音量調整値
  u32_t _fadeoutStarted;                      // フェードアウト開始時間
  bool _isFadeoutEnabled;                     // フェードアウトするか
  u16_t _fadeOutDuration;
  u8_t _slaveAddress;
  bool _isNJU72342;
  u8_t _currentGain;
};

extern NJU72341 nju72341;

#endif