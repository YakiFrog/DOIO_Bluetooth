#include <Arduino.h>
#include "EspUsbHost.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// 特殊文字の定数定義
#define CHAR_ENTER     'E'  // Enter key symbol
#define CHAR_LEFT      '<'  // Left arrow
#define CHAR_RIGHT     '>'  // Right arrow
#define CHAR_DOWN      'v'  // Down arrow
#define CHAR_UP        '^'  // Up arrow

// デバッグ出力の有効化
#define DEBUG_OUTPUT 1
// 詳細なUSB情報デバッグの有効化
#define USB_DEBUG_DETAIL 1

// ディスプレイの設定
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// 内蔵LEDを定義
#define LED_PIN 21

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 表示用のテキストバッファ
String displayText = "";
const int maxChars = 100;

// 最後のキー入力の情報を保持
char lastKeyPressed = 0;
unsigned long lastKeyPressTime = 0;
const unsigned long keyLedDuration = 500; // LEDを点灯させる時間（ミリ秒）

// ステータスフラグと更新時間の定義
bool lastUsbStatus = false;
unsigned long lastStatusUpdate = 0;
const unsigned long statusUpdateInterval = 1000; // 1秒ごとにステータス更新

// デバイス情報表示用
String deviceName = "None";
uint16_t vendorId = 0;
uint16_t productId = 0;
bool deviceInfoUpdated = false;

void updateDisplay();
void updateStatusDisplay();
void showKeyPress(char keyChar, uint8_t keycode);

