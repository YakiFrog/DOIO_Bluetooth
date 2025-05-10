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
  
  // キーボードの状態を保持するための変数
  uint8_t lastKeycode = 0;          // 最後に押されたキー
  unsigned long keyPressStartTime = 0; // キーが押された時間
  bool isKeyRepeat = false;         // 長押し状態かどうか
  unsigned long keyRepeatDelay = 500;  // 長押し認識までの遅延（ミリ秒）
  unsigned long keyRepeatRate = 100;   // 長押し時のリピート間隔（ミリ秒）
  unsigned long lastRepeatTime = 0;    // 最後にリピート処理した時間
  
  // キーの現在の状態を記録する変数
  bool keyState[256] = {false}; // すべてのキーコードに対応するための配列
  
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
    
    // タイトルを表示
    tft.setCursor(0, 5);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.println("USB Keyboard");
    
    // 区切り線
    tft.drawLine(0, 25, tft.width(), 25, ST77XX_YELLOW);
    
    // 入力されたテキスト表示のヘッダー
    tft.setCursor(0, 30);
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(1);
    tft.println("Input:");
    
    // 入力されたテキストを表示（1行に修正）
    tft.setCursor(0, 50);  // 表示位置を中央付近に調整
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
  
  // タイトル
  display.tft.setCursor(0, 5);
  display.tft.setTextColor(ST77XX_WHITE);
  display.tft.setTextSize(2);
  display.tft.println("USB Keyboard");
  
  // 区切り線
  display.tft.drawLine(0, 25, display.tft.width(), 25, ST77XX_YELLOW);
  
  // 説明
  display.tft.setCursor(0, 35);
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