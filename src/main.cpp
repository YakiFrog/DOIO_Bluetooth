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
  
  // キーボードイベントのオーバーライド
  void onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) override {
    // キー入力を文字として処理
    if (ascii >= 32 && ascii < 127) {  // 表示可能なASCII文字
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
    
    // デバッグ情報をシリアルモニターに出力
    Serial.printf("キー入力: ASCII=%c (0x%02x), キーコード=0x%02x, 修飾子=0x%02x\n", 
                  ascii, ascii, keycode, modifier);
  }
  
  // ディスプレイに現在のテキストを表示
  void displayText(Adafruit_ST7735 &tft) {
    if (!needsUpdate) return;
    
    // テキスト表示領域をクリア
    tft.fillRect(0, 60, tft.width(), 60, ST77XX_BLACK);
    
    // 入力されたテキストを表示
    tft.setCursor(0, 60);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.println("Keyboard Input:");
    
    tft.setCursor(0, 70);
    tft.setTextColor(ST77XX_GREEN);
    
    // 文字列が長い場合は最後の部分だけ表示
    const int MAX_DISPLAY_CHARS = 32;  // 表示可能な最大文字数
    char *displayText = textBuffer;
    
    if (bufferPos > MAX_DISPLAY_CHARS) {
      displayText = &textBuffer[bufferPos - MAX_DISPLAY_CHARS];
    }
    
    tft.println(displayText);
    
    // 入力統計を表示
    tft.setCursor(0, 90);
    tft.setTextColor(ST77XX_CYAN);
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
  
  // 初期テキストを表示
  display.tft.setCursor(0, 60);
  display.tft.setTextColor(ST77XX_WHITE);
  display.tft.println("Keyboard Input:");
  display.tft.setCursor(0, 70);
  display.tft.setTextColor(ST77XX_GREEN);
  display.tft.println("Waiting for keyboard...");
}

void loop() {
  // USBホストのタスクを実行
  keyboardHost.task();
  
  // メモリ情報と入力テキストの定期更新
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {  // 1秒ごとに更新
    lastUpdate = millis();
    
    // メモリ情報を表示
    // display.showMemoryInfo();
    
    // キーボード入力を表示
    keyboardHost.displayText(display.tft);
  }
}