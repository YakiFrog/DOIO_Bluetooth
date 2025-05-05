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

void sendKeyToBle(uint8_t keycode, uint8_t modifier);

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
      
      // キー入力音を鳴らす - BLE接続に関係なく必ず音を鳴らす
      speakerController.playKeySound();
      
      // BLEでキーを送信 - BLE接続時のみ（一度だけ送信）
      sendKeyToBle(keycode, modifier);
      
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
            snprintf(lastKeyCodeText, sizeof(lastKeyCodeText), "0x%02X", keycode);
            keyName = lastKeyCodeText;
            break;
        }
        
        #if DEBUG_OUTPUT
        Serial.printf("Special key: %s (0x%02X)\n", keyName, keycode);
        #endif
        
        // キー入力を表示
        displayController.showKeyPress(displayChar, keycode);
      }
    }
  }
  
  // キーボード入力の詳細表示用
  char lastKeyCodeText[10] = {0};
  
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
      if (changed_modifier & KEYBOARD_MODIFIER_LEFTSHIFT) {
        if (report.modifier & KEYBOARD_MODIFIER_LEFTSHIFT) {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.press(KEY_LEFT_SHIFT);
        } else {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.release(KEY_LEFT_SHIFT);
        }
      }
      
      if (changed_modifier & KEYBOARD_MODIFIER_LEFTALT) {
        if (report.modifier & KEYBOARD_MODIFIER_LEFTALT) {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.press(KEY_LEFT_ALT);
        } else {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.release(KEY_LEFT_ALT);
        }
      }
      
      if (changed_modifier & KEYBOARD_MODIFIER_LEFTGUI) {
        if (report.modifier & KEYBOARD_MODIFIER_LEFTGUI) {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.press(KEY_LEFT_GUI);
        } else {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.release(KEY_LEFT_GUI);
        }
      }
      
      if (changed_modifier & KEYBOARD_MODIFIER_RIGHTCTRL) {
        if (report.modifier & KEYBOARD_MODIFIER_RIGHTCTRL) {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.press(KEY_RIGHT_CTRL);
        } else {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.release(KEY_RIGHT_CTRL);
        }
      }
      
      if (changed_modifier & KEYBOARD_MODIFIER_RIGHTSHIFT) {
        if (report.modifier & KEYBOARD_MODIFIER_RIGHTSHIFT) {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.press(KEY_RIGHT_SHIFT);
        } else {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.release(KEY_RIGHT_SHIFT);
        }
      }
      
      if (changed_modifier & KEYBOARD_MODIFIER_RIGHTALT) {
        if (report.modifier & KEYBOARD_MODIFIER_RIGHTALT) {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.press(KEY_RIGHT_ALT);
        } else {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.release(KEY_RIGHT_ALT);
        }
      }
      
      if (changed_modifier & KEYBOARD_MODIFIER_RIGHTGUI) {
        if (report.modifier & KEYBOARD_MODIFIER_RIGHTGUI) {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.press(KEY_RIGHT_GUI);
        } else {
          if (bleEnabled && bleKeyboard.isConnected()) bleKeyboard.release(KEY_RIGHT_GUI);
        }
      }
      
      last_modifier = report.modifier;
    }
    
    // 新しく押されたキーを処理 (前回のレポートにないキー)
    bool shift = (report.modifier & KEYBOARD_MODIFIER_LEFTSHIFT) || (report.modifier & KEYBOARD_MODIFIER_RIGHTSHIFT);
    for (int i = 0; i < 6; i++) {
      if (report.keycode[i] != 0 && !hasKeycode(last_report, report.keycode[i])) {
        uint8_t ascii = getKeycodeToAscii(report.keycode[i], shift);
        onKeyboardKey(ascii, report.keycode[i], report.modifier);
      }
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
      Serial.println("このデバイス専用の処理を適用します...");
      
      // インターフェース2を使用（Pythonツールの解析結果から修正）
      Serial.println("Interface 2を使用します (updated)");
      
      // デバッグ出力を強化
      ESP_LOGI("EspUsbHost", "DOIO KB16 キーボード対応モードを有効化");
      ESP_LOGI("EspUsbHost", "Usage Page: 0xFF60, Usage: 0x0061");
      
      // KB16キーボード用フラグを設定
      isDoioKb16 = true;
      // データサイズ: 32バイトまたは6バイト
      doioDataSize = 32;
    }
    
    // デバイス情報の表示時間を短く調整（3秒→1秒）
    delay(1000); 
    
    // 通常画面に戻る
    displayController.updateDisplay();
  }
  
  // 生のUSBデータを表示するためのオーバーライド
  void onReceive(const usb_transfer_t *transfer) override {
    endpoint_data_t *endpoint_data = &endpoint_data_list[(transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_NUM_MASK)];

    // DOIO KB16キーボード用の特別処理（絶対に消さない）
    if (isDoioKb16) {
      if (transfer->actual_num_bytes > 0) {
        #if DEBUG_OUTPUT
        // データを16進数表示
        String hex_data = "";
        for (int i = 0; i < transfer->actual_num_bytes; i++) {
          if (transfer->data_buffer[i] < 16) hex_data += "0";
          hex_data += String(transfer->data_buffer[i], HEX) + " ";
        }
        hex_data.trim();
        Serial.printf("DOIO KB16 Raw data: EP=0x%02X, bytes=%d, data=[%s]\n", 
                    transfer->bEndpointAddress, transfer->actual_num_bytes, hex_data.c_str());
        #endif
        
        // キーボードデータ構造を解析
        hid_keyboard_report_t report = {};
        
        // 第1バイト: 修飾キー (保存するが、常に0x06の場合は無視)
        uint8_t rawModifier = transfer->data_buffer[0];
        
        // KB16は常に0x06（左Shift+左Alt）を送信するようだが、実際はそうでないので無視する
        // 代わりに特定の修飾キーを検出する
        if (rawModifier == 0x06) {
          report.modifier = 0; // 修飾キーをリセット
        } else {
          report.modifier = rawModifier; // 異なる値なら採用
        }
        
        // すべてのバイトをスキャンして非ゼロの値（キーコード）を探す
        // 無効なキーコードを除外 (0x01, 0x02, 0x03, 0xFF, 0x40, 0x80など)
        for (int i = 1; i < transfer->actual_num_bytes; i++) {
          uint8_t keycode = transfer->data_buffer[i];
          // 有効なキーコードの範囲をチェック (主に0x04-0x38, 一部特殊キー)
          if (keycode >= 0x04 && keycode <= 0x65 && // 標準的なキーコード範囲
              keycode != 0x40 && keycode != 0x80) { // よく誤検出される値を除外
            addKeyToReport(keycode, report);
          }
          
          // 特殊キーの検出 (修飾キーかどうかを確認)
          if (i >= 1 && i <= 4) { // インデックス1-4は制御バイトの可能性
            switch (keycode) {
              case 0x01: // Ctrlキーの可能性
                report.modifier |= KEYBOARD_MODIFIER_LEFTCTRL;
                break;
              case 0x02: // Shiftキーの可能性
                report.modifier |= KEYBOARD_MODIFIER_LEFTSHIFT;
                break;
              case 0x04: // Altキーの可能性 (検出済みの可能性)
                report.modifier |= KEYBOARD_MODIFIER_LEFTALT;
                break;
              case 0x08: // GUIキーの可能性
                report.modifier |= KEYBOARD_MODIFIER_LEFTGUI;
                break;
            }
          }
        }
        
        static hid_keyboard_report_t last_report = {};
        
        // キーボード状態が変化した場合だけ処理
        if (memcmp(&last_report, &report, sizeof(report)) != 0) {
          #if DEBUG_OUTPUT
          Serial.printf("KB16キーボード状態変化: modifier=0x%02X, keys=[0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X]\n",
                     report.modifier, report.keycode[0], report.keycode[1], report.keycode[2], 
                     report.keycode[3], report.keycode[4], report.keycode[5]);
          #endif
          
          // 標準のキーボード処理を呼び出す
          onKeyboard(report, last_report);
          
          // 新しく押されたキーを処理 (前回のレポートにないキー)
          bool shift = (report.modifier & KEYBOARD_MODIFIER_LEFTSHIFT) || (report.modifier & KEYBOARD_MODIFIER_RIGHTSHIFT);
          for (int i = 0; i < 6; i++) {
            if (report.keycode[i] != 0 && !hasKeycode(last_report, report.keycode[i])) {
              uint8_t ascii = getKeycodeToAscii(report.keycode[i], shift);
              #if DEBUG_OUTPUT
              Serial.printf("キー押下: ASCII=0x%02X (%c), keycode=0x%02X, modifier=0x%02X\n", 
                         ascii, (ascii >= 32 && ascii <= 126) ? (char)ascii : '?', 
                         report.keycode[i], report.modifier);
              #endif
              onKeyboardKey(ascii, report.keycode[i], report.modifier);
            }
          }
          
          // 最後の状態を更新
          memcpy(&last_report, &report, sizeof(last_report));
        }
      }
    } 
    // 通常のデバイス（DOIO KB16以外）に対する処理
    else {
      #if DEBUG_OUTPUT
      // 生のデータバッファをデバッグ表示
      if (transfer->actual_num_bytes > 0) {
        String buffer_str = "";
        for (int i = 0; i < transfer->actual_num_bytes && i < 16; i++) {
          if (transfer->data_buffer[i] < 16) {
            buffer_str += "0";
          }
          buffer_str += String(transfer->data_buffer[i], HEX) + " ";
        }
        buffer_str.trim();
        Serial.printf("Raw data received: EP=0x%02X, bytes=%d, data=[ %s ]\n", 
                    transfer->bEndpointAddress, transfer->actual_num_bytes, buffer_str.c_str());
      }
      #endif
    }
  }
  
  // 指定されたキーコードがレポートに含まれているか確認
  bool hasKeycode(const hid_keyboard_report_t &report, uint8_t keycode) {
    for (int i = 0; i < 6; i++) {
      if (report.keycode[i] == keycode) {
        return true;
      }
    }
    return false;
  }
  
  // レポートの空きスロットにキーを追加
  void addKeyToReport(uint8_t keycode, hid_keyboard_report_t &report) {
    if (keycode == 0) return; // 0は追加しない
    
    // 既に含まれているキーは追加しない
    for (int i = 0; i < 6; i++) {
      if (report.keycode[i] == keycode) {
        return;
      }
    }
    
    // 空きスロットを探して追加
    for (int i = 0; i < 6; i++) {
      if (report.keycode[i] == 0) {
        report.keycode[i] = keycode;
        return;
      }
    }
  }
  
