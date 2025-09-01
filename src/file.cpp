#include "file.h"

static SPIClass SPI_SD;
std::vector<String> dirs;                // ルートのディレクトリ一覧
std::vector<String> pngs;                // ディレクトリごとのpng
std::vector<std::vector<String>> files;  // 各ディレクトリ内のファイル一覧
static File _vgmFile;

static SemaphoreHandle_t spFileOpen;  // ファイル開く処理用セマフォ

//-------------------------------------------------------------------------

bool NDFile::init() {
  currentDir = 0;
  currentFile = 0;

  _att = 0;
  _attFM = 0;
  _attSSG = 0;

  // セマフォ作成
  spFileOpen = xSemaphoreCreateBinary();
  xSemaphoreGive(spFileOpen);

  // SD用 SPI開始
  SPI_SD.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);

  int n = 0;
  lcd.print("Checking SD card");
  while (!SD.begin(SD_CS, SPI_SD, 80000000)) {  // SD マウント試行 @ 80MHz
    vTaskDelay(200);
    n++;
    lcd.print(".");
    if (n == 10) {
      lcd.print("\n\nERROR: SD card open failed.\n");
      return false;
    }
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    lcd.println("\n\nERROR: No SD card attached.\n");
    return false;
  }

  lcd.print("\nSD card detected.");
  lcd.print("\n- Type: ");
  if (cardType == CARD_MMC) {
    lcd.println("MMC");
  } else if (cardType == CARD_SD) {
    lcd.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    lcd.println("SDHC");
  } else {
    lcd.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  lcd.printf("- Size: %llu MB\n", cardSize);
  vTaskDelay(100);

  // メモリ確保
  psramInit();  // ALWAYS CALL THIS BEFORE USING THE PSRAM
  data = (u8_t *)ps_calloc(MAX_FILE_SIZE, sizeof(u8_t));

  return true;
}

uint16_t NDFile::getNumFilesinCurrentDir() { return files[currentDir].size(); }

void NDFile::listDir(const char *dirname) {
  File root = SD.open(dirname);
  if (!root) {
    lcd.println("Error: SD card open failed.");
    return;
  }

  String dirName;
  String filename;
  bool isDir;

  lcd.println("\nReading files...");

  // ディレクトリ取得
  dirName = root.getNextFileName(&isDir);
  while (dirName != "") {
    if (isDir) {
      // システム・不可視ディレクトリ除外
      String baseDir = dirName.substring(dirName.lastIndexOf("/") + 1);

      if (baseDir != "System Volume Information" && baseDir != "__MACOSX" && !baseDir.startsWith(".")) {
        // ディレクトリ内の有効ファイルチェック
        File dir = SD.open(dirName);
        int validFileCount = 0;
        while (1) {
          filename = dir.getNextFileName(&isDir);
          if (filename == "") break;
          if (isDir) break;

          String baseFile = filename.substring(filename.lastIndexOf("/") + 1);
          String ext = filename.substring(filename.length() - 4);
          if (ext.equalsIgnoreCase(".vgm")) {
            validFileCount++;
          }
#ifdef USE_XGM
          else if (ext.equalsIgnoreCase(".xgm")) {
            validFileCount++;
          }
#endif
        }
        dir.close();

        if (validFileCount > 0) {
          dirs.push_back(dirName);
          pngs.push_back("");
        }
      }
    }
    dirName = root.getNextFileName(&isDir);
  }
  root.close();

  // 各ディレクトリ内のファイル名取得
  files.resize(dirs.size());

  u16_t x = lcd.getCursorX(), y = lcd.getCursorY();
  for (int i = 0; i < dirs.size(); i++) {
    File dir = SD.open(dirs[i]);
    lcd.setCursor(x, y);
    lcd.printf("%s                                                                                         ",
               dirs[i].c_str());

    while (1) {
      filename = dir.getNextFileName(&isDir);
      if (filename == "") break;
      if (!isDir) {
        String ext = filename.substring(filename.length() - 4);
        if (ext.equalsIgnoreCase(".vgm")) {
          totalSongs++;
          files[i].push_back(filename.substring(dirs[i].length() + 1));
        }
#ifdef USE_XGM
        else if (ext.equalsIgnoreCase(".xgm")) {
          totalSongs++;
          files[i].push_back(filename.substring(dirs[i].length() + 1));
        }
#endif
        else if (ext == ".png") {
          pngs[i] = filename.substring(dirs[i].length() + 1);
        }
      }
    }
    dir.close();
  }

  lcd.setCursor(0, y);
  return;
}

//----------------------------------------------------------------------
// ファイル開いてPSRAMに配置
bool NDFile::readFile(String path) {
  int n = 0;

  _vgmFile = SD.open(path.c_str());
  if (!_vgmFile) {
    lcd.printf("ERROR: Failed to open file.\n%s", path.c_str());
    _vgmFile.close();
    return false;
  }

  if (_vgmFile.size() > MAX_FILE_SIZE) {
    lcd.printf("ERROR: The file is too large.\nMax file size is %d.\n%s", MAX_FILE_SIZE, path.c_str());
    _vgmFile.close();
    return false;
  }

  _vgmFile.read(data, _vgmFile.size());
  Serial.printf("File name: %s\n", path.c_str());
  _vgmFile.close();

  return true;
}

