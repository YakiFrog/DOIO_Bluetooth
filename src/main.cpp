#include <Arduino.h>
#include "display.h"
#include "EspUsbHost.h"

// ディスプレイクラスのインスタンス化
Display display;

// キーボード入力を管理するクラス
class KeyboardDisplay : public EspUsbHost {
public:
  // キーボード入力用の簡易バッファ
  static const int BUFFER_SIZE = 100;
  char textBuffer[BUFFER_SIZE];
  int bufferPos = 0;
  
  // タッチタイプ情報
  int totalKeystrokes = 0;
  int correctKeystrokes = 0;
  
  // 入力表示のための変数
  bool needsUpdate = false;
  
  // デバイス情報を保存するための変数
  String deviceManufacturer = "";
  String deviceProduct = "";
  String deviceSerialNum = "";
  bool deviceConnected = false;
  
  // キーボードの状態を保持するための変数
  uint8_t lastKeycode = 0;          // 最後に押されたキー
  unsigned long keyPressStartTime = 0; // キーが押された時間
  bool isKeyRepeat = false;         // 長押し状態かどうか
  unsigned long keyRepeatDelay = 500;  // 長押し認識までの遅延（ミリ秒）
  unsigned long keyRepeatRate = 100;   // 長押し時のリピート間隔（ミリ秒）
  unsigned long lastRepeatTime = 0;    // 最後にリピート処理した時間
  
  // キーの現在の状態を記録する変数
  bool keyState[256] = {false}; // すべてのキーコードに対応するための配列
  
  // DOIOキーボード用のキー表示
  int lastPressedKeyRow = -1;
  int lastPressedKeyCol = -1;
  bool showDOIOKeyPosition = false; // DOIOキーボードのキー位置表示モード
  
  // EspUsbHostのコールバックをオーバーライドして、デバイス情報を取得
  void onNewDevice(const usb_device_info_t &dev_info) {
    deviceManufacturer = getUsbDescString(dev_info.str_desc_manufacturer);
    deviceProduct = getUsbDescString(dev_info.str_desc_product);
    deviceSerialNum = getUsbDescString(dev_info.str_desc_serial_num);
    deviceConnected = true;
    needsUpdate = true; // 画面更新のフラグを立てる
    
    // デバイスIDの取得（DOIOキーボード検出用）
    if (device_vendor_id == 0xD010 && device_product_id == 0x1601) {
      showDOIOKeyPosition = true; // DOIOキーボードモードをオン
      Serial.println("DOIO KB16キーボードを検出しました！");
    } else {
      showDOIOKeyPosition = false; // 通常モード
    }
    
    Serial.println("新しいキーボード接続:");
    Serial.print("製造元: "); 
    if (deviceManufacturer.length() > 0) {
      Serial.println(deviceManufacturer);
    } else {
      Serial.println("情報なし");
    }
    
    Serial.print("製品名: ");
    if (deviceProduct.length() > 0) {
      Serial.println(deviceProduct);
    } else {
      Serial.println("情報なし");
    }
    
    Serial.print("シリアル番号: ");
    if (deviceSerialNum.length() > 0) {
      Serial.println(deviceSerialNum);
    } else {
      Serial.println("情報なし");
    }
    
    // デバイスの詳細情報も表示
    Serial.printf("デバイス速度: %d\n", dev_info.speed);
    Serial.printf("デバイスアドレス: %d\n", dev_info.dev_addr);
    Serial.printf("最大パケットサイズ: %d\n", dev_info.bMaxPacketSize0);
    Serial.printf("コンフィグ値: %d\n", dev_info.bConfigurationValue);
  }
  
  // デバイスが取り外されたときの処理
  void onGone(const usb_host_client_event_msg_t *eventMsg) override {
    deviceConnected = false;
    deviceManufacturer = "";
    deviceProduct = "";
    deviceSerialNum = "";
    showDOIOKeyPosition = false; // DOIOモードをリセット
    lastPressedKeyRow = -1;
    lastPressedKeyCol = -1;
    needsUpdate = true;
    
    Serial.println("キーボードが取り外されました");
  }
  
