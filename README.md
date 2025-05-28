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
- 特殊なキーコード（0x09等）の正確な認識と変換
- NimBLEライブラリによるメモリ効率化（RAM 27.2KB / Flash 579KB）

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
  ## DOIO KB16特殊キーボード対応
### 対応内容
- **自動検出**: VID/PID（0xD010/0x1601）による自動認識
- **特殊レポート処理**: 0xAAフィールドを持つ独自HIDレポート形式に対応
- **専用キーマッピング**: 16キー（4x4マトリックス）のカスタムマッピング
- **キーコード変換**: 0x08-0x21（A-Z）、0x22-0x33（1-0）のカスタムマッピング

### 修正履歴（2025年5月28日）
DOIO KB16で0x09キーコードが認識されない問題を修正:
- `onKeyboard`メソッドに`isDoioKb16`フラグチェックを追加
- DOIO KB16検出時に`processDOIOKB16Report`メソッドを呼び出すように修正
- キーマッピングテーブル`kb16_key_map`に基づく正確なキー変換を実装

### キーマッピング詳細
```
位置 (0,0) -> HIDコード 0x22 ('1')
位置 (0,1) -> HIDコード 0x23 ('2')
位置 (0,2) -> HIDコード 0x24 ('3')
位置 (0,3) -> HIDコード 0x25 ('4')
位置 (1,0) -> HIDコード 0x26 ('5')
位置 (1,1) -> HIDコード 0x27 ('6')
位置 (1,2) -> HIDコード 0x30 ('7')
位置 (1,3) -> HIDコード 0x31 ('8')
位置 (2,0) -> HIDコード 0x32 ('9')
位置 (2,1) -> HIDコード 0x33 ('0')
位置 (2,2) -> HIDコード 0x28 (Enter)
位置 (2,3) -> HIDコード 0x29 (Esc)
位置 (3,0) -> HIDコード 0x2A (Backspace)
位置 (3,1) -> HIDコード 0x2B (Tab)
位置 (3,2) -> HIDコード 0x2C (Space)
位置 (3,3) -> HIDコード 0x08 ('A') ※0x09から変換
```

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

## キーコード抽出プロセス

キー入力時のデータフローと処理手順を詳細に説明します。

### 1. USB HIDレポート取得
```mermaid
flowchart TD
    START[キーボード入力] --> USB_DATA[USBデータ受信]
    USB_DATA --> CHECK{キーボードタイプ判定}
    CHECK -->|標準HIDキーボード| STANDARD[標準HIDレポート処理]
    CHECK -->|DOIO KB16| CUSTOM[KB16専用レポート処理]
    STANDARD --> EXTRACT[キーコード抽出]
    CUSTOM --> EXTRACT
```

### 2. データ構造と抽出方法

#### 標準HIDキーボードのデータ構造
```
バイト0: 修飾キー (Modifier)
  - bit 0: 左Ctrl
  - bit 1: 左Shift
  - bit 2: 左Alt
  - bit 3: 左GUI (Windows/Command)
  - bit 4: 右Ctrl
  - bit 5: 右Shift
  - bit 6: 右Alt
  - bit 7: 右GUI

バイト1: 予約済み (通常は0)

バイト2-7: プレスされたキーのコード (最大6キー)
  - 0x00: キー無し
  - 0x04-0x1D: 'a'-'z'キー
  - 0x1E-0x27: '1'-'0'キー
  - 0x28-0x38: Enter, Esc, Backspace等の特殊キー
  - 0x39-0x53: F1-F12, Caps Lockなどの機能キー
```

#### DOIO KB16専用処理
DOIO KB16は特殊なデータフォーマットを使用するため、以下の追加処理を行います:

1. **特殊レポート検証**: `reserved`フィールドが0xAAかチェック
2. **専用キーマップ処理**: `kb16_key_map`テーブルによるビット→キー変換
3. **バイト位置マッピング**: 6バイトのkeycodeフィールドから状態を抽出
4. **状態変化検出**: 前回レポートとの比較による押下/離し判定

```cpp
// キーマッピングテーブル例
const KeyMapping kb16_key_map[] = {
    { 0, 0x01, 0, 0 },  // 位置(0,0): byte0_bit0 -> '1'キー
    { 0, 0x02, 0, 1 },  // 位置(0,1): byte0_bit1 -> '2'キー
    // ... (全16キー分のマッピング)
    { 5, 0x10, 3, 3 },  // 位置(3,3): byte5_bit4 -> 'A'キー (0x09→0x08変換)
};
```

### 重要な修正内容
**問題**: DOIO KB16で0x09キーコードが認識されない
**原因**: `onKeyboard`メソッドで`processDOIOKB16Report`が呼び出されていなかった
**解決**: DOIO KB16検出時の専用処理パスを追加