//----------------------------------------------------------------------
// ディレクトリ内の count 個あとの曲再生。マイナスは前の曲
// 戻り値: 成功/不成功
bool NDFile::filePlay(int count) {
  currentFile = mod(currentFile + count, files[currentDir].size());
  return fileOpen(currentDir, currentFile);
}

//----------------------------------------------------------------------
// count 個あとのディレクトリを開いて最初のファイルを再生。
// マイナスは前のディレクトリ
// 戻り値: 成功/不成功
bool NDFile::dirPlay(int count) {
  currentFile = 0;
  currentDir = mod(currentDir + count, dirs.size());
  return fileOpen(currentDir, currentFile);
}

//----------------------------------------------------------------------
// 直接ファイル再生
// 戻り値: 成功/不成功
bool NDFile::play(uint16_t d, uint16_t f) {
  currentFile = f;
  currentDir = d;
  return fileOpen(currentDir, currentFile);
}

//----------------------------------------------------------------------
// ディレクトリ番号＋ファイル番号でファイルを開く
// 戻り値: 成功/不成功

bool NDFile::fileOpen(uint16_t d, uint16_t f) {
  nju72341.mute();
  nju72341.resetFadeout();
  ndConfig.saveHistory();
  ndFile.getAttValueInDir(dirs[d]);

  Serial.printf("Folder attenuation : %d dB\n", _att);
  Serial.printf("YM2203 FM attenuation : %d dB\n", _attFM);
  Serial.printf("YM2203 SSG attenuation : %d dB\n", _attSSG);

  if (xSemaphoreTake(spFileOpen, 0) != pdTRUE) {
    Serial.printf("Semapho is already taken.\n");
    return false;
  }

  FM.reset();

  String st = dirs[d] + "/" + files[d][f];

  bool result = false;

  if (readFile(st)) {
    // check file type
    String ext = st.substring(st.length() - 4);
    if (ext.equalsIgnoreCase(".vgm")) {
      result = vgm.ready();
    }
#ifdef USE_XGM
    else if (ext.equalsIgnoreCase(".xgm")) {
      result = vgm.XGMReady();
    }
#endif
  }

  nju72341.setVolume_1B(_attFM);
  nju72341.setVolume_2B(_attSSG);

  nju72341.reset(_att);
  xSemaphoreGive(spFileOpen);
  nju72341.unmute();

  return result;
}

//----------------------------------------------------------------------
// 指定されたディレクトリ内の減衰指定 "att*" ファイルを調べて値を返す
// 戻り値: 0 ~ 24
void NDFile::getAttValueInDir(const String &dirPath) {
  bool isDir;
  _att = 0;
  _attFM = 0;
  _attSSG = 0;
  bool foundPC98 = false;

  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  while (1) {
    String filePath = dir.getNextFileName(&isDir);
    if (filePath == "") break;

    if (!isDir) {
      String fileName = filePath.substring(filePath.lastIndexOf("/") + 1);
      if (fileName.startsWith("att")) {
        String numberPart = fileName.substring(3);
        uint8_t value = numberPart.toInt();
        if (value > 0 && value <= 24) {
          _att = value;
        }
      } else if (fileName.startsWith("fm_att")) {
        String numberPart = fileName.substring(6);
        uint8_t value = numberPart.toInt();
        if (value > 0 && value <= 24) {
          _attFM = value;
        }
      } else if (fileName.startsWith("ssg_att")) {
        String numberPart = fileName.substring(7);
        uint8_t value = numberPart.toInt();
        if (value > 0 && value <= 24) {
          _attSSG = value;
        }
      } else if (fileName == "pc98") {
        foundPC98 = true;
      }
    }
  }
  dir.close();

  // 98モード
  if (foundPC98) {
    _attFM = 0;
    _attSSG = 3;  // SSG 3db下げ
  }
  return;
}

// data access
// 8 bit 返す
u8_t NDFile::get_ui8() { return data[pos++]; }
// 16 bit 返す
u16_t NDFile::get_ui16() { return get_ui8() + (get_ui8() << 8); }
// 24 bit 返す
u32_t NDFile::get_ui24() { return get_ui8() + (get_ui8() << 16); }
// 32 bit 返す
u32_t NDFile::get_ui32() { return get_ui8() + (get_ui8() << 8) + (get_ui8() << 16) + (get_ui8() << 24); }
// 指定場所の 8 bit 返す
u8_t NDFile::get_ui8_at(uint32_t p) { return data[p]; }
// 指定場所の 16 bit 返す
u16_t NDFile::get_ui16_at(uint32_t p) { return (u32_t(data[p])) + (u32_t(data[p + 1]) << 8); }
// 指定場所の 24 bit 返す
u32_t NDFile::get_ui24_at(uint32_t p) {
  return (u32_t(data[p])) + (u32_t(data[p + 1]) << 8) + (u32_t(data[p + 2]) << 16);
}
// 指定場所の 32 bit 返す
u32_t NDFile::get_ui32_at(uint32_t p) {
  return (u32_t(data[p])) + (u32_t(data[p + 1]) << 8) + (u32_t(data[p + 2]) << 16) + (u32_t(data[p + 3]) << 24);
}

NDFile ndFile = NDFile();

int mod(int i, int j) { return (i % j) < 0 ? (i % j) + 0 + (j < 0 ? -j : j) : (i % j + 0); }
