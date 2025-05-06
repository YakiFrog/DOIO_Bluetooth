# USB-BLE キーボード変換アダプター

## 概要
このプロジェクトは、USB接続のキーボードをBluetooth接続に変換するアダプターです。ESP32-S3マイコンを使用して、USBキーボードからの入力を受け取り、Bluetooth Low Energy (BLE)経由で他のデバイスに送信します。

## 主な機能
- USB接続のキーボードをBluetoothキーボードとして使用可能
- OLED画面でデバイスのステータスと入力内容を表示
- LEDによるシステム状態表示（電源、Bluetooth接続状態）
- キー入力時のLED表示フィードバック
- 圧電スピーカーによる起動音、接続音、キークリック音
- 一般的なHIDキーボードとDOIO KB16キーボード専用サポート

## ハードウェア
- ESP32-S3マイコン (Seeed Studio XIAO ESP32S3)
- 128x64 OLED ディスプレイ (SSD1306, I2C接続)
- 赤色LED (電源/Bluetooth接続状態表示用)
- 圧電スピーカー (起動音/キークリック音用)

## ソフトウェア依存関係
- Arduino Framework
- Adafruit SSD1306 ライブラリ
- Adafruit GFX ライブラリ
- ESP32-BLE-Keyboard ライブラリ
- TinyUSB ライブラリ

## ファイル構造
- main.cpp - メインプログラム（キーボード入力処理、BLE送信、全体制御）
- DisplayController.h/.cpp - OLED表示管理クラス
- Peripherals.h/.cpp - LED制御とスピーカー制御クラス
- EspUsbHost.h/.cpp - USB HID処理クラス

## 使用方法
1. USBキーボードを本機器に接続
2. 電源を入れると自動的にBLEアドバタイジングを開始、起動メロディが再生されます
3. 接続したいデバイス(PC、スマートフォンなど)でBluetooth設定から「DOIO Keyboard」を選択
4. 接続が確立すると接続音が鳴り、状態LEDが点灯状態になります
5. OLEDディスプレイに接続状態が表示され、入力されたキーが表示されます
6. キーボードを通常通り使用可能になります

## LED・スピーカー動作
- **内蔵LED (GPIO 21)**: キー入力時に一時的に点灯
- **外部LED (GPIO 2)**:
  - 電源投入時：点灯
  - Bluetooth接続時：常時点灯
  - Bluetooth未接続時：点滅
- **圧電スピーカー (GPIO 1)**:
  - 起動時：起動メロディ（C-E-G-Cの上昇音階）
  - キー入力時：短いクリック音
  - Bluetooth接続時：接続音（上昇音）
  - Bluetooth切断時：切断音（下降音）

## システム状態遷移図

```mermaid
stateDiagram-v2
    [*] --> 初期化 : 電源投入
    初期化 --> BLE待機 : BLEアドバタイジング開始
    note right of 初期化: 起動メロディ再生
    
    state USB接続状態 {
        [*] --> USB未接続
        USB未接続 --> USBキーボード検出 : USBキーボード接続
        USBキーボード検出 --> USBキーボード認識完了 : HIDデバイス初期化
        note right of USBキーボード検出: キーボードタイプ判定
        note right of USBキーボード検出: (標準HID/DOIO KB16)
        USBキーボード認識完了 --> USB未接続 : USBキーボード切断
    }

    state Bluetooth状態 {
        [*] --> BLE待機
        BLE待機 --> BLE接続完了 : ホストデバイスと接続
        note right of BLE待機: 外部LED点滅
        BLE接続完了 --> BLE待機 : 接続切断
        note right of BLE接続完了: 接続音再生
        note right of BLE接続完了: 外部LED常時点灯
    }
    
    state キー処理 {
        [*] --> キー待機
        キー待機 --> キー処理中 : キー入力検出
        キー処理中 --> BLEキー送信
        キー処理中 --> キー表示更新
        キー処理中 --> キークリック音
        キー処理中 --> 内蔵LED点灯
        BLEキー送信 --> キー待機 : 送信完了
        キー表示更新 --> キー待機
        キークリック音 --> キー待機
        内蔵LED点灯 --> キー待機 : 100ms後自動消灯
    }
    
    BLE待機 --> BLE接続完了 : ホストデバイスと接続
    BLE接続完了 --> BLE待機 : 接続切断
    
    note right of BLE接続完了: 外部LED常時点灯
    note right of BLE待機: 外部LED点滅
```