```cpp
void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
    EspUsbHost::onKeyboard(report, last_report);
    
    // DOIO KB16専用処理（修正済み）
    if (isDoioKb16) {
        processDOIOKB16Report(report, last_report);
        return; // 専用処理のみ実行
    }
    // ... 標準HID処理
}
```

### 3. キーコードマッピング

USBのHIDキーコードをBLEキーコード/ASCIIに変換する処理:

```mermaid
flowchart TD
    HIDCode[HID USBキーコード] --> Check{キーコード分類}
    Check -->|0x04-0x1D| Alpha[アルファベットキー]
    Check -->|0x1E-0x27| Num[数字キー]
    Check -->|0x28-0x38| Special[特殊キー]
    Check -->|0x39-0x53| Function[ファンクションキー]
    Check -->|0x54-0x67| TenKey[テンキー]
    Check -->|0x87-0x8B| Japanese[日本語特殊キー]
    Check -->|0xE2-0xEA| Media[メディア制御キー]
    
    Alpha --> Shift{Shiftキー状態}
    Shift -->|押されている| UpperCase[大文字変換]
    Shift -->|押されていない| LowerCase[小文字変換]
    
    Num --> NumShift{Shiftキー状態}
    NumShift -->|押されている| Symbol[記号変換]
    NumShift -->|押されていない| Digit[数字変換]
    
    Special --> SpecialMap[特殊キーマッピング]
    Function --> FuncMap[ファンクションキーマッピング]
    TenKey --> TenKeyMap[テンキーマッピング]
    Japanese --> JpMap[日本語キー特殊処理]
    Media --> MediaMap[メディアキー特殊処理]
    
    UpperCase --> BLECode[BLEキーコード]
    LowerCase --> BLECode
    Symbol --> BLECode
    Digit --> BLECode
    SpecialMap --> BLECode
    FuncMap --> BLECode
    TenKeyMap --> BLECode
    JpMap --> RawMode[生キーコードモード]
    MediaMap --> MediaSend[メディア制御イベント]
    
    RawMode --> ModCheck{修飾キーあり?}
    ModCheck -->|あり| ModCombo[修飾キー組合せ処理]
    ModCheck -->|なし| SingleKey[単一キー処理]
    
    BLECode --> Send[BLE送信]
    MediaSend --> Send
    ModCombo --> Send
    SingleKey --> Send
```

### 4. 重複キー検出防止

実際のキーボードでは機械的な特性により、キーを押した瞬間に複数回の入力信号が発生することがあります。これを防ぐため、以下の処理を行います:

1. 同一キーコードの時間間隔チェック (15ms以内の再入力は無視)
2. 前回のキーボードレポートとの比較による新規キーのみの処理
3. キーコードバッファ (6キー分) 内の重複チェック

```mermaid
flowchart TD
    KeyDetected[キー検出] --> TimeCheck{前回検出から15ms以上経過?}
    TimeCheck -->|No| Ignore[無視]
    TimeCheck -->|Yes| ReportCheck{前回レポートに存在するか?}
    ReportCheck -->|Yes| Ignore
    ReportCheck -->|No| Process[キー処理実行]
    Process --> UpdateTime[時刻情報更新]
    Process --> Feedback[フィードバック処理]
    Process --> SendBLE[BLE送信処理]
```

### 5. BLEキーボード送信

```mermaid
flowchart TD
    KeyCode[キーコード] --> BLEConnCheck{BLE接続中?}
    BLEConnCheck -->|No| Skip[送信スキップ]
    BLEConnCheck -->|Yes| KeyTypeCheck{キータイプ判定}
    
    KeyTypeCheck -->|通常キー| Normal[単一キー送信]
    KeyTypeCheck -->|特殊キー| Special[特殊キー処理]
    KeyTypeCheck -->|修飾キー組合せ| Combo[組合せキー処理]
    KeyTypeCheck -->|メディアキー| Media[メディアキー送信]
    
    Normal --> Write[BLEキーボードwrite()]
    Special --> PressRelease[press()→release()]
    Combo --> MultiPress[複数press()→releaseAll()]
    Media --> MediaWrite[メディアキーwrite()]
    
    Write --> Done[送信完了]
    PressRelease --> Done
    MultiPress --> Done
    MediaWrite --> Done
```

### 6. パフォーマンス最適化

キーコード抽出と処理においては、以下の最適化を実施しています：

1. **必要最小限のメモリ使用**：NimBLEモードを使用し、RAMとFlashメモリ使用量を大幅に削減
   - 標準BLEモード: RAM 30.5KB / Flash 994KB
   - NimBLEモード: RAM 27.2KB / Flash 579KB (Flash容量44%削減)

