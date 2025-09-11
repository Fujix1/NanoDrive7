#ifndef VGM_H
#define VGM_H

#include <math.h>

#include <vector>

#include "SI5351.hpp"
#include "common.h"
#include "freertos/semphr.h"

#define XGM1_MAX_PCM_CH 8
#define XGM1_PCM_DELAY 68

// XGM V2 FM
#define WAIT_SHORT 0x00 ... 0x0e
#define WAIT_LONG 0x0F
#define PCM 0x10 ... 0x1f
#define FM_LOAD_INST 0x20 ... 0x2f
#define FM_FREQ 0x30 ... 0x3f
#define FM_KEY 0x40 ... 0x4f
#define FM_KEY_SEQ 0x50 ... 0x5f
#define FM0_PAN 0x60 ... 0x6f
#define FM1_PAN 0x70 ... 0x7f
#define FM_FREQ_WAIT 0x80 ... 0x8f
#define FM_TL 0x90 ... 0x9f
#define FM_FREQ_DELTA 0xa0 ... 0xaf
#define FM_FREQ_DELTA_WAIT 0xb0 ... 0xbf
#define FM_TL_DELTA 0xc0 ... 0xcf
#define FM_TL_DELTA_WAIT 0xd0 ... 0xdf
#define FM_WRITE 0xe0 ... 0xef
#define FRAME_DELAY 0xf0

#define FM_KEY_ADV 0xf8
#define FM_LFO 0xf9
#define FM_CH3_SPECIAL_ON 0xfa
#define FM_CH3_SPECIAL_OFF 0xfb
#define FM_DAC_ON 0xfc
#define FM_DAC_OFF 0xfd
#define FM_LOOP 0xff

// XGM V2 PSG
#define PSG_WAIT_SHORT 0x00 ... 0x0d
#define PSG_WAIT_LONG 0x0e
#define PSG_LOOP 0x0f
#define PSG_FREQ_LOW 0x10 ... 0x1f
#define PSG_FREQ 0x20 ... 0x2f
#define PSG_FREQ_WAIT 0x30 ... 0x3f
#define PSG_FREQ0_DELTA 0x40 ... 0x4f
#define PSG_FREQ1_DELTA 0x50 ... 0x5f
#define PSG_FREQ2_DELTA 0x60 ... 0x6f
#define PSG_FREQ3_DELTA 0x70 ... 0x7f
#define PSG_ENV0 0x80 ... 0x8f
#define PSG_ENV1 0x90 ... 0x9f
#define PSG_ENV2 0xa0 ... 0xaf
#define PSG_ENV3 0xb0 ... 0xbf
#define PSG_ENV0_DELTA 0xc0 ... 0xcf
#define PSG_ENV1_DELTA 0xd0 ... 0xdf
#define PSG_ENV2_DELTA 0xe0 ... 0xef
#define PSG_ENV3_DELTA 0xf0 ... 0xff

#define XGM2_PCM_DELAY 72

// Current format
typedef enum {
  FORMAT_UNKNOWN,
  FORMAT_VGM,
  FORMAT_XGM,
  FORMAT_XGM2,
} t_format;

const std::vector<String> FORMAT_LABEL = {"--", "VGM", "XGM1", "XGM2"};

// GD3 構造体
typedef struct {
  String trackEn, trackJp, gameEn, gameJp, systemEn, systemJp, authorEn, authorJp, date, converted, notes;
} t_gd3;

// チップ定義
typedef enum {
  CHIP_NONE,
  CHIP_SN76489_0,
  CHIP_SN76489_1,
  CHIP_YM2413,    // OPLL
  CHIP_YM2612,    // OPN2
  CHIP_YM2151,    // OPM
  CHIP_YM2203_0,  // OPN
  CHIP_YM2203_1,  // OPN
  CHIP_YM2608,    // OPNA
  CHIP_YM2610,    // OPNB
  CHIP_YM3526,    // OPL
  CHIP_YM3812,    // OPL2
  CHIP_AY8910,    // PSG
  CHIP_YMF262,    // OPL3
} t_chip;

// クロック使用番号
typedef enum { CLK_0, CLK_1, CLK_2, CLK_NONE, CLK_FIXED } t_clockSlot;

// チップ名
const std::vector<String> CHIP_LABEL = {"",       "SN76489", "SN76489", "YM2413", "YM2612", "YM2151", "YM2203",
                                        "YM2203", "YM2608",  "YM2610",  "YM3526", "YM3812", "AY8910", "YMF262"};

