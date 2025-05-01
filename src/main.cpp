#include <Arduino.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <Adafruit_TinyUSB.h>

// BLESecurityCallbacksの実装クラス
class MySecurity : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() {
    // 固定のパスキーを返す（必要に応じて変更可能）
    return 123456;
  }

  void onPassKeyNotify(uint32_t pass_key) {
    // パスキー通知時の処理
    Serial.print("認証パスキー通知: ");
    Serial.println(pass_key);
  }

  bool onConfirmPIN(uint32_t pin) {
    // PINコード確認処理
    Serial.print("PIN確認リクエスト: ");
    Serial.println(pin);
    return true; // 常に承認
  }

  bool onSecurityRequest() {
    // セキュリティリクエスト処理
    Serial.println("セキュリティリクエスト受信");
    return true; // セキュリティリクエストを承認
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
    // 認証完了時の処理
    if(cmpl.success) {
      Serial.println("認証完了: 成功");
    } else {
      Serial.println("認証完了: 失敗");
      Serial.print("認証失敗理由: ");
      Serial.println(cmpl.fail_reason);
    }
  }
};

BleKeyboard bleKeyboard("DOIO_KB16", "SeeedStudio", 100); // デバイス名

// LED指定用の定数（XIAO ESP32S3の内蔵LED）
const int LED_PIN = D0;  // Seeed XIAO ESP32S3の内蔵LED

bool lastConnectedState = false;

// USBキーボード状態管理用変数
uint8_t const* prev_report = NULL;
bool has_keyboard = false;
uint8_t current_addr = 0;
uint8_t keycode_map[6] = {0}; // 最大6キー同時押し対応

void setup() {
  Serial.begin(115200);
  Serial.println("DOIO_KB16 Bluetooth HIDブリッジを起動中...");
  
  // LED出力設定
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // セキュリティパラメータを設定（認証エラー対策）
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
  BLEDevice::setSecurityCallbacks(new MySecurity());
  
  bleKeyboard.begin(); // Bluetooth HID開始
  
  // 追加のセキュリティ設定
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setKeySize(16);
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  
  Serial.println("Bluetooth HIDデバイスを初期化しました。ペアリングを待機しています...");

  // TinyUSB ホスト初期化
  Serial.println("TinyUSB ホスト初期化中...");
  if (!tuh_init(0)) {
    Serial.println("TinyUSB初期化に失敗しました");
  } else {
    Serial.println("TinyUSB初期化完了、USBデバイスを接続してください");
  }
}

void loop() {
  tuh_task(); // TinyUSBホストの処理（必須）

  // 接続状態の変化を検出して表示
  bool isConnected = bleKeyboard.isConnected();
  if (isConnected != lastConnectedState) {
    lastConnectedState = isConnected;
    if (isConnected) {
      Serial.println("Bluetoothデバイスに接続しました！");
      digitalWrite(LED_PIN, HIGH); // 接続されたらLEDを点灯
    } else {
      Serial.println("Bluetoothデバイスが切断されました。再接続を待機中...");
      digitalWrite(LED_PIN, LOW);  // 切断されたらLEDを消灯
    }
  }
  
  // 接続状態のLED表示（接続中は点灯、未接続時はゆっくり点滅）
  if (!isConnected) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(900);
  }

  delay(10); // 軽いディレイでCPU負荷を下げる
}

// キーボードが接続された時の処理
extern "C" void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr) {
  Serial.print("HIDキーボード接続: アドレス = ");
  Serial.println(dev_addr, HEX);
  has_keyboard = true;
  current_addr = dev_addr;
}

// キーボードが切断された時の処理
extern "C" void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr) {
  Serial.print("HIDキーボード切断: アドレス = ");
  Serial.println(dev_addr, HEX);
  has_keyboard = false;
  
  // キーが押されたままの状態で切断された場合にキーを全てリリース
  if (bleKeyboard.isConnected()) {
    bleKeyboard.releaseAll();
  }
}

// キーボード入力を処理する関数
extern "C" void tuh_hid_keyboard_report_received_cb(uint8_t dev_addr, uint8_t const* report, uint16_t len) {
  // 標準HIDキーボードレポート形式を想定
  // report[0]: モディファイアキー (Shift, Ctrl, Alt, GUI)
  // report[1]: 予約済み
  // report[2]-[7]: 現在押されているキー (最大6キー同時押し)

  // BLEが接続されていなければ何もしない
  if (!bleKeyboard.isConnected()) return;

  uint8_t modifiers = report[0];
  
  // モディファイアキーの処理
  bool ctrl = modifiers & KEYBOARD_MODIFIER_LEFTCTRL || modifiers & KEYBOARD_MODIFIER_RIGHTCTRL;
  bool shift = modifiers & KEYBOARD_MODIFIER_LEFTSHIFT || modifiers & KEYBOARD_MODIFIER_RIGHTSHIFT;
  bool alt = modifiers & KEYBOARD_MODIFIER_LEFTALT || modifiers & KEYBOARD_MODIFIER_RIGHTALT;
  bool gui = modifiers & KEYBOARD_MODIFIER_LEFTGUI || modifiers & KEYBOARD_MODIFIER_RIGHTGUI;
  
  // モディファイアキーを設定
  if (ctrl) bleKeyboard.press(KEY_LEFT_CTRL);
  else bleKeyboard.release(KEY_LEFT_CTRL);
  
  if (shift) bleKeyboard.press(KEY_LEFT_SHIFT);
  else bleKeyboard.release(KEY_LEFT_SHIFT);
  
  if (alt) bleKeyboard.press(KEY_LEFT_ALT);
  else bleKeyboard.release(KEY_LEFT_ALT);
  
  if (gui) bleKeyboard.press(KEY_LEFT_GUI);
  else bleKeyboard.release(KEY_LEFT_GUI);
  
  // 前回の入力と比較して変更があったキーを検出
  for (uint8_t i = 2; i < 8; i++) {
    bool found = false;
    
    // 現在押されているキーが前回も押されていたか確認
    if (prev_report != NULL) {
      for (uint8_t j = 2; j < 8; j++) {
        if (prev_report[j] == report[i]) {
          found = true;
          break;
        }
      }
    }
    
    // 新しく押されたキーを処理
    if (!found && report[i] != 0) {
      bleKeyboard.press(report[i]);
      Serial.print("キー押下: ");
      Serial.println(report[i], HEX);
    }
  }
  
  // 前回押されていたが今回離されたキーを検出
  if (prev_report != NULL) {
    for (uint8_t i = 2; i < 8; i++) {
      if (prev_report[i] == 0) continue;
      
      bool found = false;
      for (uint8_t j = 2; j < 8; j++) {
        if (prev_report[i] == report[j]) {
          found = true;
          break;
        }
      }
      
      // 前回は押されていたが今回は押されていないキーを処理
      if (!found) {
        bleKeyboard.release(prev_report[i]);
        Serial.print("キー離し: ");
        Serial.println(prev_report[i], HEX);
      }
    }
  }
  
  // 今回のレポートを保存
  prev_report = report;
}

// TinyUSB HIDコールバックのエイリアス設定
extern "C" void tuh_hid_keyboard_mounted(uint8_t dev_addr) {
  tuh_hid_keyboard_mounted_cb(dev_addr);
}

extern "C" void tuh_hid_keyboard_unmounted(uint8_t dev_addr) {
  tuh_hid_keyboard_unmounted_cb(dev_addr);
}

extern "C" void tuh_hid_keyboard_isr(uint8_t dev_addr, uint8_t const* report, uint16_t len) {
  tuh_hid_keyboard_report_received_cb(dev_addr, report, len);
}