  // キーボードイベントのオーバーライド
  void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
    // 押されているキーを確認し、状態を更新
    for (int i = 0; i < 6; i++) {
      uint8_t keycode = report.keycode[i];
      
      // キーコードが有効で、新しく押されたキーの場合
      if (keycode != 0 && !keyState[keycode]) {
        uint8_t ascii = getKeycodeToAscii(keycode, 
                       (report.modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)));
                       
        Serial.printf("キー押下: ASCII=%c (0x%02x), キーコード=0x%02x, 修飾子=0x%02x\n", 
                    ascii, ascii, keycode, report.modifier);
                    
        // キー状態を「押された」に設定
        keyState[keycode] = true;
        lastKeycode = keycode;
        keyPressStartTime = millis();
        isKeyRepeat = false;
        
        // キー入力を処理
        processKey(ascii, keycode, report.modifier);
      }
      // キーが離された場合、状態をリセット
      else if (keycode == 0) {
        // 何もしない（last_reportのチェックで離されたキーを処理）
      }
    }
    
    // 離されたキーのチェック - last_reportにあって現在のreportにないキーを探す
    for (int i = 0; i < 6; i++) {
      uint8_t last_key = last_report.keycode[i];
      if (last_key != 0) {
        // キーがまだ押されているか確認
        bool still_pressed = false;
        for (int j = 0; j < 6; j++) {
          if (report.keycode[j] == last_key) {
            still_pressed = true;
            break;
          }
        }
        
        // キーが離された場合
        if (!still_pressed) {
          keyState[last_key] = false;
          
          // 長押し状態のリセット（このキーが長押しされていた場合）
          if (lastKeycode == last_key) {
            lastKeycode = 0;
            isKeyRepeat = false;
          }
          
          Serial.printf("キー離し: キーコード=0x%02x\n", last_key);
        }
      }
    }
  }
  
  // 元のonKeyboardKeyメソッドは無効化（使わない）
  void onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) override {
    // 使用せず、onKeyboardで処理
  }
  
  // DOIO KB16キー状態変更通知のオーバーライド
  void onKB16KeyStateChanged(uint8_t row, uint8_t col, bool pressed) override {
    if (pressed) {
      // キーが押されたとき、その位置を記録
      lastPressedKeyRow = row;
      lastPressedKeyCol = col;
      
      // キー番号を計算 (0~15)
      int keyNumber = row * 4 + col;
      
      // テキストバッファに追加
      char keyText[10];
      sprintf(keyText, "Key: %d", keyNumber);
      
      // バッファをクリアして新しい情報を表示
      bufferPos = 0;
      
      // キー位置をテキストバッファに設定
      int len = strlen(keyText);
      for (int i = 0; i < len && i < BUFFER_SIZE - 1; i++) {
        textBuffer[bufferPos++] = keyText[i];
      }
      textBuffer[bufferPos] = '\0';
      
      // DOIOキーボードモードをオン
      showDOIOKeyPosition = true;
      needsUpdate = true;
      
      Serial.printf("DOIO キーボード: キー %d (%d,%d) 押下\n", keyNumber, row, col);
    }
  }
  
  // 長押し状態を更新する関数（loop内で呼び出し）
  void updateKeyRepeat() {
    // キーが押されている状態で
    if (lastKeycode != 0 && keyState[lastKeycode]) {  // キーが実際に押されているか確認
      unsigned long currentTime = millis();
      
      // 長押し開始判定
      if (!isKeyRepeat && (currentTime - keyPressStartTime > keyRepeatDelay)) {
        isKeyRepeat = true;
        lastRepeatTime = currentTime;
        Serial.printf("長押し開始: キーコード=0x%02x\n", lastKeycode);
      }
      
      // 長押し中のリピート処理
      if (isKeyRepeat && (currentTime - lastRepeatTime > keyRepeatRate)) {
        lastRepeatTime = currentTime;
        
        // バックスペースキーの長押し
        if (lastKeycode == 0x2A && bufferPos > 0) {
          bufferPos--;
          textBuffer[bufferPos] = '\0';
          needsUpdate = true;
          Serial.println("バックスペース長押し処理");
        }
        // 文字キーの長押し - 表示可能なASCII文字の場合は繰り返し入力
        else if (lastKeycode >= 0x04 && lastKeycode <= 0x1D) {  // アルファベットキー
          // HIDキーコードからASCII文字を取得
          uint8_t ascii = getKeycodeToAscii(lastKeycode, 0); // シフトなしの場合
          if (ascii >= 32 && ascii < 127) {
            Serial.printf("文字キー長押し処理: %c\n", ascii);
            processKey(ascii, lastKeycode, 0);
          }
        }
        // その他の長押し対応キー（必要に応じて追加）
        // ...
      }
    }
    else if (lastKeycode != 0 && !keyState[lastKeycode]) {
      // キーが物理的に離されたのにlastKeycodeがリセットされていない場合の対策
      lastKeycode = 0;
      isKeyRepeat = false;
    }
  }
  
  // 実際のキー処理を行う関数
  void processKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) {
    // エンターキー処理（改行として扱う）
    if (keycode == 0x28) {
      // バッファクリア - エンターを改行ではなくクリアとして扱う
      bufferPos = 0;
      textBuffer[bufferPos] = '\0';
      needsUpdate = true;
      return;
    }
                  
    // 表示可能なASCII文字の処理
    if (ascii >= 32 && ascii < 127) {
      // バッファが満杯なら先頭の文字を削除
      if (bufferPos >= BUFFER_SIZE - 1) {
        for (int i = 0; i < BUFFER_SIZE - 1; i++) {
          textBuffer[i] = textBuffer[i + 1];
        }
        bufferPos--;
      }
      
      // バッファに文字を追加
      textBuffer[bufferPos] = ascii;
      bufferPos++;
      textBuffer[bufferPos] = '\0'; // NULL終端
      
      totalKeystrokes++;
      needsUpdate = true;
    }
    // バックスペースの処理
    else if (keycode == 0x2A && bufferPos > 0) {  // バックスペースキー
      bufferPos--;
      textBuffer[bufferPos] = '\0';
      needsUpdate = true;
    }
  }
  
  // ディスプレイに現在のテキストを表示
  void displayText(Adafruit_ST7735 &tft) {
    if (!needsUpdate) return;
    
    // 画面全体をクリア
    tft.fillScreen(ST77XX_BLACK);
    
    // タイトルを表示（小さいサイズで）
    tft.setCursor(0, 5);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);  // サイズを1に縮小
    tft.println("USB Keyboard");
    
    // 区切り線（タイトルが小さくなったので上に移動）
    tft.drawLine(0, 15, tft.width(), 15, ST77XX_YELLOW);
    
    // デバイス情報または待機メッセージの表示
    tft.setCursor(0, 20);
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(1);
    
    if (deviceConnected) {
      // 接続されたキーボードの情報を表示
      tft.print("Mfr: ");
      // 製造元情報が空の場合は「不明」と表示
      String mfrText = deviceManufacturer.length() > 0 ? deviceManufacturer : "Unknown";
      tft.setTextColor(ST77XX_CYAN);
      tft.println(mfrText.length() > 10 ? mfrText.substring(0, 10) + "..." : mfrText);
      
      tft.setCursor(0, 30);
      tft.print("Dev: ");
      // 製品名情報が空の場合は「不明」と表示
      String devText = deviceProduct.length() > 0 ? deviceProduct : "Unknown";
      tft.setTextColor(ST77XX_CYAN);
      tft.println(devText.length() > 10 ? devText.substring(0, 10) + "..." : devText);
    } else {
      tft.println("Waiting for keyboard...");
    }
    
    // DOIO KB16キーボードの場合は特別表示
    if (deviceConnected && (device_vendor_id == 0xD010 && device_product_id == 0x1601) && showDOIOKeyPosition) {
      // DOIOキーボード用の表示
      tft.setCursor(0, 45);
      tft.setTextColor(ST77XX_GREEN);
      tft.println("DOIO KB16 Layout:");

      // 4x4マトリックスを表示
      for (int row = 0; row < 4; row++) {
        tft.setCursor(10, 60 + row * 15);
        for (int col = 0; col < 4; col++) {
          int keyNum = row * 4 + col;
          
          // 最後に押されたキーは別の色で表示
          if (row == lastPressedKeyRow && col == lastPressedKeyCol) {
            tft.setTextColor(ST77XX_RED);
            tft.print(keyNum);
            tft.setTextColor(ST77XX_GREEN);
          } else {
            tft.print(keyNum);
          }
          
          // 間隔をあける
          if (col < 3) tft.print(" ");
        }
      }
      
      // 現在選択されているキー番号の大きな表示
      if (lastPressedKeyRow >= 0 && lastPressedKeyCol >= 0) {
        int keyNumber = lastPressedKeyRow * 4 + lastPressedKeyCol;
        tft.setCursor(20, tft.height() - 60);
        tft.setTextColor(ST77XX_YELLOW);
        tft.setTextSize(3);
        tft.print("Key:");
        tft.print(keyNumber);
      }
    } else {
      // 通常のキーボード表示
      
      // 入力されたテキスト表示のヘッダー
      tft.setCursor(0, 45);
      tft.println("Input:");
      
      // 入力されたテキストを表示（1行に修正）
      tft.setCursor(0, 55);  // 表示位置の調整
      tft.setTextColor(ST77XX_GREEN);
      tft.setTextSize(2);  // テキストサイズを大きく
      
      // 文字列が長い場合は最後の部分だけ表示
      const int MAX_DISPLAY_CHARS = 8;  // 1行表示で画面に収まる文字数
      char displayBuffer[MAX_DISPLAY_CHARS + 1]; // +1 はNULL終端用
      
      // 表示するテキストを決定
      if (bufferPos <= MAX_DISPLAY_CHARS) {
        // バッファ全体が表示できる場合
        strcpy(displayBuffer, textBuffer);
      } else {
        // 長い場合は最後の部分だけ表示
        strcpy(displayBuffer, &textBuffer[bufferPos - MAX_DISPLAY_CHARS]);
      }
      
      // テキストを1行で表示
      tft.print(displayBuffer);
      
      // カーソルを表示
      if ((millis() / 500) % 2 == 0) {  // 500ms間隔で点滅
        tft.print("_");
      }
    }
    
    // 入力統計を表示
    tft.setCursor(0, tft.height() - 20);
    tft.setTextColor(ST77XX_MAGENTA);
    tft.setTextSize(1);
    tft.print("Keys: ");
    tft.print(totalKeystrokes);
    
    // 更新完了
    needsUpdate = false;
  }
};