// デバイス定義
typedef enum {
  YM2413,
  YMF262,
  YM2203_FM0,
  YM2203_FM1,
  YM2203_SSG0,
  YM2203_SSG1,
  DEVICE_COUNT  // デバイスの総数
} t_device;

#define MAX_CHANNELS 18

// デバイスごとのチャンネル数
static const int device_channels[DEVICE_COUNT] = {
    [YM2413] = 9, [YMF262] = 18, [YM2203_FM0] = 3, [YM2203_FM1] = 3, [YM2203_SSG0] = 3, [YM2203_SSG1] = 3};

// 音階情報
struct NoteInfo {
  int octave;  // 1〜8, これ以外はキーオフ扱い
  int note;    // 0=C, 1=C#, 2=D, 3=D#, 4=E, ..., 9=A, 10=A#, 11=B
};

// 周波数から音階に変換
inline NoteInfo freqToNote(double freq) {
  if (freq <= 0) return {0, 0};

  // A4 = 440Hz を基準
  double n = 12.0 * log2(freq / 440.0);
  // A=9 に合わせる
  int noteIndexFromC0 = (int)round(n) + 57;

  if (noteIndexFromC0 < 12) {  // 音域外, C1未満
    return {0, 0};
  }

  int octave = noteIndexFromC0 / 12;
  int note = noteIndexFromC0 % 12;

  return {octave, note};
}

// 秩父別キー状態セマフォ
extern SemaphoreHandle_t keyInfoMutex;

class VGM {
 public:
  t_format format;

  u32_t version;     // VGM バージョン
  u32_t dataOffset;  // データオフセット
  u32_t loopOffset;  // ループオフセット
  u32_t gd3Offset;   // gd3オフセット
  // u32_t totalSamples;  // 全サンプル数
  boolean SN76489_Freq0is0X400;  // SN76489 が Sega VDP ではない

  std::vector<si5351Freq_t> freq = {SI5351_UNDEFINED, SI5351_UNDEFINED};

  byte chipSlot[14];
  byte clockSlot[14];  // クロック使用番号

  // VGM データ
  static constexpr byte NUM_CHANNELS = 6;
  static constexpr byte NUM_OCTAVES = 8;
  static constexpr byte NUM_KEYS = 12;

  // チップデバイスチャンネル別キー状態
  struct NoteInfo keyInfo[DEVICE_COUNT][MAX_CHANNELS];

  bool vgmLoaded = false;
  bool xgmLoaded = false;

  VGM();
  bool ready();  // VGM の再生準備
  void vgmProcess();
  void vgmProcessMain();

  void resetKeyInfo();

#ifdef USE_XGM
  u8_t XGMVersion;  // XGM バージョン 1 or 2
  std::vector<u32_t> XGMSampleAddressTable;
  std::vector<u32_t> XGMSampleSizeTable;

  u32_t XGM_SLEN;
  u32_t XGM_MLEN;
  u8_t XGM_FLAGS;
  u32_t XGM_FMLEN;   // XGM2
  u32_t XGM_PSGLEN;  // XGM2
  bool XGMReady();   // XGM の再生準備
  void xgmProcess();
  void xgm2Process();
#endif
  u64_t getCurrentTimeSec();

 private:
  t_gd3 gd3;

  u16_t _vgmLoop;
  u64_t _vgmSamples;
  u64_t _vgmRealSamples;
  u64_t _vgmStart;
  u64_t _vgmWaitUntil;
  u32_t _pcmpos = 0;
  s64_t micros64();
  String _formatChipName(si5351Freq_t freq, t_chip chip);

  u8_t _dat0;

  u8_t _ym2203_SSG_reg[2][16] = {0};
  u8_t _ym2203_FM_reg[2][7] = {0};
  u8_t _ym2203_FM_prescaler = 4;   // 仕様と異なる
  u8_t _ym2203_SSG_prescaler = 2;  // 仕様と異なる
  u8_t _ym2413_reg[0x38] = {0};
  u8_t _ymf262_reg[2][256] = {0};

  //
  // YM2203 SSG周波数計算
  double _getPSGFreq(byte chip, byte ch) {
    int coarse = _ym2203_SSG_reg[chip][ch * 2 + 1] & 0x0f;
    int fine = _ym2203_SSG_reg[chip][ch * 2 + 0];
    int TP = (coarse << 8) | fine;
    if (TP == 0) return 0;
    return (double)freq[0] * _ym2203_SSG_prescaler / (64.0 * TP);
  }

