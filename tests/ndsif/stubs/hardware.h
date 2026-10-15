#pragma once
#include <stdint.h>
#include <stddef.h>
#include <atomic>
#include <string.h>
using byte = uint8_t;
using u8_t = uint8_t;
using u16_t = uint16_t;
using u32_t = uint32_t;
#include "../../../lib/SI5351/SI5351.hpp"
using SemaphoreHandle_t = void*;
using TaskHandle_t = void*;
using BaseType_t = int;
using esp_event_base_t = const char*;
using EventHandler = void (*)(void*, esp_event_base_t, int32_t, void*);
constexpr int pdPASS = 1, pdTRUE = 1, APP_CPU_NUM = 1;
constexpr unsigned portMAX_DELAY = ~0u;
constexpr int MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2;
constexpr int ARDUINO_HW_CDC_BUS_RESET_EVENT = 1;
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
extern unsigned testFailures;
#define configASSERT(e) do { if (!(e)) ++testFailures; } while (0)
void* heap_caps_malloc(size_t, int);
SemaphoreHandle_t xSemaphoreCreateMutex();
int xSemaphoreTake(SemaphoreHandle_t, unsigned);
int xSemaphoreGive(SemaphoreHandle_t);
BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, unsigned, void*, unsigned, TaskHandle_t*, int);
void vTaskDelay(unsigned);
uint32_t millis();
constexpr int USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY = 1;
inline bool usb_serial_jtag_ll_txfifo_writable() { return false; }
inline void usb_serial_jtag_ll_ena_intr_mask(int) {}
inline void usb_serial_jtag_ll_txfifo_flush() {}

class MockSerial {
 public:
  uint8_t rx[32768], tx[32768];
  unsigned head, tail, txSize, txSpace;
  bool plugged, connected;
  size_t setRxBufferSize(size_t n) { return n; }
  size_t setTxBufferSize(size_t n) { return n; }
  void begin(int) {}
  int availableForWrite() { return int(txSpace); }
  bool isPlugged() { return plugged; }
  bool isConnected() { return connected; }
  int available() { return int(head - tail); }
  size_t read(uint8_t*, size_t);
  size_t write(const uint8_t*, size_t);
  void onEvent(int, EventHandler) {}
};
extern MockSerial Serial;
enum class ViewMode { Player, Config, Serial, Visual };
class MockDisp {
 public:
  ViewMode currentView;
  void serialModeDraw() {}
};
extern MockDisp disp;
extern unsigned footerRequests, resetCount, keyResets, keyRefreshes;
void serialModeUpdateFooter();
class MockVolume {
 public:
  uint8_t attenuation;
  void mute() { attenuation = 96; }
  void unmute();
  void reset(uint8_t);
  void setVolumeAll(uint8_t a) { attenuation = a; }
  void setVolume_3B_4B(uint8_t a) { attenuation = a; }
};
extern MockVolume nju72341;
struct RegisterWrite { uint8_t slot, page, address, value; };
extern RegisterWrite registerWrites[8192];
extern unsigned writeCount, clockChanges;
extern uint32_t clockHz[2];
class MockFM {
 public:
  void reset() { ++resetCount; }
  void setRegister(uint8_t address, uint8_t value, int slot);
  void setRegisterOPL3(uint8_t page, uint8_t address, uint8_t value, int slot);
};
extern MockFM FM;
enum { FORMAT_UNKNOWN, DEVICE_COUNT = 6, MAX_CHANNELS = 18 };
struct NoteInfo { int octave, note; };
extern SemaphoreHandle_t keyInfoMutex;
class VGM {
 public:
  bool vgmLoaded, xgmLoaded;
  int format;
  si5351Freq_t freq[2];
  NoteInfo keyInfo[DEVICE_COUNT][MAX_CHANNELS];
  uint8_t _ym2203_SSG_reg[2][16], _ym2203_FM_reg[2][7], _ym2413_reg[0x40], _ymf262_reg[2][256];
  bool _serialFMKeyOn[2][3];
  void resetSerialState();
  void writeSerialRegister(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
  void refreshSerialKeyInfo() { ++keyRefreshes; }
};
extern VGM vgm;
