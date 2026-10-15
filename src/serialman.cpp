#include "serialman.h"

#include <atomic>
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include <hal/usb_serial_jtag_ll.h>

#include "NJU72341.h"
#include "SI5351.hpp"
#include "common.h"
#include "disp.h"
#include "fm.h"
#include "vgm.h"

namespace {
portMUX_TYPE usbEventMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t pendingBusResets = 0;
enum TransportLoss : uint32_t { BusReset = 1, ReceiveOverflow = 2, ExplicitDiscard = 4 };
std::atomic<uint32_t> serialClocks[2]{{4000000}, {14318180}};
uint8_t outputAttenuation = 0;
bool opl3Enabled = false;

// Only immediate register BURST and variable-clock features are advertised.
// The board has no PCM stream or position-based register-event queues.
const ndsif::Capability serialCapabilities[] = {
    {0x55, 0, 0, 5, ndsif::CLOCK_VARIABLE, 0},
    {0x55, 1, 0, 5, ndsif::CLOCK_VARIABLE, 0},
    {0xa5, 1, 0, 5, ndsif::CLOCK_VARIABLE, 0},
    {0x51, 0, 0, 1, ND_YM2413_CLOCK_HZ ? ndsif::CLOCK_FIXED : ndsif::CLOCK_UNKNOWN, ND_YM2413_CLOCK_HZ},
    {0x5e, 0, 0, 5, ndsif::CLOCK_VARIABLE, 0},
    {0x5f, 0, 1, 5, ndsif::CLOCK_VARIABLE, 0},
    {0xa0, 0, 0, 5, ndsif::CLOCK_VARIABLE, 0},
    {0xa0, 1, 0, 5, ndsif::CLOCK_VARIABLE, 0},
    {0x5a, 0, 0, 5, ndsif::CLOCK_VARIABLE, 0},
    {0x5b, 0, 0, 5, ndsif::CLOCK_VARIABLE, 0},
};

void usbBusReset(void*, esp_event_base_t, int32_t, void*) {
  // USB event callbacks only signal the receiving task.
  portENTER_CRITICAL(&usbEventMux);
  ++pendingBusResets;
  portEXIT_CRITICAL(&usbEventMux);
}

void resetChips() {
  nju72341.mute();
  FM.reset();
  vgm.resetSerialState();
  opl3Enabled = false;
  // Explicitly disable OPL3 NEW after reset, matching the OPL1/2 routes.
  FM.setRegisterOPL3(1, 0x05, 0, 2);
  nju72341.reset(outputAttenuation);
  nju72341.unmute();
}

bool validTarget(uint8_t id, uint8_t instance, uint8_t page) {
  switch (id) {
    case 0x55: case 0xa0: return instance <= 1 && page == 0;
    case 0xa5: return instance == 1 && page == 0;
    case 0x51: case 0x5a: case 0x5b: case 0x5e: return instance == 0 && page == 0;
    case 0x5f: return instance == 0 && page == 1;
    default: return false;
  }
}

void selectOpl2Mode() {
  if (!opl3Enabled) return;
  vgm.writeSerialRegister(0x5f, 0, 1, 0x05, 0);
  opl3Enabled = false;
}

void writeRegisters(uint8_t id, uint8_t instance, uint8_t page, const uint8_t* pairs, size_t count) {
  if (!validTarget(id, instance, page) || pairs == nullptr || count == 0 || count > 127) return;
  // Validate ALL addresses before any I/O (including a compatibility-mode change).
  if (id == 0xa0 || id == 0x51) {
    const uint8_t maxAddress = id == 0xa0 ? 0x0f : 0x3f;
    for (size_t i = 0; i < count; ++i) if (pairs[2 * i] > maxAddress) return;
  }
  if (id == 0x5a || id == 0x5b) selectOpl2Mode();
  for (size_t i = 0; i < count; ++i) {
    const uint8_t address = pairs[2 * i], value = pairs[2 * i + 1];
    vgm.writeSerialRegister(id, instance, page, address, value);
    if (id == 0x5f && address == 0x05) opl3Enabled = (value & 1) != 0;
  }
  vgm.refreshSerialKeyInfo();
}

void setOutputVolume(uint8_t attenuation) {
  outputAttenuation = attenuation;
  // Channels 3/4 are the ND7 main stereo output. Keep FM/SSG trims at zero.
  nju72341.reset(attenuation);
  nju72341.setVolume_3B_4B(attenuation);
}

si5351Freq_t normalizePhysicalClock(uint8_t slot, uint32_t hz) {
  // Whitelisted physical clocks, using the existing ND7 oscillator tables.
  // Reject arbitrary/flagged Hz instead of using the VGM parser's fallback clock.
  if (hz >= 3579000 && hz <= 3580000) hz = 3579545;
  if (hz >= 14316000 && hz <= 14320000) hz = 14318180;
  if (slot == 0 && hz == 3993600) hz = 4000000;
  const si5351Freq_t opn[] = {SI5351_1022, SI5351_1250, SI5351_1500, SI5351_1536,
      SI5351_1789, SI5351_2000, SI5351_2045, SI5351_2500, SI5351_2578, SI5351_3000,
      SI5351_3072, SI5351_3332, SI5351_3375, SI5351_3500, SI5351_3579, SI5351_4000, SI5351_4500};
  const si5351Freq_t opl[] = {SI5351_8000, SI5351_9000, SI5351_12000, SI5351_14000,
      SI5351_14318, SI5351_16000};
  if (slot == 0) {
    for (auto value : opn) if (uint32_t(value) == hz) return value;
  } else {
    for (auto value : opl) if (uint32_t(value) == hz) return value;
  }
  return SI5351_UNDEFINED;
}

bool setChipClock(uint8_t id, uint8_t instance, uint32_t hz) {
  const uint8_t page = id == 0x5f ? 1 : 0;
  if (!validTarget(id, instance, page) || id == 0x51 || hz == 0) return false;
  const bool opn = id == 0x55 || id == 0xa5 || id == 0xa0;
  const uint8_t slot = opn ? 0 : 1;
  const uint32_t multiplier = id == 0xa0 ? 2 : (id == 0x5a || id == 0x5b ? 4 : 1);
  if (hz > UINT32_MAX / multiplier) return false;
  const si5351Freq_t frequency = normalizePhysicalClock(slot, hz * multiplier);
  if (frequency == SI5351_UNDEFINED) return false;
  if (id == 0x5a || id == 0x5b) selectOpl2Mode();
  if (vgm.freq[slot] != frequency) {
    SI5351.setFreq(frequency, slot);
    vgm.freq[slot] = frequency;
    serialClocks[slot].store(uint32_t(frequency), std::memory_order_relaxed);
    serialModeUpdateFooter();
  }
  vgm.refreshSerialKeyInfo();
  return true;
}
}  // namespace

