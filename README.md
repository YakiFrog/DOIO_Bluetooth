# DOIO Bluetooth HIDブリッジ

このプロジェクトは、USB接続されたキーボードの入力をBluetooth HID経由で他のデバイスに転送するブリッジ機能を提供します。ESP32（Seeed XIAO ESP32S3）を使用して、USBキーボードをBluetooth対応デバイス（PC、スマートフォン、タブレットなど）で使用できるようにします。

## 主な機能

- USB HIDキーボードからの入力をBluetooth HIDに変換
- 最大6キー同時押しのNキーロールオーバー対応
- モディファイアキー（Ctrl、Shift、Alt、GUI/Windowsキー）の完全サポート
- セキュアなBluetooth接続（ペアリングと暗号化）
- 接続状態をLEDで表示
- シリアルモニタによるデバッグ情報出力

## システム概要図

```mermaid
graph LR
    A[USB キーボード] -->|USB HID| B[ESP32]
    B -->|BLE HID| C[PC/スマートフォン/タブレット]
    D[LED] -->|接続状態表示| B
    E[シリアル通信] -->|デバッグ情報| B
```

## 処理フロー

```mermaid
flowchart TD
    START([開始]) --> INIT[初期化処理]
    INIT --> BLE[Bluetooth HID初期化]
    BLE --> TUSB[TinyUSB初期化]
    TUSB --> WAIT{待機状態}
    
    WAIT --> |USB接続| USB_CONN[USB HID接続検出]
    USB_CONN --> WAIT
    
    WAIT --> |キー入力| KEY_INPUT[キー入力処理]
    KEY_INPUT --> CONVERT[USB→BLE変換]
    CONVERT --> SEND[Bluetoothで送信]
    SEND --> WAIT
    
    WAIT --> |Bluetooth接続| BLE_CONN[BLE接続状態更新]
    BLE_CONN --> LED[LED状態更新]
    LED --> WAIT
    
    WAIT --> |USB切断| USB_DISCONN[USB HID切断検出]
    USB_DISCONN --> RELEASE[全キーリリース]
    RELEASE --> WAIT
    
    WAIT --> |Bluetooth切断| BLE_DISCONN[BLE切断状態更新]
    BLE_DISCONN --> LED_OFF[LED点滅モード]
    LED_OFF --> WAIT
```

## シーケンス図

```mermaid
sequenceDiagram
    participant K as USBキーボード
    participant E as ESP32
    participant D as 接続先デバイス
    
    Note over E: 初期化
    E->>E: BLEセキュリティ設定
    E->>E: TinyUSB初期化
    
    K->>E: USB接続
    E->>E: tuh_hid_keyboard_mounted_cb()
    
    K->>E: キー入力
    E->>E: tuh_hid_keyboard_report_received_cb()
    E->>D: BLEキー入力送信
    
    K->>E: キーリリース
    E->>E: キー状態変更検出
    E->>D: BLEキーリリース送信
    
    K--xE: USB切断
    E->>E: tuh_hid_keyboard_unmounted_cb()
    E->>D: 全キーリリース送信
    
    D-->>E: Bluetooth接続
    E->>E: LED点灯
    
    D--xE: Bluetooth切断
    E->>E: LED点滅モード開始
```

## キー処理ロジック

```mermaid
flowchart TD
    REPORT[キーボードレポート受信] --> MOD[モディファイアキー処理]
    MOD --> LOOP[各キーコードを処理]
    LOOP --> CHECK{前回と比較}
    CHECK -->|新規キー| PRESS[BLEキー押下]
    CHECK -->|継続キー| SKIP[スキップ]
    CHECK -->|解放キー| RELEASE[BLEキー解放]
    PRESS & SKIP & RELEASE --> SAVE[現在のレポートを保存]
    SAVE --> END([完了])
```

## 使用方法

1. ESP32（Seeed XIAO ESP32S3）にこのファームウェアを書き込みます
2. USBキーボードをESP32のUSBポートに接続します
3. Bluetooth対応デバイスでペアリング設定を開き、"DOIO_KB16"という名前のデバイスを選択します
4. ペアリングが完了すると、USBキーボードからの入力がBluetooth経由でデバイスに送信されます
5. 接続状態はLEDで確認できます：
   - 点灯：Bluetooth接続中
   - ゆっくり点滅：ペアリング待機中

## ハードウェア要件

- Seeed XIAO ESP32S3（または互換性のあるESP32マイコン）
- USB HIDキーボード
- USB OTGケーブル（必要に応じて）

## ソフトウェア構成

```mermaid
graph TD
    MAIN[main.cpp] --> BLE[BleKeyboard]
    MAIN --> TUSB[Adafruit_TinyUSB]
    MAIN --> SEC[BLESecurity]
    
    BLE --> BLESRV[BLEServer]
    BLE --> HIDDVC[HIDデバイス機能]
    
    TUSB --> USBHOST[USBホスト機能]
    TUSB --> HIDCLNT[HIDクライアント機能]
    
    SEC --> AUTH[認証・暗号化]
    
    subgraph コールバック関数
    CB1[tuh_hid_keyboard_mounted_cb]
    CB2[tuh_hid_keyboard_unmounted_cb]
    CB3[tuh_hid_keyboard_report_received_cb]
    end
    
    HIDCLNT --> CB1 & CB2 & CB3
```

## セキュリティ

このプロジェクトでは、Bluetooth接続のセキュリティを確保するために以下の機能を実装しています：

- MITM（Man-In-The-Middle）プロテクション
- ボンディング（ペアリング情報の永続化）
- 16ビット暗号化キー
- カスタマイズ可能なパスキー認証

## トラブルシューティング

- ペアリングに失敗する場合：デバイスのBluetooth設定からDOIO_KB16を削除して再試行してください
- キー入力が反応しない場合：接続状態を確認し、必要に応じてUSBキーボードの再接続やESP32のリセットを行ってください
- 特殊キーが動作しない場合：シリアルモニタでキーコードを確認し、必要に応じてコードを修正してください

## ライセンス

このプロジェクトはオープンソースとして提供されています。使用するライブラリのライセンスに従ってください。

## 貢献

バグ報告や機能強化の提案は大歓迎です。プルリクエストを送るか、イシューを作成してください。