## データフロー図

```mermaid
flowchart TD
    USB[USBキーボード] -->|HIDレポート| ESP[ESP32-S3]
    ESP -->|キーコード処理| BLE[BLEキーボード送信]
    ESP -->|状態表示| OLED[OLEDディスプレイ]
    ESP -->|視覚フィードバック| LED[ステータスLED]
    ESP -->|聴覚フィードバック| SPK[圧電スピーカー]
    BLE -->|HIDプロファイル| HOST[接続先デバイス]
    
    subgraph ESP32-S3内部処理
        USB_IN[USB入力検出] --> RAW[生データ解析]
        RAW --> KEY_DET[キーコード検出]
        KEY_DET --> BLE_SEND[BLE送信処理]
        KEY_DET --> KEY_FDB[キー入力フィードバック]
        KEY_FDB --> LED_CTRL[LED制御]
        KEY_FDB --> SOUND_CTRL[サウンド制御]
        KEY_FDB --> DISP_CTRL[ディスプレイ制御]
    end
```

## USBキーボード検出プロセス

```mermaid
sequenceDiagram
    participant U as USBキーボード
    participant E as ESP32-S3
    participant B as Bluetoothホスト
    
    U->>E: USB接続
    E->>E: USBデバイス検出
    E->>E: ベンダー/製品ID確認
    alt DOIOキーボード検出
        E->>E: DOIO KB16専用処理を有効化
    else 標準HIDキーボード
        E->>E: 標準HID処理を使用
    end
    E->>E: デバイス情報をOLEDに表示
    
    U->>E: キーボードHIDレポート送信
    E->>E: キーコード抽出
    E->>E: キープレス音再生
    E->>E: 内蔵LEDを点灯
    E->>E: キーをディスプレイに表示
    E->>B: BLEキーボードHIDレポート送信
    B->>E: ステータス更新 (接続/切断)
    
    alt Bluetooth切断
        E->>E: 外部LEDを点滅モードに変更
        E->>E: 切断音を再生
    else Bluetooth再接続
        E->>E: 外部LEDを点灯モードに変更
        E->>E: 接続音を再生
    end
```

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

## カスタマイズ可能なパラメータ
- Peripherals.h:
  - BLE_BLINK_INTERVAL: LED点滅間隔(ms)
  - KEY_FREQ: キープレス音の周波数(Hz)
  - KEY_DURATION: キープレス音の長さ(ms)
  - SOUND_ENABLED: サウンド機能のオン/オフ

- DisplayController.h:
  - SCREEN_WIDTH/HEIGHT: ディスプレイサイズ
  - maxChars: 表示文字列バッファサイズ

- main.cpp:
  - bleKeyboard("DOIO Keyboard", "DOIO", 100): デバイス名、製造者名、バッテリー%

## トラブルシューティング
- キーボードが認識されない場合:
  - USB接続を確認してください
  - サポートされていないキーボードの可能性があります
  
- Bluetoothが接続できない場合:
  - 接続先デバイスのBluetooth設定を確認してください
  - 一度ペアリングを削除して、再度接続を試みてください
  
- プログラムが書き込めない場合:
  - USB関連設定をコメントアウトしてください
  - ブートモード操作を正確に行ってください

- キーが誤認識される場合:
  - キーボードの種類に応じた特殊処理が必要な可能性があります
  - キーコードのデバッグ出力を有効にして確認してください

## システムアーキテクチャ

```mermaid
graph TB
    subgraph ハードウェア
        ESP32[ESP32-S3 マイコン]
        USB[USBホスト]
        BLE[Bluetooth LE]
        OLED[OLEDディスプレイ]
        LED_INT[内蔵LED]
        LED_EXT[外部LED]
        SPKR[圧電スピーカー]
    end
    
    subgraph コアモジュール
        MAIN[main.cpp]
        ESPUSB[EspUsbHost]
        DISP[DisplayController]
        PERI[Peripherals]
        BLE_KB[BleKeyboard]
    end
    
    MAIN --> ESPUSB
    MAIN --> DISP
    MAIN --> PERI
    MAIN --> BLE_KB
    
    ESPUSB --> USB
    BLE_KB --> BLE
    DISP --> OLED
    PERI --> LED_INT
    PERI --> LED_EXT
    PERI --> SPKR
```

## ライセンス
MIT License

## 最終更新日
2025年5月6日