uint32_t SerialMan::chipClock(uint8_t slot) const {
  return slot < 2 ? serialClocks[slot].load(std::memory_order_relaxed) : 0;
}

void SerialMan::beginUsb() {
  // Resize only before the USB ISR starts. Measure the empty byte-ring's
  // usable space, which may differ from its allocated size.
  const size_t rxSize = Serial.setRxBufferSize(USB_RX_SIZE);
  const size_t txSize = Serial.setTxBufferSize(USB_TX_SIZE);
  configASSERT(rxSize == USB_RX_SIZE && txSize == USB_TX_SIZE);
  Serial.begin(115200);
  usbTxIdleSpace = Serial.availableForWrite();
  configASSERT(usbTxIdleSpace > 0);
}

void SerialMan::serviceUsbTx() {
  // HWCDC clears IN_EMPTY and returns without draining its TX ring when
  // isPlugged() momentarily becomes false. Protocol::consumeOutput() has
  // already handed the reply off, and one-way register BURST will not wake TX.
  // Recover queued bytes even when there is no pending protocol response.
  if (usbTxIdleSpace <= 0 || Serial.availableForWrite() >= usbTxIdleSpace) return;
  // beginUsb() and this task run on APP_CPU_NUM, where the USB ISR lives.
  // Protect the writable check + empty-FIFO flush against that ISR filling
  // the FIFO between them. No ring-buffer operation runs with IRQs disabled.
  portENTER_CRITICAL(&usbEventMux);
  if (usb_serial_jtag_ll_txfifo_writable()) {
    usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
    usb_serial_jtag_ll_txfifo_flush();
  }
  portEXIT_CRITICAL(&usbEventMux);
}

