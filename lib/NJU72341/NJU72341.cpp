#include "NJU72341.h"

#include <Wire.h>

#include "../../src/common.h"
#include "../../src/config.h"

#define FADEOUT_STEPS 50

/**
 * 設定 db: 0 -95
 * 96: ミュート
 * NJU72341: 2ch版の場合は 1ch 2ch ステレオ処理のみ
 * NJU72342: 4ch版の場合は 3ch 4ch は個別に設定
 */

// Aカーブの音量マップ
static const uint8_t NJU72341_db[FADEOUT_STEPS] = {0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  3,  3,
                                                   4,  5,  5,  6,  7,  8,  9,  11, 12, 13, 15, 17, 18, 20, 22, 24, 26,
                                                   28, 30, 32, 35, 38, 41, 44, 48, 52, 57, 62, 68, 74, 80, 88, 96};

static TimerHandle_t hFadeOutTimer;  // フェードアウト用タイマー
static u32_t _fadeOutStartMS;
static byte _fadeOutStep;

static void fadeOutTimerHandler(void* param) {
  if (nju72341.fadeOutStatus != FADEOUT_PROCESSING) {
    return;
  }

  nju72341.setAVolume(_fadeOutStep++);

  if (_fadeOutStep == FADEOUT_STEPS) {
    xTimerStop(hFadeOutTimer, 0);
    nju72341.fadeOutStatus = FADEOUT_COMPLETED;
    _fadeOutStep = 0;
  }
}

void NJU72341::init(uint16_t fadeOutDuration, bool NJU72342) {
  Wire.begin(I2C_SDA, I2C_SCL, 400000);

  if (NJU72342) {
    _slaveAddress = NJU72342_ADDR;
    _isNJU72342 = true;
  } else {
    _slaveAddress = NJU72341_ADDR;
    _isNJU72342 = false;
  }
  _attenuation = 0;  // 音量調整値

  fadeOutStatus = FADEOUT_BEFORE;  // フェードアウトしてない
  _isFadeoutEnabled = (fadeOutDuration != FO_0);

  mute();                  // メイン(3ch+4ch)ミュート
  setVolume_1B(0);         // 1ch
  setVolume_2B(0);         // 2ch
  setInputGain(1, GAIN9);  // 入力ゲイン設定
  setInputGain(2, GAIN9);
  setInputGain(3, GAIN9);
  setInputGain(4, GAIN9);

  // タイマー生成
  if (fadeOutDuration == FO_0) {
    _fadeOutDuration = 8000;
  } else {
    _fadeOutDuration = fadeOutDuration;
  }

  hFadeOutTimer = xTimerCreate("FADEOUT_TIMER", _fadeOutDuration / FADEOUT_STEPS, pdTRUE, NULL, fadeOutTimerHandler);
}

void NJU72341::setFadeoutDuration(uint16_t fadeOutDuration) {
  if (fadeOutDuration == FO_0) {
    _isFadeoutEnabled = false;
  } else {
    if (fadeOutDuration != _fadeOutDuration) {
      _isFadeoutEnabled = true;
      _fadeOutDuration = fadeOutDuration;
      xTimerChangePeriod(hFadeOutTimer, _fadeOutDuration / FADEOUT_STEPS, 0);
    }
  }
}

void NJU72341::startFadeout() {
  // フェードアウトは 3ch 4ch のみ
  // すでに処理中のときはなにもしない。フェード時間よりループが短い場合
  if (fadeOutStatus == FADEOUT_PROCESSING) {
    return;
  }

  _fadeOutStartMS = millis();
  _fadeOutStep = 0;
  if (_isFadeoutEnabled) {
    fadeOutStatus = FADEOUT_PROCESSING;
    xTimerStart(hFadeOutTimer, 0);
  } else {
    fadeOutStatus = FADEOUT_COMPLETED;
  }
}

// att 減衰量dB:
void NJU72341::reset(u8_t att) {
  if (att >= 0) {
    _attenuation = att;
  }
  fadeOutStatus = FADEOUT_BEFORE;
  resetFadeout();
}

void NJU72341::resetFadeout() {
  xTimerStop(hFadeOutTimer, 0);
  fadeOutStatus = FADEOUT_BEFORE;
  _fadeOutStep = 0;
}

