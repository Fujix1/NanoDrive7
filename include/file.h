#ifndef FILE_H
#define FILE_H

#include <FS.h>
#include <SD.h>

#include "NJU72341.h"
#include "common.h"
#include "disp.h"
#include "fm.h"
#include "vgm.h"

int mod(int i, int j);

class NDFile {
 public:
  bool init();
  void listDir(const char* dirname);
  bool readFile(String path);
  bool filePlay(int count);
  bool dirPlay(int count);
  bool play(u16_t d, u16_t f);
  bool fileOpen(u16_t d, u16_t f);

  void getAttValueInDir(const String& dirPath);  // フォルダの音量減衰取得

  u16_t currentDir;      // 現在のディレクトリ
  u16_t currentFile;     // 現在のファイル
  u16_t totalSongs = 0;  // 合計曲数
  u16_t getNumFilesinCurrentDir();

  u8_t* data;                              // データ本体
  u32_t pos;                               // データ位置
  std::vector<String> dirs;                // ルートのディレクトリ一覧
  std::vector<String> pngs;                // ディレクトリごとのpng
  std::vector<std::vector<String>> files;  // 各ディレクトリ内のファイル一覧

  u8_t get_ui8();
  u16_t get_ui16();
  u32_t get_ui24();
  u32_t get_ui32();
  u8_t get_ui8_at(u32_t p);
  u16_t get_ui16_at(u32_t p);
  u32_t get_ui24_at(u32_t p);
  u32_t get_ui32_at(u32_t p);

  boolean isVGZ;

 private:
  u8_t _att;     // 現在のフォルダの全体減衰量 db
  u8_t _attFM;   // 現在のフォルダの YM2203 FM の減衰量 db
  u8_t _attSSG;  // 現在のフォルダの YM2203 SSG の減衰量 db
};

extern NDFile ndFile;

#endif
