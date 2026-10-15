#ifndef SERIALMAN_H
#define SERIALMAN_H

#include <Arduino.h>
#include <freertos/semphr.h>
#include "ndsif.h"

class SerialMan {
 public:
  // USB の RX キューは Serial.begin() より前に確保する。
  static constexpr size_t USB_RX_SIZE = 8192;
  static constexpr size_t USB_TX_SIZE = 512;
  static constexpr size_t RECEIVE_SIZE = 8192;

  struct ReceiveStatus {
    bool usbLinkActive = false;  // SOF の有無。演奏アプリの接続状態ではない。
    bool dataLost = false;       // このバッファでの欠落。USB ドライバ内部の欠落は別。
    size_t bufferedBytes = 0;
    uint64_t receivedBytes = 0;
    uint64_t discardedBytes = 0;
    uint32_t connections = 0;
    uint32_t disconnections = 0;
    uint32_t busResets = 0;
    uint32_t overflows = 0;
    uint32_t lastReceiveMs = 0;
  };

  void beginUsb();
  void init();
  void startSerialTask();
  // タスク専用。read() の消費者は NDSIF 受信タスクのみ。
  size_t read(uint8_t* data, size_t size);
  ReceiveStatus receiveStatus();
  uint32_t chipClock(uint8_t slot) const;  // Physical clock snapshot for the UI.
  void clearReceiveBuffer();  // 未読データを破棄し、次の区切りで再同期。

 private:
  static void serialTask(void* arg);
  void receive();
  void processCommands();
  void serviceUsbTx();
  void resetReceiveLocked(uint32_t reason);
  SemaphoreHandle_t receiveMutex = nullptr;
  TaskHandle_t receiveTask = nullptr;
  uint8_t* receiveBuffer = nullptr;
  size_t receiveHead = 0;
  size_t receiveTail = 0;
  size_t usbDiscardRemaining = 0;
  int usbTxIdleSpace = 0;
  ReceiveStatus status;
  bool protocolResetPending = false;
  uint32_t transportLossPending = 0;
  ndsif::Protocol protocol;
};

extern SerialMan serialMan;

#endif