// シリアルモード初期化
void SerialMan::init() {
  receiveMutex = xSemaphoreCreateMutex();
  receiveBuffer = static_cast<uint8_t*>(
      heap_caps_malloc(RECEIVE_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  configASSERT(receiveMutex != nullptr && receiveBuffer != nullptr);
  protocol.configure(resetChips, nullptr, ND_FIRMWARE_VERSION, setChipClock, nullptr, setOutputVolume);
  protocol.setDeviceInfo("NanoDrive 7");
  protocol.setRegisterWriter(writeRegisters);
  const bool capabilitiesReady = protocol.setCapabilities(
      serialCapabilities, sizeof(serialCapabilities) / sizeof(serialCapabilities[0]));
  configASSERT(capabilitiesReady);
  status.usbLinkActive = Serial.isPlugged();
  Serial.onEvent(ARDUINO_HW_CDC_BUS_RESET_EVENT, usbBusReset);

  vgm.vgmLoaded = vgm.xgmLoaded = false;
  vgm.format = FORMAT_UNKNOWN;
  vgm.freq[0] = SI5351_4000;
  vgm.freq[1] = SI5351_14318;
  serialClocks[0].store(SI5351_4000, std::memory_order_relaxed);
  serialClocks[1].store(SI5351_14318, std::memory_order_relaxed);
  SI5351.setFreq(vgm.freq[0], 0);
  SI5351.setFreq(vgm.freq[1], 1);
  outputAttenuation = 0;
  nju72341.setVolumeAll(0);
  resetChips();
  disp.currentView = ViewMode::Serial;
  disp.serialModeDraw();
}

void SerialMan::startSerialTask() {
  if (receiveTask != nullptr) return;
  // Receive and register I/O run on the same core as the dedicated GPIO bundle.
  // Keep the receiver above the normal UI task (priority 1).
  const BaseType_t result = xTaskCreatePinnedToCore(
      serialTask, "serialTask", 4096, this, 2, &receiveTask, APP_CPU_NUM);
  configASSERT(result == pdPASS && receiveTask != nullptr);
}

void SerialMan::serialTask(void* arg) {
  auto& receiver = *static_cast<SerialMan*>(arg);
  while (true) {
    receiver.receive();
    receiver.processCommands();
    receiver.serviceUsbTx();
    vTaskDelay(1);
  }
}

void SerialMan::resetReceiveLocked(uint32_t reason) {
  status.discardedBytes += status.bufferedBytes;
  status.bufferedBytes = 0;
  status.dataLost = false;
  receiveHead = receiveTail = 0;
  protocolResetPending = true;
  transportLossPending |= reason;
}

void SerialMan::receive() {
  uint32_t resets;
  portENTER_CRITICAL(&usbEventMux);
  resets = pendingBusResets;
  pendingBusResets = 0;
  portEXIT_CRITICAL(&usbEventMux);
  const bool plugged = Serial.isPlugged();

  xSemaphoreTake(receiveMutex, portMAX_DELAY);
  const bool linkChanged = plugged != status.usbLinkActive;
  if (linkChanged) {
    if (plugged) ++status.connections;
    else ++status.disconnections;
    status.usbLinkActive = plugged;
  }
  status.busResets += resets;
  // isPlugged() is a SOF observation, not proof of transport data loss.
  // A transient observation must not discard valid queued bytes or reset chips.
  // An explicit USB bus reset still resets chips and resynchronizes RX.
  if (resets != 0) {
    resetReceiveLocked(BusReset);
    const int queued = Serial.available();
    usbDiscardRemaining = queued > 0 ? static_cast<size_t>(queued) : 0;
  }

  uint8_t bytes[64];
  // 1 周の仕事量を制限する。RX 状態・入力処理を長時間占有しない。
  for (unsigned block = 0; block < 8 && Serial.available() > 0; ++block) {
    const size_t request = usbDiscardRemaining != 0 && usbDiscardRemaining < sizeof(bytes)
        ? usbDiscardRemaining : sizeof(bytes);
    const size_t count = Serial.read(bytes, request);
    if (count == 0 || count > sizeof(bytes)) break;
    status.receivedBytes += count;
    status.lastReceiveMs = millis();
    if (usbDiscardRemaining != 0) {
      const size_t discard = count < usbDiscardRemaining ? count : usbDiscardRemaining;
      usbDiscardRemaining -= discard;
      status.discardedBytes += count;
      continue;
    }
    if (status.dataLost) {
      status.discardedBytes += count;
      continue;
    }
    if (count > RECEIVE_SIZE - status.bufferedBytes) {
      // 欠落後の byte 列を正常なフレームとして渡さない。
      ++status.overflows;
      resetReceiveLocked(ReceiveOverflow);
      status.dataLost = true;
      status.discardedBytes += count;
      continue;
    }
    for (size_t i = 0; i < count; ++i) {
      receiveBuffer[receiveHead] = bytes[i];
      receiveHead = (receiveHead + 1) % RECEIVE_SIZE;
    }
    status.bufferedBytes += count;
  }
  xSemaphoreGive(receiveMutex);
}

size_t SerialMan::read(uint8_t* data, size_t size) {
  if (receiveMutex == nullptr || data == nullptr || size == 0) return 0;
  xSemaphoreTake(receiveMutex, portMAX_DELAY);
  const size_t count = status.dataLost ? 0 :
      (size < status.bufferedBytes ? size : status.bufferedBytes);
  for (size_t i = 0; i < count; ++i) {
    data[i] = receiveBuffer[receiveTail];
    receiveTail = (receiveTail + 1) % RECEIVE_SIZE;
  }
  status.bufferedBytes -= count;
  xSemaphoreGive(receiveMutex);
  return count;
}

SerialMan::ReceiveStatus SerialMan::receiveStatus() {
  if (receiveMutex == nullptr) return {};
  xSemaphoreTake(receiveMutex, portMAX_DELAY);
  const ReceiveStatus result = status;
  xSemaphoreGive(receiveMutex);
  return result;
}

void SerialMan::clearReceiveBuffer() {
  if (receiveMutex == nullptr) return;
  xSemaphoreTake(receiveMutex, portMAX_DELAY);
  resetReceiveLocked(ExplicitDiscard);
  xSemaphoreGive(receiveMutex);
}

void SerialMan::processCommands() {
  // Parser / response state belongs exclusively to this task.
  xSemaphoreTake(receiveMutex, portMAX_DELAY);
  const bool reset = protocolResetPending;
  const uint32_t lossReason = transportLossPending;
  protocolResetPending = false;
  transportLossPending = 0;
  // Overflow already discarded the ring in receive(). Resume at a delimiter.
  status.dataLost = false;
  xSemaphoreGive(receiveMutex);
  if (reset) {
    protocol.clear(true);
    if (lossReason != 0) resetChips();
  }
  protocol.expire(millis());

  for (size_t count = 0; count < 512; ++count) {
    if (protocol.outputSize() != 0) {
      // HWCDC isConnected is used only for TX readiness, never as COM-open status.
      if (!Serial.isConnected()) return;
      const int space = Serial.availableForWrite();
      if (space <= 0) return;
      const size_t pending = protocol.outputSize();
      const size_t amount = pending < static_cast<size_t>(space) ? pending : space;
      protocol.consumeOutput(Serial.write(protocol.output(), amount));
      if (protocol.outputSize() != 0) return;
    }
    uint8_t byte;
    if (read(&byte, 1) == 0) return;
    protocol.feed(byte, millis());
  }
}

SerialMan serialMan;