class MyEspUsbHost : public EspUsbHost {
public:
  void onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) override {
    // キーコードとASCIIの両方をデバッグ出力
    #if DEBUG_OUTPUT
    Serial.printf("Key pressed: ASCII=0x%02X, Keycode=0x%02X, Modifier=0x%02X\n", ascii, keycode, modifier);
    #endif

    // 何かしらのキー入力があった
    if (keycode != 0) {
      // LEDを点灯
      digitalWrite(LED_PIN, HIGH);
      lastKeyPressTime = millis();
      
      if (' ' <= ascii && ascii <= '~') {
        // 印字可能文字
        #if DEBUG_OUTPUT
        Serial.printf("Printable char: %c\n", ascii);
        #endif
        
        // テキストバッファに追加
        displayText += (char)ascii;
        if (displayText.length() > maxChars) {
          displayText = displayText.substring(displayText.length() - maxChars);
        }
        
        // キー入力を表示
        showKeyPress((char)ascii, keycode);
      } else if (ascii == '\r') {
        // 改行
        #if DEBUG_OUTPUT
        Serial.println("Enter key");
        #endif
        
        displayText += '\n';
        if (displayText.length() > maxChars) {
          displayText = displayText.substring(displayText.length() - maxChars);
        }
        
        // キー入力を表示
        showKeyPress(CHAR_ENTER, keycode);
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
        showKeyPress(displayChar, keycode);
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
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("USB: Connected");
    display.setCursor(0, 10);
    display.printf("VID: 0x%04X", idVendor);
    display.setCursor(0, 20);
    display.printf("PID: 0x%04X", idProduct);
    display.setCursor(0, 30);
    display.println(manufacturer);
    display.setCursor(0, 40);
    display.println(productName);
    display.display();
    
    // 情報を更新
    deviceName = productName;
    vendorId = idVendor;
    productId = idProduct;
    deviceInfoUpdated = true;
    
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
    // LeafLabs Maple（VID: 0x1EAF, PID: 0x0003）の場合
    else if (idVendor == 0x1EAF && idProduct == 0x0003) {
      Serial.println("LeafLabs Maple デバイスを検出しました！");
      Serial.println("このデバイス用のカスタム処理を適用します...");
      
      // デバッグモードを強化
      #if DEBUG_OUTPUT
      Serial.println("詳細なデバッグ出力を有効化しました");
      #endif
      
      // LeafLabs Maple用の特殊処理
      // このデバイスが標準HIDプロトコルと異なる場合に必要な調整
    }
    
    delay(3000); // 3秒間デバイス情報を表示
    updateDisplay();
  }
  
  // デバイスが接続解除されたときの処理
  void onGone(const usb_host_client_event_msg_t *eventMsg) override {
    #if DEBUG_OUTPUT
    Serial.println("USB Device Disconnected");
    #endif
    
    // USB切断時にLEDを消灯
    digitalWrite(LED_PIN, LOW);
    
    // デバイス切断をディスプレイに表示
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("USB: Disconnected");
    display.setCursor(0, 20);
    display.println("Device removed!");
    display.display();
    
    // 状態更新
    lastUsbStatus = false;
    delay(1000); // 切断メッセージを1秒間表示
    updateDisplay();
  }
  
  // 生のUSBデータを表示するためのオーバーライド
  void onReceive(const usb_transfer_t *transfer) override {
    endpoint_data_t *endpoint_data = &endpoint_data_list[(transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_NUM_MASK)];

    // DOIO KB16キーボード用の特別処理
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
        // KB16固有のフォーマットを検出
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
          
          // キー押下イベントの検出
          bool anyKeyPressed = false;
          for (int i = 0; i < 6; i++) {
            if (report.keycode[i] != 0) {
              anyKeyPressed = true;
              break;
            }
          }
          
          // 押されているキーがない場合でも、修飾キーが変化した場合は検出
          if (!anyKeyPressed && report.modifier != last_report.modifier) {
            #if DEBUG_OUTPUT
            Serial.printf("修飾キー変更: 0x%02X -> 0x%02X\n", last_report.modifier, report.modifier);
            #endif
          }
        }
      }
    } 
    // 通常のデバイス（DOIO KB16以外）に対する処理
    else {
      #if USB_DEBUG_DETAIL
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

// キー入力を大きく表示する関数
void showKeyPress(char keyChar, uint8_t keycode) {
  display.clearDisplay();
  
  // ステータス行を表示（最上部）
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print("USB: ");
  if (usbHost.isReady) {
    display.println("Connected");
  } else {
    display.println("Not Connected");
  }
  
  // キー入力を大きく表示（中央部）
  display.setTextSize(3);
  
  if (keyChar == CHAR_ENTER) {
    // Enterキーの場合は特別な表示
    display.setCursor(20, 25);
    display.print("Enter");
  } else {
    display.setCursor(56, 25);
    display.print(keyChar);
  }
  
  // キーコードを16進数表示（調査用）
  display.setTextSize(1);
  display.setCursor(100, 0);
  display.printf("0x%02X", keycode);
  
  // 入力履歴を小さく表示（下部）
  display.setCursor(0, 56);
  // 最後の16文字だけ表示
  if (displayText.length() > 16) {
    display.print(displayText.substring(displayText.length() - 16));
  } else {
    display.print(displayText);
  }
  
  display.display();
}

// 通常のディスプレイ更新関数
void updateDisplay() {
  display.clearDisplay();
  
  // ステータス行を表示（最上部）
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("USB: ");
  if (usbHost.isReady) {
    display.println("Connected");
  } else {
    display.println("Not Connected");
  }
  
  // 入力テキストを表示（2行目以降）
  display.setCursor(0, 10);
  display.println(displayText);
  
  display.setCursor(0, 48);
  display.println("Ready for input...");
  
  display.display();
}

// ステータスのみ更新する関数
void updateStatusDisplay() {
  // 接続状態が変わったときだけフル更新
  if (lastUsbStatus != usbHost.isReady) {
    lastUsbStatus = usbHost.isReady;
    
    #if DEBUG_OUTPUT
    Serial.printf("USB Status changed: %s\n", usbHost.isReady ? "Connected" : "Not connected");
    #endif
    
    updateDisplay();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  // GPIO21をLEDピンとして設定
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // 初期状態はOFF
  
  #if DEBUG_OUTPUT
  Serial.println("Starting USB Host Controller...");
  #endif
  
  // I2C初期化
  Wire.begin();
  
  // SSD1306ディスプレイの初期化
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    #if DEBUG_OUTPUT
    Serial.println(F("SSD1306 allocation failed"));
    #endif
    for(;;); // 初期化失敗時は無限ループ
  }
  
  // ディスプレイの初期設定
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("USB Host Starting..."));
  display.display();
  delay(1000);
  
  // LEDテスト - 起動時に一度点滅させる
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  
  usbHost.begin();
  usbHost.setHIDLocal(HID_LOCAL_Japan_Katakana);
  
  // 初期状態を表示
  updateDisplay();
  
  #if DEBUG_OUTPUT
  Serial.println("USB Host initialized. Waiting for devices...");
  #endif
}

void loop() {
  usbHost.task();
  
  // LED自動消灯（キー入力から一定時間経過後）
  if (lastKeyPressTime > 0 && millis() - lastKeyPressTime > keyLedDuration) {
    digitalWrite(LED_PIN, LOW);
    lastKeyPressTime = 0;
    updateDisplay(); // 通常表示に戻す
  }
  
  // 定期的にステータスを更新
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusUpdate > statusUpdateInterval) {
    lastStatusUpdate = currentMillis;
    updateStatusDisplay();
  }
}
