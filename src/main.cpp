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
  // キー入力イベント処理のタイムスタンプ
  unsigned long lastKeyEventTime = 0;
  unsigned long lastKeyTimes[256] = {0}; // キーごとの最後の処理時間
  uint8_t lastProcessedKeycode = 0;
  
  void onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) override {
    unsigned long currentTime = millis();
    
    // 重複防止：同じキーが短時間内（15ms以内）に再度押された場合は無視
    // 以前は50msだったが、より高速な連打を可能にするため15msに短縮
    if (currentTime - lastKeyTimes[keycode] < 15) {
      Serial.printf("重複キー検出（無視）: ASCII=0x%02X, Keycode=0x%02X, Time=%ums\n", 
                  ascii, keycode, currentTime - lastKeyTimes[keycode]);
      return;
    }
    
    // このキーの処理時間を記録（次回の重複チェック用）
    lastKeyTimes[keycode] = currentTime;
    lastKeyEventTime = currentTime;
    lastProcessedKeycode = keycode;
    
    // デバッグ出力
    Serial.printf("Key processed: ASCII=0x%02X, Keycode=0x%02X, Modifier=0x%02X\n", 
                ascii, keycode, modifier);
    
    // 内蔵LEDを点灯
    ledController.keyPressed();
    
    // キー入力音を鳴らす
    speakerController.playKeySound();
    
    // BLEでキーを送信
    sendKeyToBle(keycode, modifier);
    
    // すべてのキーを必ず表示する
    char keyDescStr[32] = {0};
    
    // ASCIIコードの判定を修正
    if (' ' <= ascii && ascii <= '~') {
      sprintf(keyDescStr, "Key: %c (0x%02X)", ascii, keycode);
      displayController.showKeyPress((char)ascii, keycode);
    } else {
      // 特殊キーや未知のキーの処理（すべて確実に表示）
      sprintf(keyDescStr, "Key: 0x%02X", keycode);
      
      // 一般的な特殊キーの説明を追加
      switch (keycode) {
        case 0x28: strcat(keyDescStr, " [Enter]"); break;
        case 0x29: strcat(keyDescStr, " [Esc]"); break;
        case 0x2A: strcat(keyDescStr, " [Backspace]"); break;
        case 0x2B: strcat(keyDescStr, " [Tab]"); break;
        case 0x2C: strcat(keyDescStr, " [Space]"); break;
        case 0x4F: strcat(keyDescStr, " [Right]"); break;
        case 0x50: strcat(keyDescStr, " [Left]"); break;
        case 0x51: strcat(keyDescStr, " [Down]"); break;
        case 0x52: strcat(keyDescStr, " [Up]"); break;
        case 0x4A: strcat(keyDescStr, " [Home]"); break;
        case 0x4D: strcat(keyDescStr, " [End]"); break;
        case 0x4B: strcat(keyDescStr, " [PgUp]"); break;
        case 0x4E: strcat(keyDescStr, " [PgDn]"); break;
        case 0x39: strcat(keyDescStr, " [CapsLock]"); break;
        case 0x06: strcat(keyDescStr, " [Special]"); break;
        default: 
          if (keycode >= 0x3A && keycode <= 0x45) {
            sprintf(keyDescStr + strlen(keyDescStr), " [F%d]", keycode - 0x3A + 1);
          } else {
            strcat(keyDescStr, " [Unknown]");
          }
          break;
      }
      
      // 特殊キーは専用メソッドで表示（常に実行されるようにする）
      displayController.showRawKeyCode(keycode, keyDescStr);
    }
    
    // 印字可能文字の場合はテキストバッファに追加
    if (' ' <= ascii && ascii <= '~') {
      Serial.printf("Printable char: %c\n", ascii);
      displayController.addDisplayText((char)ascii);
    } else if (ascii == '\r') {
      Serial.println("Enter key");
      displayController.addDisplayText('\n');
    }
  }
  
  // キーボードレポート全体を処理するメソッドをオーバーライド
  void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
    // 親クラスのメソッドを呼び出して、通常のログ処理を行う
    EspUsbHost::onKeyboard(report, last_report);
    
    // シフト状態の確認
    bool shift = (report.modifier & KEYBOARD_MODIFIER_LEFTSHIFT) || 
                (report.modifier & KEYBOARD_MODIFIER_RIGHTSHIFT);
    
    // 新しく押されたキーのみを処理
    for (int i = 0; i < 6; i++) {
      if (report.keycode[i] != 0) {
        // このキーが前回レポートにないか確認（新規キーのみ）
        bool isNewKey = true;
        for (int j = 0; j < 6; j++) {
          if (last_report.keycode[j] == report.keycode[i]) {
            isNewKey = false;
            break;
          }
        }
        
        // 新しいキーのみを処理
        if (isNewKey) {
          uint8_t ascii = getKeycodeToAscii(report.keycode[i], shift);
          
          // すべてのキーコードを出力・処理（特殊キー含む）
          Serial.printf("新規キー検出: ASCII=0x%02X, keycode=0x%02X\n", 
                     ascii, report.keycode[i]);
          
          // キー入力処理を呼び出す
          onKeyboardKey(ascii, report.keycode[i], report.modifier);
        }
      }
    }
  }
  
  // 生のUSBデータを表示するためのオーバーライド
  void onReceive(const usb_transfer_t *transfer) override {
    endpoint_data_t *endpoint_data = &endpoint_data_list[(transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_NUM_MASK)];

    // すべてのエンドポイントからのデータを詳細に検査
    if (transfer->actual_num_bytes > 0) {
      // データを16進数表示
      String hex_data = "";
      for (int i = 0; i < transfer->actual_num_bytes; i++) {
        if (transfer->data_buffer[i] < 16) hex_data += "0";
        hex_data += String(transfer->data_buffer[i], HEX) + " ";
      }
      hex_data.trim();
      Serial.printf("Raw USB data: EP=0x%02X, Class=0x%02X, SubClass=0x%02X, bytes=%d, data=[%s]\n", 
                  transfer->bEndpointAddress, 
                  endpoint_data->bInterfaceClass,
                  endpoint_data->bInterfaceSubClass,
                  transfer->actual_num_bytes, hex_data.c_str());

      // 非ゼロのデータを探す（可能なキーコードを特定）
      for (int i = 0; i < transfer->actual_num_bytes; i++) {
        if (transfer->data_buffer[i] != 0 && i >= 2) {  // 先頭の2バイトは通常制御情報
          uint8_t possibleKeycode = transfer->data_buffer[i];
          
          // 一般的なキーコードの範囲内かチェック
          if (possibleKeycode >= 0x04 && possibleKeycode <= 0xE7 && 
              possibleKeycode != 0x00 && possibleKeycode != 0x01 && 
              possibleKeycode != 0xFF) {
            Serial.printf("  潜在的なキーコード検出: 0x%02X at position %d\n", possibleKeycode, i);
            
            // キーが前回のデータで処理されていない場合にのみ処理
            if (millis() - lastKeyTimes[possibleKeycode] > 200) { // 200ms以上経過なら別のキー入力と判断
              uint8_t modifier = transfer->data_buffer[0]; // 最初のバイトは通常修飾キー
              uint8_t ascii = getKeycodeToAscii(possibleKeycode, 
                             (modifier & KEYBOARD_MODIFIER_LEFTSHIFT) || 
                             (modifier & KEYBOARD_MODIFIER_RIGHTSHIFT));
                             
              Serial.printf("  未処理キーを検出: ASCII=0x%02X, keycode=0x%02X, modifier=0x%02X\n",
                         ascii, possibleKeycode, modifier);
              
              // 通常のキー処理チャネルで処理されなかったキーをここで処理
              onKeyboardKey(ascii, possibleKeycode, modifier);
              break; // 一度に1つのキーだけ処理
            }
          }
        }
      }
    }
    
    // 残りの通常処理を実行
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
  bool handleAsRawKeycode = false;
  
  // HIDキーコードからBLEキーコードへの変換
  switch (keycode) {
    // 英数字キー (HID Usage Table基準)
    case 0x04 ... 0x1D: // A-Z (0x04-0x1D)
      // 大文字か小文字かを判断（SHIFTキーの状態を確認）
      if (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) {
        // 大文字（SHIFTが押されている）
        bleKeycode = keycode - 0x04 + 'A'; // 例: 0x04 -> 'A' (65)
      } else {
        // 小文字（SHIFTが押されていない）
        bleKeycode = keycode - 0x04 + 'a'; // 例: 0x04 -> 'a' (97)
      }
      break;
      
    case 0x1E ... 0x27: // 1-0 (0x1E-0x27)
      if (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) {
        // SHIFTが押されている場合は記号を送信
        switch (keycode) {
          case 0x1E: bleKeycode = '!'; break; // 1/!
          case 0x1F: bleKeycode = '@'; break; // 2/@
          case 0x20: bleKeycode = '#'; break; // 3/#
          case 0x21: bleKeycode = '$'; break; // 4/$
          case 0x22: bleKeycode = '%'; break; // 5/%
          case 0x23: bleKeycode = '^'; break; // 6/^
          case 0x24: bleKeycode = '&'; break; // 7/&
          case 0x25: bleKeycode = '*'; break; // 8/*
          case 0x26: bleKeycode = '('; break; // 9/(
          case 0x27: bleKeycode = ')'; break; // 0/)
        }
      } else {
        // 数字を送信
        if (keycode == 0x27) { // 0キー
          bleKeycode = '0';
        } else {
          bleKeycode = keycode - 0x1E + '1'; // 例: 0x1E -> '1' (49)
        }
      }
      break;
    
    // 特殊キー
    case 0x28: bleKeycode = KEY_RETURN; break; // Enter
    case 0x29: bleKeycode = KEY_ESC; break; // ESC
    case 0x2A: bleKeycode = KEY_BACKSPACE; break; // Backspace
    case 0x2B: bleKeycode = KEY_TAB; break; // Tab
    case 0x2C: bleKeycode = ' '; break; // Space
    
    // 矢印キー
    case 0x4F: bleKeycode = KEY_RIGHT_ARROW; break;
    case 0x50: bleKeycode = KEY_LEFT_ARROW; break;
    case 0x51: bleKeycode = KEY_DOWN_ARROW; break;
    case 0x52: bleKeycode = KEY_UP_ARROW; break;
    
    // 記号キー
    case 0x2D: // -/_
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '_' : '-';
      break;
    case 0x2E: // =/+
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '+' : '=';
      break;
    case 0x2F: // [/{
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '{' : '[';
      break;
    case 0x30: // ]/}
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '}' : ']';
      break;
    case 0x31: // \|
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '|' : '\\';
      break;
    case 0x32: // 日本語キーボード特有キー (#/~)
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '~' : '#';
      break;
    case 0x33: // ;/:
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? ':' : ';';
      break;
    case 0x34: // '/''
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '"' : '\'';
      break;
    case 0x35: // `/~
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '~' : '`';
      break;
    case 0x36: // ,/<
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '<' : ',';
      break;
    case 0x37: // ./>
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '>' : '.';
      break;
    case 0x38: // //?
      bleKeycode = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) ? '?' : '/';
      break;
    case 0x39: // Caps Lock
      bleKeycode = KEY_CAPS_LOCK;
      break;
      
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
    case 0x46: bleKeycode = KEY_PRTSC; break; // Print Screen
    case 0x47: bleKeycode = KEY_CAPS_LOCK; break; // Scroll Lock - KEY_SCROLL_LOCKは未定義のため代替
    case 0x48: bleKeycode = KEY_PRTSC; break; // Pause - KEY_PAUSEは未定義のため代替
    case 0x49: bleKeycode = KEY_INSERT; break; // Insert
    case 0x4A: bleKeycode = KEY_HOME; break; // Home
    case 0x4B: bleKeycode = KEY_PAGE_UP; break; // Page Up
    case 0x4C: bleKeycode = KEY_DELETE; break; // Delete
    case 0x4D: bleKeycode = KEY_END; break; // End
    case 0x4E: bleKeycode = KEY_PAGE_DOWN; break; // Page Down
    
    // テンキー
    case 0x53: bleKeycode = KEY_CAPS_LOCK; break; // Num Lock - KEY_NUM_LOCKは未定義のため代替
    case 0x54: bleKeycode = KEY_NUM_SLASH; break; // テンキー /
    case 0x55: bleKeycode = KEY_NUM_ASTERISK; break; // テンキー *
    case 0x56: bleKeycode = KEY_NUM_MINUS; break; // テンキー -
    case 0x57: bleKeycode = KEY_NUM_PLUS; break; // テンキー +
    case 0x58: bleKeycode = KEY_NUM_ENTER; break; // テンキー Enter
    case 0x59: bleKeycode = KEY_NUM_1; break; // テンキー 1
    case 0x5A: bleKeycode = KEY_NUM_2; break; // テンキー 2
    case 0x5B: bleKeycode = KEY_NUM_3; break; // テンキー 3
    case 0x5C: bleKeycode = KEY_NUM_4; break; // テンキー 4
    case 0x5D: bleKeycode = KEY_NUM_5; break; // テンキー 5
    case 0x5E: bleKeycode = KEY_NUM_6; break; // テンキー 6
    case 0x5F: bleKeycode = KEY_NUM_7; break; // テンキー 7
    case 0x60: bleKeycode = KEY_NUM_8; break; // テンキー 8
    case 0x61: bleKeycode = KEY_NUM_9; break; // テンキー 9
    case 0x62: bleKeycode = KEY_NUM_0; break; // テンキー 0
    case 0x63: bleKeycode = KEY_NUM_PERIOD; break; // テンキー .
    
    // 言語固有キー
    case 0x87: bleKeycode = 0x87; handleAsRawKeycode = true; break; // International 1 (日本語キーボードの場合「\」「_」)
    case 0x88: bleKeycode = 0x88; handleAsRawKeycode = true; break; // International 2 (かな/カナ)
    case 0x89: bleKeycode = 0x89; handleAsRawKeycode = true; break; // International 3 (¥ | 円記号)
    case 0x8A: bleKeycode = 0x8A; handleAsRawKeycode = true; break; // International 4 (変換)
    case 0x8B: bleKeycode = 0x8B; handleAsRawKeycode = true; break; // International 5 (無変換)
    
    // メディアコントロールキー
    case 0xE2: 
      bleKeyboard.write(KEY_MEDIA_MUTE);
      return;
    case 0xE9: 
      bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
      return;
    case 0xEA: 
      bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
      return;
    case 0xB5: 
      bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
      return;
    case 0xB6: 
      bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
      return;
    case 0xB7: 
      bleKeyboard.write(KEY_MEDIA_STOP);
      return;
    case 0xCD: 
      bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
      return;
    
    default:
      #if DEBUG_OUTPUT
      Serial.printf("未対応のキーコード: 0x%02X\n", keycode);
      #endif
      return;
  }
  
  // キーを単一のイベントとして送信（1回の書き込み操作）
  #if DEBUG_OUTPUT
  Serial.printf("BLE write: 0x%02X (char: %c)\n", bleKeycode, 
              (bleKeycode >= 32 && bleKeycode <= 126) ? (char)bleKeycode : '?');
  #endif
  
  if (handleAsRawKeycode) {
    // 生のキーコードとして送信（特殊な言語キーなど）
    if (modifier) {
      // 修飾キーと組み合わせる場合
      if (modifier & KEYBOARD_MODIFIER_LEFTCTRL) bleKeyboard.press(KEY_LEFT_CTRL);
      if (modifier & KEYBOARD_MODIFIER_LEFTSHIFT) bleKeyboard.press(KEY_LEFT_SHIFT);
      if (modifier & KEYBOARD_MODIFIER_LEFTALT) bleKeyboard.press(KEY_LEFT_ALT);
      if (modifier & KEYBOARD_MODIFIER_LEFTGUI) bleKeyboard.press(KEY_LEFT_GUI);
      if (modifier & KEYBOARD_MODIFIER_RIGHTCTRL) bleKeyboard.press(KEY_RIGHT_CTRL);
      if (modifier & KEYBOARD_MODIFIER_RIGHTSHIFT) bleKeyboard.press(KEY_RIGHT_SHIFT);
      if (modifier & KEYBOARD_MODIFIER_RIGHTALT) bleKeyboard.press(KEY_RIGHT_ALT);
      if (modifier & KEYBOARD_MODIFIER_RIGHTGUI) bleKeyboard.press(KEY_RIGHT_GUI);
      
      bleKeyboard.press(bleKeycode);
      delay(10);
      bleKeyboard.releaseAll();
    } else {
      bleKeyboard.press(bleKeycode);
      delay(10);
      bleKeyboard.release(bleKeycode);
    }
  } else {
    // 通常のキー入力として送信
    bleKeyboard.write(bleKeycode);
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
  
  // キーフラグのリセット処理
  usbHost.resetKeyFlags();
}
