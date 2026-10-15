#include "input.h"

#include "disp.h"
#include "file.h"

namespace {
TaskHandle_t adcTaskHandle = nullptr;
TaskHandle_t eventTaskHandle = nullptr;
TaskHandle_t inputHandlerTaskHandle = nullptr;
QueueHandle_t buttonQueue = nullptr;
QueueHandle_t inputEventQueue = nullptr;

void adcTask(void*) {
  while (true) {
    if (input.isEnabled()) {
      const Button button = input.checkButton();
      if (button != btnNONE) xQueueOverwrite(buttonQueue, &button);
    }
    vTaskDelay(pdMS_TO_TICKS(INPUT_CAPTURE_INTERVAL));
  }
}

// ND6 と同様、現在の画面にイベントを配送する。
void eventTask(void*) {
  event ev;
  while (true) {
    if (xQueueReceive(inputEventQueue, &ev, portMAX_DELAY) != pdTRUE) continue;
    if (!input.isEnabled()) continue;

    switch (disp.currentView) {
      case ViewMode::Config:
        cfgWindow.eventHandler(ev);
        break;
      case ViewMode::Visual:
        visualWindow.eventHandler(ev);
        break;
      default:
        disp.eventHandler(ev);
        break;
    }
  }
}

void inputHandlerTask(void*) {
  while (true) {
    input.inputHandler();
    vTaskDelay(1);
  }
}

void sendEvent(event ev) {
  if (inputEventQueue != nullptr) xQueueOverwrite(inputEventQueue, &ev);
}

void releaseInputResources() {
  if (adcTaskHandle != nullptr) vTaskDelete(adcTaskHandle);
  if (inputHandlerTaskHandle != nullptr) vTaskDelete(inputHandlerTaskHandle);
  if (eventTaskHandle != nullptr) vTaskDelete(eventTaskHandle);
  adcTaskHandle = inputHandlerTaskHandle = eventTaskHandle = nullptr;
  if (buttonQueue != nullptr) vQueueDelete(buttonQueue);
  if (inputEventQueue != nullptr) vQueueDelete(inputEventQueue);
  buttonQueue = inputEventQueue = nullptr;
}
}  // namespace

bool Input::init() {
  if (adcTaskHandle != nullptr) return true;
  pinMode(INPUT_PIN, ANALOG);

  // ボタンの受け渡しもキューにし、読み取りとクリアの競合を避ける。
  buttonQueue = xQueueCreate(1, sizeof(Button));
  inputEventQueue = xQueueCreate(1, sizeof(event));
  if (buttonQueue == nullptr || inputEventQueue == nullptr) {
    releaseInputResources();
    return false;
  }

  if (xTaskCreatePinnedToCore(eventTask, "inputEvent", 8192, nullptr, 1, &eventTaskHandle, PRO_CPU_NUM) != pdPASS ||
      xTaskCreatePinnedToCore(inputHandlerTask, "inputHandler", 4096, nullptr, 1, &inputHandlerTaskHandle,
                              PRO_CPU_NUM) != pdPASS ||
      xTaskCreatePinnedToCore(adcTask, "adcInput", 4096, nullptr, 1, &adcTaskHandle, PRO_CPU_NUM) != pdPASS) {
    releaseInputResources();
    return false;
  }
  return true;
}

void Input::inputHandler() {
  if (!_enabled) return;
  Button button;
  if (xQueueReceive(buttonQueue, &button, 0) != pdTRUE) return;

  if (disp.currentView == ViewMode::Config) {
    switch (button) {
      case btnUP: sendEvent(event::Up); break;
      case btnDOWN: sendEvent(event::Down); break;
      case btnLEFT: sendEvent(event::Left); break;
      case btnRIGHT: sendEvent(event::Right); break;
      case btnSELECT: sendEvent(event::Close); break;
      default: break;
    }
  } else if (ndConfig.currentMode == MODE_SERIAL) {
    // The host owns clocks and registers; local buttons only navigate the UI.
    if (button == btnSELECT) {
      sendEvent(disp.currentView == ViewMode::Visual ? event::Close : event::Option);
    }
  } else if (disp.currentView == ViewMode::Visual || disp.currentView == ViewMode::Player) {
    switch (button) {
      case btnUP: ndFile.requestDirPlay(1); break;
      case btnDOWN: ndFile.requestDirPlay(-1); break;
      case btnLEFT: ndFile.requestFilePlay(1); break;
      case btnRIGHT: ndFile.requestFilePlay(-1); break;
      case btnSELECT:
        sendEvent(disp.currentView == ViewMode::Visual ? event::Close : event::Option);
        break;
      default: break;
    }
  }
}

void Input::setEnabled(bool state) {
  _enabled = state;
  if (!state) {
    if (buttonQueue != nullptr) xQueueReset(buttonQueue);
    if (inputEventQueue != nullptr) xQueueReset(inputEventQueue);
    ndFile.clearPlaybackQueue();
  }
}

bool Input::isEnabled() const { return _enabled; }

// ND7 の抵抗分圧値は維持する。
Button Input::_readButton() {
  const uint16_t in = analogRead(INPUT_PIN);
  if (in > VAL_NONE - 100) return btnNONE;
  if (in < VAL_0 + 100) return btnSELECT;
  if (VAL_1 - 80 <= in && in < VAL_1 + 80) return btnRIGHT;
  if (VAL_2 - 100 <= in && in < VAL_2 + 100) return btnLEFT;
  if (VAL_3 - 120 <= in && in < VAL_3 + 120) return btnDOWN;
  if (VAL_4 - 180 <= in && in < VAL_4 + 180) return btnUP;
  return btnNONE;
}

Button Input::checkButton() {
  const uint32_t ms = millis();
  const Button button = _readButton();
  if (button == btnNONE) {
    if (_lastButton == btnUP || _lastButton == btnDOWN || _lastButton == btnLEFT || _lastButton == btnRIGHT) {
      ndFile.clearPlaybackQueue();
    }
    _lastButton = btnNONE;
    _buttonRepeatStarted = 0;
    return btnNONE;
  }
  if (_lastButton != button) {
    _lastButton = button;
    _buttonRepeatStarted = 0;
    return btnNONE;
  }
  if (_buttonRepeatStarted == 0) {
    _buttonRepeatStarted = ms;
    return button;
  }
  if (button == btnSELECT || button == btnFUNC || ms - _buttonRepeatStarted < INPUT_REPEAT_DELAY) {
    return btnNONE;
  }
  return button;
}

Input input;