2. **キー処理の高速化**：
   - キー遅延を15msに設定（連打性能と誤入力防止のバランス）
   - BLEキー送信のバッファリング処理
   - 不要なデバッグ出力の条件付きコンパイル

3. **専用キーボード最適化**：
   - DOIO KB16の特殊なレポート構造に対応した専用処理パス
   - ベンダー/製品ID自動検出による適切な処理の選択
   - 0xAAフィールド検証による確実なレポート識別

4. **キーコード変換の最適化**：
   - 位置ベース（row/col）からHIDコードへの直接マッピング
   - ビットマスク演算による高速な状態変化検出
   - 重複防止機能（15ms以内の重複入力を除外）

## 注意事項

### ファームウェア書き込み
ESP32-S3がUSBホストモードになると、通常のシリアル通信ができなくなります。ファームウェアを書き込む際は以下の手順に従ってください。

1. Keyboardに接続されてるUSBケーブルを抜く
2. BOOTボタン（またはIO0ボタン）を押しながらUSBケーブルを接続
3. BOOTボタンを数秒間押し続ける
4. ファームウェアを書き込む

### 対応キーボード
- **標準的なHIDキーボード**: 一般的なUSB接続キーボード
- **DOIO KB16キーボード**: 専用サポート（VID: 0xD010, PID: 0x1601）
  - 4x4マトリックス構成（16キー）
  - 特殊HIDレポート形式（0xAAフィールド）
  - カスタムキーマッピング対応
  - 0x09キーコード問題修正済み
- **その他多くのUSB HIDキーボード**: 自動検出機能付き

### 既知の制限事項
- USBホストモード時のシリアル通信制限
- 一部の特殊キーボードで追加設定が必要な場合あり
- Bluetooth接続数の制限（同時接続1台まで）

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

### キーボード関連
- **キーボードが認識されない場合**:
  - USB接続を確認してください
  - サポートされていないキーボードの可能性があります
  - シリアルモニタでデバイス検出ログを確認してください

- **DOIO KB16で特定キーが効かない場合**:
  - 0x09キーコード問題は修正済みです（2025年5月28日）
  - デバイスのVID/PID（0xD010/0x1601）が正しく検出されているか確認
  - `processDOIOKB16Report`が呼び出されているかログで確認

- **キーが誤認識される場合**:
  - キーボードの種類に応じた特殊処理が必要な可能性があります
  - キーコードのデバッグ出力を有効にして確認してください
  - 重複防止の時間間隔（15ms）を調整してください

### Bluetooth関連
- **Bluetoothが接続できない場合**:
  - 接続先デバイスのBluetooth設定を確認してください
  - 一度ペアリングを削除して、再度接続を試みてください
  - デバイス名「DOIO Keyboard」で検索してください

### プログラム書き込み関連  
- **プログラムが書き込めない場合**:
  - USBキーボードを接続している場合は一度外してください
  - BOOTボタンを押しながらUSBケーブルを接続してブートモードに入ってください
  - USB関連設定をコメントアウトしてテスト書き込みを試してください

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

## バージョン履歴
- **v1.2.1** (2025年5月28日): DOIO KB16の0x09キーコード認識問題を修正
- **v1.2.0** (2025年5月): NimBLEライブラリ対応、メモリ効率化
- **v1.1.0** (2025年4月): DOIO KB16専用サポート追加
- **v1.0.0** (2025年3月): 初回リリース

## 最終更新日
2025年5月28日

```mermaid
sequenceDiagram
    participant OniNode as おにノード（Actionサーバー・クライアント）
    participant NigeNode as にげノード（Actionサーバー・クライアント）
    participant NavServerOni as おにのナビゲーションActionサーバー
    participant NavServerNige as にげのナビゲーションActionサーバー

    Note over OniNode, NigeNode: ゲーム開始！

    NigeNode->>NavServerNige: Goal送信（遠ざかる場所へ移動）
    NavServerNige-->>NigeNode: Feedback（移動中）

    loop 周期処理（例：5Hz）
        OniNode->>NigeNode: にげの位置要求（トピック or サービス）
        NigeNode-->>OniNode: 現在位置返答
        OniNode->>NavServerOni: Goal送信（にげの位置へ移動）
        NavServerOni-->>OniNode: Feedback（接近中）
    end

    alt 接近完了（距離<1m）
        NavServerOni-->>OniNode: Result（接近完了）
        OniNode-->>NigeNode: 捕まえた通知（オプション）
    else にげが回避行動
        NigeNode->>NavServerNige: Goalキャンセル
        NigeNode->>NavServerNige: 新しいGoal送信（逃走先更新）
    end
```