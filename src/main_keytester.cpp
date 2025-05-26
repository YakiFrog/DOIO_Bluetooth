/**
 * @file main_keytester.cpp
 * @brief DOIO KB16キーテスター用のメインファイル
 */

#include <Arduino.h>
#include "display.h"
#include "EspUsbHost.h"
#include "kb16_keytester.h"

// ディスプレイクラスのインスタンス化
Display display;

// KB16キーボードホスト（USBホストの拡張）
class KB16KeyboardHost : public EspUsbHost {
public:
    // キーテスターへの参照
    KB16KeyTester* keyTester = nullptr;
    
    // デバイス情報を保存するための変数
    String deviceManufacturer = "";
    String deviceProduct = "";
    String deviceSerialNum = "";
    bool deviceConnected = false;
    bool isDOIOKeyboard = false;
    
    // EspUsbHostのコールバックをオーバーライドして、デバイス情報を取得
    void onNewDevice(const usb_device_info_t &dev_info) override {
        deviceManufacturer = getUsbDescString(dev_info.str_desc_manufacturer);
        deviceProduct = getUsbDescString(dev_info.str_desc_product);
        deviceSerialNum = getUsbDescString(dev_info.str_desc_serial_num);
        deviceConnected = true;
        
        // デバイスIDの取得（DOIOキーボード検出用）
        if (device_vendor_id == 0xD010 && device_product_id == 0x1601) {
            isDOIOKeyboard = true;
            Serial.println("DOIO KB16キーボードを検出しました！");
        } else {
            isDOIOKeyboard = false;
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

        // キーテスター画面を描画
        if (keyTester != nullptr) {
            keyTester->drawKeyTester();
            
            // 画面に接続情報も表示
            if (isDOIOKeyboard) {
                // DOIO KB16キーボードであることを表示
                display.tft.fillRect(0, 12, display.tft.width(), 6, KB16KeyTester::COLOR_BG);
                display.tft.setCursor(2, 12);
                display.tft.print("DOIO KB16 Ready");
            } else {
                // 他のキーボード
                display.tft.fillRect(0, 12, display.tft.width(), 6, KB16KeyTester::COLOR_BG);
                display.tft.setCursor(2, 12);
                display.tft.print("Keyboard Ready");
            }
        }
    }
    
    // デバイスが取り外されたときの処理
    void onGone(const usb_host_client_event_msg_t *eventMsg) override {
        deviceConnected = false;
        deviceManufacturer = "";
        deviceProduct = "";
        deviceSerialNum = "";
        isDOIOKeyboard = false;
        
        Serial.println("キーボードが取り外されました");

        // キーをリセット
        if (keyTester != nullptr) {
            keyTester->resetKeys();
            
            // キーボード未接続状態を画面に表示
            display.tft.fillRect(0, 12, display.tft.width(), 6, KB16KeyTester::COLOR_BG);
            display.tft.setCursor(2, 12);
            display.tft.print("No KB connected");
        }
    }
    
    // キーボードイベントのオーバーライド
    void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
        // 押されたキーを処理
        for (int i = 0; i < 6; i++) {
            uint8_t keycode = report.keycode[i];
            if (keycode != 0) {
                // キーが押された時にキーテスターに通知
                if (keyTester != nullptr) {
                    keyTester->updateKey(keycode, true);
                }
                
                // シリアル出力
                uint8_t ascii = getKeycodeToAscii(keycode, 
                    (report.modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)));
                Serial.printf("キー押下: ASCII=%c (0x%02x), キーコード=0x%02x, 修飾子=0x%02x\n", 
                    ascii, ascii, keycode, report.modifier);
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
                    // キーが離された時にキーテスターに通知
                    if (keyTester != nullptr) {
                        keyTester->updateKey(last_key, false);
                    }
                    
                    Serial.printf("キー離し: キーコード=0x%02x\n", last_key);
                }
            }
        }
    }
    
    // DOIO KB16キー状態変更通知のオーバーライド
    void onKB16KeyStateChanged(uint8_t row, uint8_t col, bool pressed) override {
        Serial.printf("DOIO KB16 キーボード: キー (%d,%d) %s\n", 
                    row, col, pressed ? "押下" : "離し");
        
        // DOIOキーボード専用のハイライト処理
        if (keyTester != nullptr) {
            // 直接マトリックス位置情報を使ってキーをハイライト
            keyTester->updateKeyPosition(row, col, pressed);
            
            // デバッグ用に実際のキー位置情報を出力
            static const char KEY_CHARS[4][4] = {
                {'A', 'B', 'C', 'D'},
                {'E', 'F', 'G', 'H'},
                {'I', 'J', 'K', 'L'},
                {'M', 'N', 'O', 'P'}
            };
            if (row < 4 && col < 4) {
                Serial.printf("マトリックス位置: (%d,%d) -> %c\n", row, col, KEY_CHARS[row][col]);
            }
        }
    }
};

// USBホストのインスタンス化
KB16KeyboardHost keyboardHost;

// キーテスターのインスタンス
KB16KeyTester keyTester(display.tft, keyboardHost);

void setup(void) {
    // シリアル通信の初期化
    Serial.begin(115200);
    delay(1000); // 起動時の安定化待機

    Serial.println(F("DOIO KB16 キーテスター - ESP32"));

    // ディスプレイの初期化
    display.init();
    
    // キーボードホストの設定
    keyboardHost.keyTester = &keyTester;
    
    // キーボードの初期化
    keyboardHost.begin();
    
    // キーテスターの初期化
    keyTester.begin();
}

void loop() {
    // USBホストのタスクを実行
    keyboardHost.task();
    
    // キーテスターの更新
    keyTester.update();
}
