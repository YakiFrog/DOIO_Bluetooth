#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ディスプレイの設定
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// 特殊文字の定数定義
#define CHAR_ENTER     'E'  // Enter key symbol
#define CHAR_LEFT      '<'  // Left arrow
#define CHAR_RIGHT     '>'  // Right arrow
#define CHAR_DOWN      'v'  // Down arrow
#define CHAR_UP        '^'  // Up arrow

// ディスプレイ操作を管理するクラス
class DisplayController {
public:
    // 初期化
    void begin();
    
    // 表示の更新
    void updateDisplay();
    void updateStatusDisplay();
    void showKeyPress(char keyChar, uint8_t keycode);
    
    // 生のキーコードを表示するための特別なメソッド
    void showRawKeyCode(uint8_t keycode, const char* description);
    
    // デバイス情報の更新
    void showDeviceInfo(const String& manufacturer, const String& productName, 
                      uint16_t idVendor, uint16_t idProduct);
    
    // 接続状態の設定
    void setUsbConnected(bool connected);
    void setBleConnected(bool connected);
    
    // 表示テキストの追加
    void addDisplayText(char c);
    void clearDisplayText();
    
private:
    Adafruit_SSD1306 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    
    // 表示用のテキストバッファ
    String displayText = "";
    const int maxChars = 100;
    
    // 接続ステータス
    bool usbConnected = false;
    bool bleConnected = false;
    
    // デバイス情報
    String deviceName = "None";
    uint16_t vendorId = 0;
    uint16_t productId = 0;
};

// グローバルインスタンス
extern DisplayController displayController;

#endif // DISPLAY_CONTROLLER_H