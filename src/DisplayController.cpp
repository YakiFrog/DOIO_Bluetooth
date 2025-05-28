#include "DisplayController.h"
#include "Peripherals.h"

// グローバルインスタンス
DisplayController displayController;

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

// 生のキーコードを表示する専用メソッド（未知のキーや特殊キー用）
void DisplayController::showRawKeyCode(uint8_t keycode, const char* description) {
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
    
    // キーコードを中央に大きく表示
    display.setTextSize(2);
    display.setCursor(10, 16);
    display.printf("0x%02X", keycode);
    
    // 説明テキストを表示
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.println(description);
    
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

// 起動遅延モードの表示関数
void DisplayController::showProgrammingMode() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // "Programming Mode" を中央表示
    display.setCursor(10, 15);
    display.println("Programming Mode");
    display.setCursor(20, 30);
    display.println("USB Write Mode");
    display.display();
}

void DisplayController::showCountdown(int seconds) {
    // カウントダウン表示エリアをクリア
    display.fillRect(0, 45, 128, 20, SSD1306_BLACK);
    
    String countText = "Start in " + String(seconds) + "s";
    display.setCursor(30, 50);
    display.print(countText);
    display.display();
}

void DisplayController::showUsbHostModeActivated() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(20, 20);
    display.println("USB Host Mode");
    display.setCursor(25, 35);
    display.println("Activated");
    display.display();
}