# USB-BLE キーボード変換アダプター

## 概要
このプロジェクトは、USB接続のキーボードをBluetooth接続に変換するアダプターです。ESP32-S3マイコンを使用して、USBキーボードからの入力を受け取り、Bluetooth Low Energy (BLE)経由で他のデバイスに送信します。

## 主な機能
- USB接続のキーボードをBluetoothキーボードとして使用可能
- OLED画面でデバイスのステータスと入力内容を表示
- LEDによるキー入力の視覚的フィードバック
- 圧電スピーカーによるキーフィードバック音
- 一般的なHIDキーボードとDOIO KB16キーボード専用サポート

## ハードウェア
- ESP32-S3マイコン (Seeed Studio XIAO ESP32S3)
- 128x64 OLED ディスプレイ (SSD1306, I2C接続)
- LED表示 (GPIO: TBD)
- 圧電スピーカー (GPIO: TBD)

## ソフトウェア依存関係
- Arduino Framework
- Adafruit SSD1306 ライブラリ
- Adafruit GFX ライブラリ
- ESP32-BLE-Keyboard ライブラリ
- TinyUSB ライブラリ

## 使用方法
1. USBキーボードを本機器に接続
2. 電源を入れると自動的にBLEアドバタイジングを開始
3. 接続したいデバイス(PC、スマートフォンなど)でBluetooth設定から「DOIO Keyboard」を選択
4. 接続が確立するとOLEDディスプレイに接続状態が表示される
5. キーボードを通常通り使用可能

## 注意事項

### ファームウェア書き込み
ESP32-S3がUSBホストモードになると、通常のシリアル通信ができなくなります。ファームウェアを書き込む際は以下の手順に従ってください。

1. platformio.iniのUSB関連設定をコメントアウト:
```ini
build_flags =
    ; -D ARDUINO_USB_MODE=1
    ; -D CONFIG_USB_ENABLED=1
    ; -D CONFIG_TINYUSB_ENABLED=1
    ; -D CONFIG_TINYUSB_HID_ENABLED=1
```

2. ファームウェアを書き込む
3. 書き込み後、上記の設定のコメントを外して再ビルド

または、ESP32-S3をブートモードに入れる方法：
1. USBケーブルを抜く
2. BOOTボタン（またはIO0ボタン）を押しながらUSBケーブルを接続
3. BOOTボタンを数秒間押し続ける
4. ファームウェアを書き込む

### 対応キーボード
- 標準的なHIDキーボード
- DOIO KB16キーボード（専用サポート）
- その他多くのUSB HIDキーボード

## GPIO設定
| 機能 | GPIO番号 | 備考 |
|------|----------|------|
| 内蔵LED | 21 | キー入力表示用 |
| 拡張LED（赤） | 2 | 電源/Bluetooth状態表示用 |
| 圧電スピーカー | 1 | キークリック音・起動音 |
| OLED SDA | I2C標準 | ディスプレイデータライン |
| OLED SCL | I2C標準 | ディスプレイクロックライン |

## カスタマイズ
コード内の定数を変更することで、以下の動作をカスタマイズできます：
- Bluetoothデバイス名
- LEDの点灯時間
- ディスプレイの表示内容
- キーボード言語設定（日本語キーボード対応あり）

## ライセンス
MIT License

## 制作者
開発者名: TBD