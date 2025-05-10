#include "display.h"

// ディスプレイクラスのインスタンス化
Display display;

void setup(void) {
  // シリアル通信の初期化
  Serial.begin(115200);
  delay(1000); // 起動時の安定化待機

  Serial.println(F("ESP32 メモリモニター - XIAO ESP32S3"));

  // ディスプレイの初期化
  display.init();
}

void loop() {
  // メモリ情報の定期更新
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 3000) {  // 3秒ごとに更新
    lastUpdate = millis();
    
    // メモリ情報を表示
    display.showMemoryInfo();
  }
}