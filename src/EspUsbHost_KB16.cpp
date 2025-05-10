#include <Arduino.h>
#include "../include/EspUsbHost.h"

// KB16キーマトリックス状態管理メソッドの実装
void EspUsbHost::updateKB16KeyState(uint8_t row, uint8_t col, bool pressed) {
  if (row < 4 && col < 4) {
    kb16_key_states[row][col] = pressed;
    ESP_LOGD("KB16", "キーマトリックス状態更新: (%d,%d)=%s", 
            row, col, pressed ? "押下" : "解放");
  } else {
    ESP_LOGW("KB16", "キーマトリックス範囲外: (%d,%d)", row, col);
  }
}

bool EspUsbHost::getKB16KeyState(uint8_t row, uint8_t col) {
  if (row < 4 && col < 4) {
    return kb16_key_states[row][col];
  }
  return false; // 範囲外の場合はfalse
}