  // YM2203 FM周波数計算
  double _getFMFreq(byte chip, int channel) {
    // F-number
    uint16_t f_number_high = (_ym2203_FM_reg[chip][4 + channel] & 0x07) << 8;
    uint8_t f_number_low = _ym2203_FM_reg[chip][channel];
    uint16_t f_number = f_number_high | f_number_low;

    // Blockの計算
    uint8_t block = (_ym2203_FM_reg[chip][4 + channel] & 0x38) >> 3;

    // F-Numberが0の場合は周波数を0と見なす
    if (f_number == 0) {
      return 0.0;
    }

    // 周波数計算の式: f_note = F-Number * φM / (144 * 2^(21-Block))
    return (double)f_number * freq[0] * _ym2203_FM_prescaler / (144.0 * pow(2.0, 21.0 - block));
  }

  // YM2413の周波数計算
  double _getYM2413Freq(byte ch) {
    constexpr double F_OSC = 3579545 / 72.0;

    // F-Number 下位8bit
    uint8_t fnum_l = _ym2413_reg[0x10 + ch];
    // F-Number 上位1bit + BLOCK + KEYON
    uint8_t blk_ky = _ym2413_reg[0x20 + ch];

    // 9bitのF-Number
    int F = fnum_l | ((blk_ky & 0x01) << 8);
    // BLOCK (オクターブ)
    int block = (blk_ky >> 1) & 0x07;
    // KEY ONフラグ
    bool keyon = (blk_ky & 0x10) != 0;
    if (keyon == 0) return 0.0;

    // 音色番号
    uint8_t inst = (_ym2413_reg[0x30 + ch] >> 4) & 0x0F;

    return (double)F * (1 << (block - 1)) * F_OSC / (1 << 18);
  }

  // YMF262周波数
  double _getYMF262Freq(int array, int ch) {
    int kon = (_ymf262_reg[array][0xB0 + ch] >> 5) & 0x01;
    if (!kon) return 0.0;

    int fnum = (_ymf262_reg[array][0xA0 + ch] & 0xFF) | ((_ymf262_reg[array][0xB0 + ch] & 0x03) << 8);
    int block = (_ymf262_reg[array][0xB0 + ch] >> 2) & 0x07;
    if (fnum == 0) return 0.0;

    double fs = freq[1] / 288.0;
    // f = (fnum * fs) / 2^19 * 2^block
    double f = (fnum * fs) / (1 << 19);
    return f * (1 << block);
  }

#ifdef USE_XGM
  u32_t _xgmSamplePos[XGM1_MAX_PCM_CH];
  u8_t _xgmSampleId[XGM1_MAX_PCM_CH];
  u8_t _xgmPriorities[XGM1_MAX_PCM_CH];
  bool _xgmSampleOn[XGM1_MAX_PCM_CH];
  bool _xgmPCMHalfSpeed[3];
  bool _xgmPCMHalfSent[3];
  u32_t _xgmFrame;
  u32_t _xgmYMSNFrame;
  u64_t _xgmStartTick;
  u64_t _xgmWaitUntil;
  u64_t _xgmWaitYMUntil;
  u64_t _xgmWaitPsgUntil;
  bool _xgmIsNTSC;

  u8_t _xgmYmState[2][0x100];
  s16_t _xgmPsgState[2][4];
  u32_t _xgmYMFrame, _xgmPSGFrame;

  u32_t _xgm2_ym_offset;
  u32_t _xgm2_ym_pos;
  u32_t _xgm2_psg_offset;
  u32_t _xgm2_psg_pos;
#endif

  si5351Freq_t normalizeFreq(u32_t freq, t_chip chip);

  u32_t _gd3p;
  void _parseGD3(u32_t pos);
  String _digGD3();
  void _resetGD3();

  // when reach the end of the song
  void endProcedure();

  // xgm1
  bool _xgm1ProcessYMSN();
  void _xgm1ProcessPCM();

  // xgm2
  bool _xgm2ProcessYM();
  bool _xgm2ProcessSN();
  void _xgm2ProcessPCM();

  int _getYMPort(u32_t pos);
  int _getYMChannel(u32_t pos);
  int _getYMSlot(u8_t command);
  u8_t _getChannel(u32_t pos);
};

extern VGM vgm;

#endif
