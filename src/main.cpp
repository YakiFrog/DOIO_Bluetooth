#include <Arduino.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>

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
}

void loop() {
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

  if (Serial.available()) { // USB CDC経由でデータ受信 (DOIO KB16から)
    char key = Serial.read(); // キーコード取得
    
    if (key >= 32 && key <= 126) {  // 印字可能な文字
      Serial.print("受信キー: '");
      Serial.print(key);
      Serial.print("' (ASCII: ");
      Serial.print((int)key);
      Serial.println(")");
    } else {
      Serial.print("受信制御コード: ");
      Serial.println((int)key);
    }
    
    if (bleKeyboard.isConnected()) { // Bluetooth接続済み
      bleKeyboard.print(key); // 標準キー送信
      
      // 特殊キーの処理例（必要に応じて拡張可能）
      // 例：ASCIIコード13（CR）をEnterキーとして扱う
      if (key == 13) {
        bleKeyboard.write(KEY_RETURN);
      }
    } else {
      Serial.println("Bluetooth未接続のためキー送信をスキップしました。");
    }
  }
  
  // 接続状態のフラッシュ表示（接続中は点灯、未接続時はゆっくり点滅）
  if (!isConnected) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(900);
  }
}