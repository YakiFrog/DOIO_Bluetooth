#include <Arduino.h>
#include "EspUsbHost.h"
#include <Wire.h>
#include <BleKeyboard.h>
#include "DisplayController.h"
#include "Peripherals.h"

// BLEキーボードの設定
BleKeyboard bleKeyboard("DOIO Keyboard", "DOIO", 100);
bool bleEnabled = true;  // BLE機能のオンオフ制御用

// 最後のキー入力の情報を保持
char lastKeyCodeText[8]; // キーコードを文字列として保持するバッファ

void forwardKeyToBle(uint8_t keycode, uint8_t modifier, bool keyDown);

class MyEspUsbHost : public EspUsbHost {
public:
  void onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) override {
    // キーコードとASCIIの両方をデバッグ出力
    #if DEBUG_OUTPUT
    Serial.printf("Key pressed: ASCII=0x%02X, Keycode=0x%02X, Modifier=0x%02X\n", ascii, keycode, modifier);
    #endif

    // 何かしらのキー入力があった
    if (keycode != 0) {
      // 内蔵LEDを点灯
      ledController.keyPressed();
      
      // キー入力音を鳴らす
      speakerController.playKeySound();
      
      // BLEでキーを送信(キー押下)
      forwardKeyToBle(keycode, modifier, true);
      
      if (' ' <= ascii && ascii <= '~') {
        // 印字可能文字
        #if DEBUG_OUTPUT
        Serial.printf("Printable char: %c\n", ascii);
        #endif
        
        // テキストバッファに追加
        displayController.addDisplayText((char)ascii);
        
        // キー入力を表示
        displayController.showKeyPress((char)ascii, keycode);
      } else if (ascii == '\r') {
        // 改行
        #if DEBUG_OUTPUT
        Serial.println("Enter key");
        #endif
        
        displayController.addDisplayText('\n');
        
        // キー入力を表示
        displayController.showKeyPress(CHAR_ENTER, keycode);
      } else {
        // その他の特殊キー
        char displayChar = '?';
        const char* keyName = "Unknown";
        
        switch (keycode) {
          case 0x29: // ESC
            keyName = "ESC";
            displayChar = 'E';
            break;
          case 0x2A: // BACKSPACE
            keyName = "BS";
            displayChar = CHAR_LEFT;
            break;
          case 0x2B: // TAB
            keyName = "TAB";
            displayChar = 'T';
            break;
          case 0x4F: // 右矢印
            keyName = "RIGHT";
            displayChar = CHAR_RIGHT;
            break;
          case 0x50: // 左矢印
            keyName = "LEFT";
            displayChar = CHAR_LEFT;
            break;
          case 0x51: // 下矢印
            keyName = "DOWN";
            displayChar = CHAR_DOWN;
            break;
          case 0x52: // 上矢印
            keyName = "UP";
            displayChar = CHAR_UP;
            break;
          default:
            // 不明なキーはコードを16進数で表示
            snprintf(::lastKeyCodeText, sizeof(::lastKeyCodeText), "0x%02X", keycode);
            keyName = ::lastKeyCodeText;
            break;
        }
        
        #if DEBUG_OUTPUT
        Serial.printf("Special key: %s (0x%02X)\n", keyName, keycode);
        #endif
        
        // キー入力を表示
        displayController.showKeyPress(displayChar, keycode);
      }
      
      // キーのリリース(キーを離す操作)も送信
      forwardKeyToBle(keycode, modifier, false);
    }
  }
  
  // キーボードレポート全体を処理するメソッドをオーバーライド
  void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
    // 親クラスのメソッドを呼び出して、通常の処理を行う
    EspUsbHost::onKeyboard(report, last_report);
    
    #if DEBUG_OUTPUT
    // レポート全体をデバッグ出力
    Serial.printf("Keyboard report: modifier=0x%02X, keys=[", report.modifier);
    for (int i = 0; i < 6; i++) {
      Serial.printf("0x%02X ", report.keycode[i]);
    }
    Serial.println("]");
    #endif
    
    // 修飾キーの検出と送信（BLE経由）
    static uint8_t last_modifier = 0;
    if (report.modifier != last_modifier) {
      // 修飾キー（Ctrl, Shift, Alt, GUI）の変更を処理
      uint8_t changed_modifier = last_modifier ^ report.modifier;
      
      // 変更された修飾キーを特定して送信
      if (changed_modifier & KEYBOARD_MODIFIER_LEFTCTRL) {
        if (report.modifier & KEYBOARD_MODIFIER_LEFTCTRL) {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.press(KEY_LEFT_CTRL);
        } else {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.release(KEY_LEFT_CTRL);
        }
      }
      
      // 他の修飾キーも同様に処理
      // ...existing code...
      
      last_modifier = report.modifier;
    }
  }
  
  // デバイス接続時のコールバック
  void onDeviceConnected() override {
    // デバイス情報を表示
    Serial.println("\n==== USB DEVICE DETECTED ====");
    Serial.printf("Vendor ID: 0x%04X\n", idVendor);
    Serial.printf("Product ID: 0x%04X\n", idProduct);
    Serial.printf("Manufacturer: %s\n", manufacturer.c_str());
    Serial.printf("Product: %s\n", productName.c_str());
    Serial.printf("Serial: %s\n", serialNumber.c_str());
    Serial.println("============================\n");
    
    // ディスプレイに情報を表示
    displayController.showDeviceInfo(manufacturer, productName, idVendor, idProduct);
    displayController.setUsbConnected(true);
    
    // DOIO KB16キーボード用の特別処理
    if ((idVendor == 0xD010 && idProduct == 0x1601) || 
        (manufacturer == "DOIO" && productName == "DOIO")) {
      Serial.println("DOIO KB16キーボードを検出しました！");
      // ...existing code...
    }
    // その他のデバイス固有処理
    // ...existing code...
    
    delay(3000); // 3秒間デバイス情報を表示
    displayController.updateDisplay();
  }
  
  // 生のUSBデータを表示するためのオーバーライド
  void onReceive(const usb_transfer_t *transfer) override {
    // ...existing code...
  }
};