void NJU72341::setInputGain(u8_t ch, tNJU72341_GAIN newInputGain) {
  uint8_t shift = (ch - 1) * 2;
  _currentGain &= ~(0b11 << shift);                  // 指定chの2bitをクリア
  _currentGain |= ((newInputGain & 0b11) << shift);  // 指定chに新値セット
  Wire.beginTransmission(_slaveAddress);
  Wire.write(0x00);
  Wire.write(_currentGain);
  Wire.endTransmission();
}

// 全チャンネルの音量設定
// 0:最大音量, 96: ミュート
void NJU72341::setVolumeAll(uint8_t newGain) {
  uint8_t bit = 119 - newGain - _attenuation;
  Wire.beginTransmission(_slaveAddress);
  Wire.write(0x01);
  Wire.write(bit);
  Wire.write(bit);
  if (_isNJU72342) {
    Wire.write(bit);
    Wire.write(bit);
  }
  Wire.endTransmission();
  _currentVolume[0] = newGain;  //
  _currentVolume[1] = newGain;  //
  _currentVolume[2] = newGain;  //
  _currentVolume[3] = newGain;  //
}

void NJU72341::setVolume_1B_2B(uint8_t newGain) {
  if (_currentVolume[0] == newGain) {
    return;  // 変更なしのとき
  }
  uint8_t bit = 119 - newGain;

  Wire.beginTransmission(_slaveAddress);
  Wire.write(0x01);
  Wire.write(bit);
  Wire.write(bit);
  Wire.endTransmission();

  _currentVolume[0] = newGain;  // 1ch
  _currentVolume[1] = newGain;  // 2ch
}

void NJU72341::setVolume_3B_4B(uint8_t newGain) {
  if (_currentVolume[2] == newGain) {
    return;  // 変更なしのとき
  }
  uint8_t bit = 119 - newGain;

  Wire.beginTransmission(_slaveAddress);
  Wire.write(0x03);
  Wire.write(bit);
  Wire.write(bit);
  Wire.endTransmission();

  _currentVolume[2] = newGain;  // 3ch
  _currentVolume[3] = newGain;  // 4ch
}

void NJU72341::setVolume_1B(uint8_t newGain) {
  if (_currentVolume[0] == newGain) {
    return;  // 変更なしのとき
  }
  uint8_t bit = 119 - newGain;
  Wire.beginTransmission(_slaveAddress);
  Wire.write(0x01);
  Wire.write(bit);
  Wire.endTransmission();
  _currentVolume[0] = newGain;  // 1ch
}

void NJU72341::setVolume_2B(uint8_t newGain) {
  if (_currentVolume[1] == newGain) {
    return;  // 変更なしのとき
  }
  uint8_t bit = 119 - newGain;
  Wire.beginTransmission(_slaveAddress);
  Wire.write(0x02);
  Wire.write(bit);
  Wire.endTransmission();
  _currentVolume[1] = newGain;  // 2ch
}

void NJU72341::setVolume_3B(uint8_t newGain) {
  if (_currentVolume[2] == newGain) {
    return;  // 変更なしのとき
  }
  uint8_t bit = 119 - newGain;
  Wire.beginTransmission(_slaveAddress);
  Wire.write(0x03);
  Wire.write(bit);
  Wire.endTransmission();
  _currentVolume[2] = newGain;  // 3ch
}

void NJU72341::setVolume_4B(uint8_t newGain) {
  if (_currentVolume[3] == newGain) {
    return;  // 変更なしのとき
  }
  uint8_t bit = 119 - newGain;
  Wire.beginTransmission(_slaveAddress);
  Wire.write(0x04);
  Wire.write(bit);
  Wire.endTransmission();
  _currentVolume[3] = newGain;  // 4ch
}

void NJU72341::mute() {
  // Serial.printf("NJU72341::mute();\n");
  // setVolume_1B_2B(96);
  setVolume_3B_4B(96);
  _isMuted = true;
}

void NJU72341::unmute() {
  // Serial.printf("NJU72341::unmute();\n");
  // setVolume_1B_2B(_attenuation);
  setVolume_3B_4B(_attenuation);
  _isMuted = false;
}

void NJU72341::setAVolume(uint8_t step) {
  if (step > FADEOUT_STEPS - 1) {
    step = FADEOUT_STEPS - 1;
  }
  // setVolume_1B_2B(NJU72341_db[step] + _attenuation);
  setVolume_3B_4B(NJU72341_db[step] + _attenuation);
}

NJU72341 nju72341 = NJU72341();
