// Real parser, receiver, board callbacks and register I/O mapping, with fake hardware.
#include "hardware.h"
#include "ndsif.h"
#define private public
#include "serialman.h"
#undef private
#include "register_methods.inc"
#include "../../src/serialman.cpp"

unsigned testFailures, footerRequests, resetCount, keyResets, keyRefreshes, writeCount, clockChanges;
RegisterWrite registerWrites[8192];
uint32_t clockHz[2];
MockSerial Serial;
MockDisp disp;
MockVolume nju72341;
MockFM FM;
SI5351_cls SI5351;
VGM vgm;
SemaphoreHandle_t keyInfoMutex;
namespace {
uint8_t heap[8192];
uint32_t nowMs;
uint8_t volumeAttenuation;
}
// The DLL has no CRT startup: provide only the memory primitives it uses.
extern "C" void* __cdecl memset(void* p, int value, size_t n) {
  volatile uint8_t* out = static_cast<volatile uint8_t*>(p);
  for (size_t i = 0; i < n; ++i) out[i] = uint8_t(value);
  return p;
}
extern "C" void* __cdecl memcpy(void* p, const void* q, size_t n) {
  volatile uint8_t* out = static_cast<volatile uint8_t*>(p);
  const volatile uint8_t* in = static_cast<const volatile uint8_t*>(q);
  for (size_t i = 0; i < n; ++i) out[i] = in[i];
  return p;
}
void* heap_caps_malloc(size_t size, int) { return size <= sizeof(heap) ? heap : nullptr; }
SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void*>(1); }
int xSemaphoreTake(SemaphoreHandle_t, unsigned) { return pdTRUE; }
int xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, unsigned, void*, unsigned, TaskHandle_t* task, int) {
  *task = reinterpret_cast<void*>(1); return pdPASS;
}
void vTaskDelay(unsigned) {}
uint32_t millis() { return nowMs; }
size_t MockSerial::read(uint8_t* out, size_t size) {
  size_t count = size < head - tail ? size : head - tail;
  for (size_t i = 0; i < count; ++i) out[i] = rx[tail++ % sizeof(rx)];
  return count;
}
size_t MockSerial::write(const uint8_t* in, size_t size) {
  if (size > txSpace) size = txSpace;
  if (size > sizeof(tx) - txSize) size = sizeof(tx) - txSize;
  for (size_t i = 0; i < size; ++i) tx[txSize++] = in[i];
  return size;
}
void MockFM::setRegister(uint8_t address, uint8_t value, int slot) {
  if (writeCount < 8192) registerWrites[writeCount++] = {uint8_t(slot), 0, address, value};
}
void MockFM::setRegisterOPL3(uint8_t page, uint8_t address, uint8_t value, int slot) {
  if (writeCount < 8192) registerWrites[writeCount++] = {uint8_t(slot), page, address, value};
}
SI5351_cls::SI5351_cls() {}
void SI5351_cls::setFreq(si5351Freq_t hz, uint8_t slot) { ++clockChanges; clockHz[slot] = uint32_t(hz); }
void serialModeUpdateFooter() { ++footerRequests; }
void MockVolume::reset(uint8_t a) { volumeAttenuation = a; }
void MockVolume::unmute() { attenuation = volumeAttenuation; }

extern "C" {
__declspec(dllexport) void init() {
  serialMan = SerialMan{};
  memset(&Serial, 0, sizeof(Serial));
  memset(&vgm, 0, sizeof(vgm));
  Serial.connected = Serial.plugged = true;
  Serial.txSpace = 512;
  nowMs = 1;
  pendingBusResets = 0;
  testFailures = footerRequests = resetCount = keyResets = keyRefreshes = writeCount = clockChanges = 0;
  serialMan.beginUsb();
  serialMan.init();
  writeCount = resetCount = clockChanges = footerRequests = 0;
}
__declspec(dllexport) void feed(const uint8_t* bytes, unsigned count, unsigned now) {
  nowMs = now;
  for (unsigned i = 0; i < count; ++i) Serial.rx[Serial.head++ % sizeof(Serial.rx)] = bytes[i];
}
__declspec(dllexport) void pump(unsigned count) {
  for (unsigned i = 0; i < count; ++i) {
    serialMan.receive(); serialMan.processCommands(); serialMan.serviceUsbTx();
  }
}
__declspec(dllexport) unsigned output(uint8_t* out) {
  unsigned n = Serial.txSize;
  for (unsigned i = 0; i < n; ++i) out[i] = Serial.tx[i];
  Serial.txSize = 0; return n;
}
__declspec(dllexport) unsigned metric(unsigned index) {
  auto status = serialMan.receiveStatus();
  switch (index) {
    case 0: return resetCount;
    case 1: return writeCount;
    case 2: return clockChanges;
    case 3: return clockHz[0];
    case 4: return clockHz[1];
    case 5: return nju72341.attenuation;
    case 6: return status.overflows;
    case 7: return status.bufferedBytes;
    case 8: return status.discardedBytes;
    case 9: return testFailures;
    case 10: return footerRequests;
    default: return keyRefreshes;
  }
}
__declspec(dllexport) unsigned write_at(unsigned i) {
  const auto& w = registerWrites[i];
  return unsigned(w.slot) | unsigned(w.page) << 8 | unsigned(w.address) << 16 | unsigned(w.value) << 24;
}
__declspec(dllexport) void connection(unsigned plugged, unsigned connected) {
  Serial.plugged = plugged != 0; Serial.connected = connected != 0;
}
__declspec(dllexport) void tx_space(unsigned n) { Serial.txSpace = n; }
__declspec(dllexport) void bus_reset() { usbBusReset(nullptr, nullptr, 0, nullptr); }
__declspec(dllexport) void discard() { serialMan.clearReceiveBuffer(); }
__declspec(dllexport) void legacy() {
  // Keep ND8's original configure() API functional for future upstream merges.
  serialMan.protocol.configure(resetChips, [](uint8_t a, uint8_t v) { FM.setRegister(a, v, 0); }, "test");
  serialMan.protocol.setDeviceInfo("NanoDrive 8", 4096, 1024);
}
}
