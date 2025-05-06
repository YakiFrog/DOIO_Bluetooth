#include "DisplayController.h"
#include "Peripherals.h"

// グローバルインスタンス
DisplayController displayController;

// 特殊キーの表示名を取得する関数
const char* DisplayController::getSpecialKeyName(uint8_t keycode) {
    static char unknownKey[8];
    
    switch (keycode) {
        case 0x28: return "Enter";
        case 0x29: return "Esc";
        case 0x2A: return "BS";
        case 0x2B: return "Tab";
        case 0x2C: return "Space";
        case 0x4F: return "Right";
        case 0x50: return "Left";
        case 0x51: return "Down";
        case 0x52: return "Up";
        case 0x39: return "Caps";
        case 0x3A: return "F1";
        case 0x3B: return "F2";
        case 0x3C: return "F3";
        case 0x3D: return "F4";
        case 0x3E: return "F5";
        case 0x3F: return "F6";
        case 0x40: return "F7";
        case 0x41: return "F8";
        case 0x42: return "F9";
        case 0x43: return "F10";
        case 0x44: return "F11";
        case 0x45: return "F12";
        case 0x46: return "PrtSc";
        case 0x47: return "ScrLk";
        case 0x48: return "Pause";
        case 0x49: return "Ins";
        case 0x4A: return "Home";
        case 0x4B: return "PgUp";
        case 0x4C: return "Del";
        case 0x4D: return "End";
        case 0x4E: return "PgDn";
        case 0x53: return "NumLk";
        // テンキー
        case 0x54: return "Num/";
        case 0x55: return "Num*";
        case 0x56: return "Num-";
        case 0x57: return "Num+";
        case 0x58: return "NumEnt";
        case 0x59: return "Num1";
        case 0x5A: return "Num2";
        case 0x5B: return "Num3";
        case 0x5C: return "Num4";
        case 0x5D: return "Num5";
        case 0x5E: return "Num6";
        case 0x5F: return "Num7";
        case 0x60: return "Num8";
        case 0x61: return "Num9";
        case 0x62: return "Num0";
        case 0x63: return "Num.";
        // 日本語キー
        case 0x87: return "\\/_";
        case 0x88: return "カナ";
        case 0x89: return "¥";
        case 0x8A: return "変換";
        case 0x8B: return "無変換";
        // メディアキー
        case 0xE2: return "Mute";
        case 0xE9: return "Vol+";
        case 0xEA: return "Vol-";
        case 0xB5: return "Next";
        case 0xB6: return "Prev";
        case 0xB7: return "Stop";
        case 0xCD: return "Play";
        default:
            snprintf(unknownKey, sizeof(unknownKey), "0x%02X", keycode);
            return unknownKey;
    }
}

void DisplayController::begin() {
    // I2C初期化はmain.cppで行うため、ここでは行わない
    
    // SSD1306ディスプレイの初期化
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        #if DEBUG_OUTPUT
        Serial.println(F("SSD1306 allocation failed"));
        #endif
        return;
    }
    
    // ディスプレイの初期設定
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("USB-BLE Keyboard"));
    display.println(F("Initializing..."));
    display.display();
}

void DisplayController::updateDisplay() {
    display.clearDisplay();
    
    // ステータス行を表示（最上部）
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("USB: ");
    if (usbConnected) {
        display.print("Con ");
    } else {
        display.print("-- ");
    }
    
    // BLEステータスを表示
    display.print("BLE: ");
    if (bleConnected) {
        display.println("Con");
    } else {
        display.println("Wait");
    }
    
    // 入力テキストを表示（2行目以降）
    display.setCursor(0, 10);
    display.println(displayText);
    
    display.setCursor(0, 48);
    display.println("Ready for input...");
    
    display.display();
}

void DisplayController::updateStatusDisplay() {
    // 最上部のステータス行のみを更新する軽量版表示更新
    display.fillRect(0, 0, SCREEN_WIDTH, 10, SSD1306_BLACK);
    
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("USB: ");
    if (usbConnected) {
        display.print("Con ");
    } else {
        display.print("-- ");
    }
    
    // BLEステータスを表示
    display.print("BLE: ");
    if (bleConnected) {
        display.println("Con");
    } else {
        display.println("Wait");
    }
    
    display.display();
}

void DisplayController::showKeyPress(char keyChar, uint8_t keycode) {
    display.clearDisplay();
    
    // ステータス行を表示（最上部）
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("USB: ");
    if (usbConnected) {
        display.print("Con ");
    } else {
        display.print("-- ");
    }
    
    // BLEステータスを表示
    display.print("BLE: ");
    if (bleConnected) {
        display.println("Con");
    } else {
        display.println("Wait");
    }
    
    // キーコードを16進数表示（常に表示）
    display.setCursor(100, 0);
    display.printf("0x%02X", keycode);
    
    // キー入力を大きく表示（中央部）
    display.setTextSize(2);  // サイズを少し小さく調整
    
    // キーコードから特殊キーの表示名を取得
    const char* specialKeyName = getSpecialKeyName(keycode);
    
    // 印字可能文字の場合
    if (' ' <= keyChar && keyChar <= '~') {
        display.setCursor(56, 25);
        display.print(keyChar);
    }
    // 特殊キーの場合
    else if (keyChar == CHAR_ENTER || keyChar == CHAR_LEFT || 
             keyChar == CHAR_RIGHT || keyChar == CHAR_UP || 
             keyChar == CHAR_DOWN || specialKeyName[0] != '0') {
        // 特殊キー名を中央に表示
        int16_t nameLen = strlen(specialKeyName) * 12;  // 大まかな幅を計算
        int16_t xPos = (SCREEN_WIDTH - nameLen) / 2;
        if (xPos < 0) xPos = 0;
        
        display.setCursor(xPos, 25);
        display.print(specialKeyName);
    }
    else {
        // 未知のキーも必ず表示 (バイナリデータとして)
        char hexStr[8];
        snprintf(hexStr, sizeof(hexStr), "0x%02X", keycode);
        
        display.setCursor(20, 25);
        display.print("Key:");
        display.print(hexStr);
    }
    
    // 入力履歴を小さく表示（下部）
    display.setTextSize(1);
    display.setCursor(0, 56);
    // 最後の16文字だけ表示
    if (displayText.length() > 16) {
        display.print(displayText.substring(displayText.length() - 16));
    } else {
        display.print(displayText);
    }
    
    display.display();
}

void DisplayController::showDeviceInfo(const String& manufacturer, const String& productName, 
                                     uint16_t idVendor, uint16_t idProduct) {
    deviceName = productName;
    vendorId = idVendor;
    productId = idProduct;
    
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
}

void DisplayController::setUsbConnected(bool connected) {
    if (usbConnected != connected) {
        usbConnected = connected;
        updateStatusDisplay();
    }
}

void DisplayController::setBleConnected(bool connected) {
    if (bleConnected != connected) {
        bleConnected = connected;
        updateStatusDisplay();
    }
}

void DisplayController::addDisplayText(char c) {
    if (c == '\r' || c == '\n') {
        displayText += '\n';
    } else {
        displayText += c;
    }
    
    // 表示テキストが長すぎる場合は切り詰める
    if (displayText.length() > maxChars) {
        displayText = displayText.substring(displayText.length() - maxChars);
    }
}

void DisplayController::clearDisplayText() {
    displayText = "";
}