// USBホストのインスタンス化
KeyboardDisplay keyboardHost;

void setup(void) {
  // シリアル通信の初期化
  Serial.begin(115200);
  delay(1000); // 起動時の安定化待機

  Serial.println(F("ESP32 USBキーボードモニター - XIAO ESP32S3"));

  // ディスプレイの初期化
  display.init();
  
  // キーボードの初期化
  keyboardHost.begin();
  
  // 初期画面を表示
  display.tft.fillScreen(ST77XX_BLACK);
  
  // タイトル（小さいサイズで）
  display.tft.setCursor(0, 5);
  display.tft.setTextColor(ST77XX_WHITE);
  display.tft.setTextSize(1);  // サイズを1に縮小
  display.tft.println("USB Keyboard");
  
  // 区切り線（タイトルが小さくなったので上に移動）
  display.tft.drawLine(0, 15, display.tft.width(), 15, ST77XX_YELLOW);
  
  // 説明
  display.tft.setCursor(0, 20);
  display.tft.setTextColor(ST77XX_CYAN);
  display.tft.setTextSize(1);
  display.tft.println("Waiting for keyboard...");
  
  // キーボードバッファの初期化
  keyboardHost.textBuffer[0] = '\0';
}

void loop() {
  // USBホストのタスクを実行（高速で応答するために優先的に呼び出し）
  keyboardHost.task();
  
  // 長押し状態の更新
  keyboardHost.updateKeyRepeat();
  
  // キーボード入力の表示を更新（レスポンス向上のため間隔を短く）
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 50) {  // 50msごとに更新（20fps相当）
    lastUpdate = millis();
    
    // キーボード入力が変更されたら表示更新
    if (keyboardHost.needsUpdate) {
      keyboardHost.displayText(display.tft);
    }
  }
  
  // カーソル点滅のために定期的に更新
  static unsigned long lastCursorUpdate = 0;
  if (millis() - lastCursorUpdate > 500) {  // 500msごとにカーソル点滅
    lastCursorUpdate = millis();
    keyboardHost.needsUpdate = true;
  }
}