MyEspUsbHost usbHost;

// BLEにキーを転送する関数
void forwardKeyToBle(uint8_t keycode, uint8_t modifier, bool keyDown) {
  if (!bleEnabled) {
    return;
  }

  // BLEが未接続の場合は接続試行
  if (!bleKeyboard.isConnected()) {
    #if DEBUG_OUTPUT
    Serial.println("BLE not connected, ensuring advertising is active...");
    #endif
    return;
  }

  #if DEBUG_OUTPUT
  Serial.printf("BLE forward key: keycode=0x%02X, modifier=0x%02X, %s\n", 
              keycode, modifier, keyDown ? "press" : "release");
  #endif

  uint8_t bleKeycode = 0;
  
  // HIDキーコードからBLEキーコードへの変換
  switch (keycode) {
    // 英数字キー (HID Usage Table基準)
    case 0x04 ... 0x1D: // A-Z (0x04-0x1D)
      bleKeycode = keycode - 0x04 + 'a'; // 例: 0x04 -> 'a' (97)
      break;
      
    case 0x1E ... 0x27: // 1-0 (0x1E-0x27)
      if (keycode == 0x27) { // 0キー
        bleKeycode = '0';
      } else {
        bleKeycode = keycode - 0x1E + '1'; // 例: 0x1E -> '1' (49)
      }
      break;
    
    // 特殊キー
    case 0x28: bleKeycode = KEY_RETURN; break;
    case 0x29: bleKeycode = KEY_ESC; break;
    case 0x2A: bleKeycode = KEY_BACKSPACE; break;
    case 0x2B: bleKeycode = KEY_TAB; break;
    case 0x2C: bleKeycode = ' '; break;
    case 0x4F: bleKeycode = KEY_RIGHT_ARROW; break;
    case 0x50: bleKeycode = KEY_LEFT_ARROW; break;
    case 0x51: bleKeycode = KEY_DOWN_ARROW; break;
    case 0x52: bleKeycode = KEY_UP_ARROW; break;
    
    // 記号キー
    case 0x2D: bleKeycode = '-'; break;
    case 0x2E: bleKeycode = '='; break;
    case 0x2F: bleKeycode = '['; break;
    case 0x30: bleKeycode = ']'; break;
    case 0x31: bleKeycode = '\\'; break;
    case 0x33: bleKeycode = ';'; break;
    case 0x34: bleKeycode = '\''; break;
    case 0x35: bleKeycode = '`'; break;
    case 0x36: bleKeycode = ','; break;
    case 0x37: bleKeycode = '.'; break;
    case 0x38: bleKeycode = '/'; break;
      
    // ファンクションキー
    case 0x3A: bleKeycode = KEY_F1; break;
    case 0x3B: bleKeycode = KEY_F2; break;
    // ...existing code...
    
    default:
      #if DEBUG_OUTPUT
      Serial.printf("未対応のキーコード: 0x%02X\n", keycode);
      #endif
      return;
  }
  
  // キーを送信
  if (keyDown) {
    #if DEBUG_OUTPUT
    Serial.printf("BLE press: 0x%02X (char: %c)\n", bleKeycode, 
              (bleKeycode >= 32 && bleKeycode <= 126) ? (char)bleKeycode : '?');
    #endif
    bleKeyboard.press(bleKeycode);
  } else {
    #if DEBUG_OUTPUT
    Serial.printf("BLE release: 0x%02X\n", bleKeycode);
    #endif
    bleKeyboard.release(bleKeycode);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  // I2C通信の初期化
  Wire.begin();
  
  #if DEBUG_OUTPUT
  Serial.println("Starting USB Host and BLE Controller...");
  #endif
  
  // デバイスコントローラの初期化
  displayController.begin();
  ledController.begin();
  speakerController.begin();
  
  // 起動音を再生
  speakerController.playStartupMelody();
  
  // BLEキーボードの初期化
  if (bleEnabled) {
    bleKeyboard.begin();
    #if DEBUG_OUTPUT
    Serial.println("BLE Keyboard initialized and advertising...");
    #endif
  }
  
  // USBホストの初期化
  usbHost.begin();
  usbHost.setHIDLocal(HID_LOCAL_Japan_Katakana);
  
  // 初期状態を表示
  displayController.updateDisplay();
  
  #if DEBUG_OUTPUT
  Serial.println("USB Host initialized. Waiting for devices...");
  #endif
}

void loop() {
  // USBホストのタスク処理
  usbHost.task();
  
  // BLE接続状態の確認と管理
  static unsigned long lastBleCheckTime = 0;
  static bool wasConnected = false;
  
  // 1秒ごとにBLE接続状態を確認
  if (bleEnabled && millis() - lastBleCheckTime > 2000) {
    lastBleCheckTime = millis();
    
    // 現在の接続状態を取得
    bool isConnected = bleKeyboard.isConnected();
    
    // 接続状態が変わった時の処理
    if (wasConnected && !isConnected) {
      // 切断を検出
      #if DEBUG_OUTPUT
      Serial.println("BLE disconnected.");
      #endif
      
      // ステータスLEDを更新
      ledController.setBleConnected(false);
      displayController.setBleConnected(false);
      
      // 切断音を再生
      speakerController.playDisconnectedSound();
    } 
    else if (!wasConnected && isConnected) {
      // 接続を検出
      #if DEBUG_OUTPUT
      Serial.println("BLE connected successfully!");
      #endif
      
      // ステータスLEDを更新
      ledController.setBleConnected(true);
      displayController.setBleConnected(true);
      
      // 接続音を再生
      speakerController.playConnectedSound();
    }
    
    // 現在の状態を保存
    wasConnected = isConnected;
  }
  
  // LED更新処理
  ledController.updateKeyLED();   // キー入力LED
  ledController.updateStatusLED(); // ステータスLED
}