private:
  // DOIO KB16キーボードフラグ
  bool isDoioKb16 = false;
  // DOIO KB16のデータサイズ（32バイトまたは6バイト）
  uint8_t doioDataSize = 32;
};

MyEspUsbHost usbHost;

// BLEにキーを転送する関数
void sendKeyToBle(uint8_t keycode, uint8_t modifier) {
  if (!bleEnabled) {
    return;
  }

  // BLEが未接続の場合は何もしない
  if (!bleKeyboard.isConnected()) {
    #if DEBUG_OUTPUT
    Serial.println("BLE not connected, skipping key send");
    #endif
    return;
  }

  #if DEBUG_OUTPUT
  Serial.printf("BLE send key: keycode=0x%02X, modifier=0x%02X\n", keycode, modifier);
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
    case 0x3C: bleKeycode = KEY_F3; break;
    case 0x3D: bleKeycode = KEY_F4; break;
    case 0x3E: bleKeycode = KEY_F5; break;
    case 0x3F: bleKeycode = KEY_F6; break;
    case 0x40: bleKeycode = KEY_F7; break;
    case 0x41: bleKeycode = KEY_F8; break;
    case 0x42: bleKeycode = KEY_F9; break;
    case 0x43: bleKeycode = KEY_F10; break;
    case 0x44: bleKeycode = KEY_F11; break;
    case 0x45: bleKeycode = KEY_F12; break;
    
    // その他の特殊キー
    case 0x46: bleKeycode = KEY_PRTSC; break;
    case 0x47: bleKeycode = KEY_CAPS_LOCK; break; // SCROLL_LOCKをCAPS_LOCKに置き換え
    case 0x48: bleKeycode = KEY_INSERT; break;    // PAUSEをINSERTに置き換え
    case 0x49: bleKeycode = KEY_INSERT; break;
    case 0x4A: bleKeycode = KEY_HOME; break;
    case 0x4B: bleKeycode = KEY_PAGE_UP; break;
    case 0x4C: bleKeycode = KEY_DELETE; break;
    case 0x4D: bleKeycode = KEY_END; break;
    case 0x4E: bleKeycode = KEY_PAGE_DOWN; break;
    
    default:
      #if DEBUG_OUTPUT
      Serial.printf("未対応のキーコード: 0x%02X\n", keycode);
      #endif
      return;
  }
  
  // キーを単一のイベントとして送信（1回の書き込み操作）
  // write()メソッドはキーを押してすぐにリリースする
  #if DEBUG_OUTPUT
  Serial.printf("BLE write: 0x%02X (char: %c)\n", bleKeycode, 
              (bleKeycode >= 32 && bleKeycode <= 126) ? (char)bleKeycode : '?');
  #endif
  bleKeyboard.write(bleKeycode);
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
