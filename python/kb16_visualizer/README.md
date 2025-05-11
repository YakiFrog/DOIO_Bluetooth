#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_visualizer/README.md
# DOIO KB16 可視化ツール

このフォルダには、DOIO KB16キーボードの操作を可視化するためのツールが含まれています。これらのツールを使用することで、キーボードの16個のキーと3つのノブの操作状態をリアルタイムまたはオフラインで可視化できます。

## ツールの説明

- **kb16_visual_ui.py**: リアルタイムでDOIO KB16の操作を可視化するツール
- **kb16_offline_visualizer.py**: 保存されたキャプチャファイルを使ってオフラインで操作を可視化するツール
- **setup_requirements.py**: 必要なパッケージのインストールを支援するツール

## 使用方法

### 共通の起動方法 (推奨)

親ディレクトリにある `kb16_visualizer_launcher.py` を使って起動すると、依存関係の確認と適切な実行環境の設定を行います:

```bash
cd /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python
python kb16_visualizer_launcher.py --mode realtime  # リアルタイム可視化
python kb16_visualizer_launcher.py --mode offline   # オフライン可視化
python kb16_visualizer_launcher.py --mode offline --file キャプチャファイル.json  # 指定したファイルを開く
```

### 直接実行する方法

#### リアルタイム可視化

```bash
cd /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python
python kb16_visualizer/kb16_visual_ui.py
```

このスクリプトは実行するとDOIO KB16キーボードの接続を試み、接続できれば操作を視覚化します。
16個のキーと3つのノブの状態が表示され、操作に応じてUIの色が変化します。

#### オフライン可視化

```bash
cd /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python
python kb16_visualizer/kb16_offline_visualizer.py [--file キャプチャファイル.json]
```

このスクリプトはキャプチャされたデータファイル（JSONまたはCSV形式）を読み込み、記録された操作を再生します。
再生速度の調整やタイムラインのスライドが可能です。

### キャプチャファイルの取得方法

キャプチャファイルは `kb16_protocol_analyzer.py` を使って作成できます:

```bash
cd /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python
python kb16_protocol_analyzer.py --capture
```

キャプチャしたデータは自動的にJSONとCSV形式で保存されます。

## 必要なパッケージ

- PySide6: `pip install PySide6`
- hidapi: `pip install hidapi`

依存パッケージの管理には `setup_requirements.py` を使用できます:

```bash
python kb16_visualizer/setup_requirements.py
```

## UI機能説明

### リアルタイム可視化ツール

- 4x4のキーグリッド: 各キーが押されると赤色に変わります
- 3つのノブ表示: ノブの回転と押下状態を視覚的に表示します
- ステータスバー: 接続状態や操作情報を表示します

### オフライン可視化ツール

- ファイル読み込み: JSONまたはCSV形式のキャプチャファイルをサポート
- 再生コントロール: 再生/一時停止ボタンとタイムラインスライダー
- 再生速度調整: 0.25倍速〜4倍速まで調整可能

## キーボードとの互換性

このツールは以下の設定のDOIO KB16キーボードで動作確認されています:

- VID: 0xD010
- PID: 0x1601

他のVID/PIDを持つキーボードを使用する場合は、ソースコード内の `DOIO_VENDOR_ID` と `DOIO_PRODUCT_ID` の値を適切に変更してください。

## 注意点

- キーボードの検出には管理者権限が必要な場合があります。
- macOSでは「入力監視」の権限を許可する必要がある場合があります。
- キーボードのプロトコルに合わせた調整が必要な場合があります。既存のプロトコル解析ツールと組み合わせて使用してください。
- 初回実行時にはPySide6のダウンロードとインストールに時間がかかる場合があります。
