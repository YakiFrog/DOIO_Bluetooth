#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_visualizer/kb16_visual_ui.py
# DOIO KB16 キーボード可視化ツール
# PySide6を使用してDOIO KB16のキー操作を視覚的に表示する

import sys
import os
import time
import json
import datetime
from collections import defaultdict
import threading

# 親ディレクトリをPythonパスに追加して、既存のモジュールをインポートできるようにする
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# PySide6のインポート
try:
    from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QGridLayout,
                                QVBoxLayout, QHBoxLayout, QPushButton, QLabel,
                                QFrame, QSizePolicy, QDial, QFileDialog, QMenuBar, QMenu, QMessageBox)
    from PySide6.QtCore import Qt, QTimer, Signal, Slot, QSize
    from PySide6.QtGui import QColor, QPalette, QAction
except ImportError:
    print("PySide6がインストールされていません。pip install PySide6でインストールしてください。")
    sys.exit(1)

# 既存のDOIO KB16解析ツールをインポート
try:
    import hid
    from kb16_keyboard_manager import KeyboardManager, KeyboardNotFoundError
except ImportError:
    print("必要なモジュールがインストールされていません。")
    print("hidapiがインストールされていることを確認してください: pip install hidapi")
    sys.exit(1)

# DOIOの定数定義
DOIO_VENDOR_ID = 0xD010
DOIO_PRODUCT_ID = 0x1601

# キーマトリックスのサイズ
GRID_ROWS = 4
GRID_COLS = 4

# カラー設定
DEFAULT_COLOR = "#2E2E2E"         # 通常時の色
PRESSED_COLOR = "#FF5733"         # 押下時の色
KNOB_DEFAULT_COLOR = "#3E3E3E"    # ノブの通常色
KNOB_ACTIVE_COLOR = "#33AAFF"     # ノブのアクティブ色

# 設定ファイルの保存パス
CONFIG_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CONFIG_PATH = os.path.join(CONFIG_DIR, "kb16_mapping.json")

# デフォルトのキーマッピング
DEFAULT_KEY_MAPPING = {
    # 物理的なキーID: 表示上のキーID (グリッド上の位置)
    1: 1,  # 左上
    2: 2,
    3: 3,
    4: 4,  # 右上
    5: 5,
    6: 6,
    7: 7,
    8: 8,
    9: 9,
    10: 10,
    11: 11,
    12: 12,
    13: 13, # 左下
    14: 14,
    15: 15,
    16: 16  # 右下
}

# デフォルトのノブマッピング
DEFAULT_KNOB_MAPPING = {
    # 物理的なノブID: 表示上のノブID
    1: 1,
    2: 2,
    3: 3
}


class KeyButton(QPushButton):
    """キーボードのキーを表すボタン"""
    def __init__(self, key_id, display_id=None, parent=None):
        super().__init__(parent)
        self.key_id = key_id
        self.display_id = display_id if display_id is not None else key_id
        self.setText(f"{self.display_id}")
        self.setMinimumSize(60, 60)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setStyleSheet(f"background-color: {DEFAULT_COLOR}; color: white; border-radius: 5px;")
        self.pressed_state = False
    
    def set_pressed(self, is_pressed):
        """キーの押下状態を設定"""
        if self.pressed_state != is_pressed:
            self.pressed_state = is_pressed
            color = PRESSED_COLOR if is_pressed else DEFAULT_COLOR
            self.setStyleSheet(f"background-color: {color}; color: white; border-radius: 5px;")
    
    def set_display_id(self, display_id):
        """表示用IDを設定"""
        self.display_id = display_id
        self.setText(f"{display_id}")


class KnobDial(QWidget):
    """ノブを表すダイアル"""
    def __init__(self, knob_id, display_id=None, parent=None):
        super().__init__(parent)
        self.knob_id = knob_id
        self.display_id = display_id if display_id is not None else knob_id
        self.pressed = False
        
        layout = QVBoxLayout(self)
        
        # ダイアルウィジェット
        self.dial = QDial()
        self.dial.setMinimumSize(60, 60)
        self.dial.setRange(0, 100)
        self.dial.setValue(50)
        self.dial.setWrapping(True)
        
        # ラベル
        self.label = QLabel(f"ノブ {self.display_id}")
        self.label.setAlignment(Qt.AlignCenter)
        self.label.setStyleSheet("color: white;")
        
        # レイアウトに追加
        layout.addWidget(self.dial)
        layout.addWidget(self.label)
        
        self.setStyleSheet(f"background-color: {KNOB_DEFAULT_COLOR}; border-radius: 10px;")
        self.last_value = 50
    
    def set_pressed(self, is_pressed):
        """ノブの押下状態を設定"""
        if self.pressed != is_pressed:
            self.pressed = is_pressed
            color = PRESSED_COLOR if is_pressed else KNOB_DEFAULT_COLOR
            self.setStyleSheet(f"background-color: {color}; border-radius: 10px;")
    
    def set_rotated(self, value):
        """ノブの回転状態を更新"""
        if value != self.last_value:
            self.dial.setValue(value)
            # 回転を示すハイライト
            self.setStyleSheet(f"background-color: {KNOB_ACTIVE_COLOR}; border-radius: 10px;")
            # 短いタイマーで通常の色に戻す
            QTimer.singleShot(150, lambda: self.setStyleSheet(
                f"background-color: {PRESSED_COLOR if self.pressed else KNOB_DEFAULT_COLOR}; border-radius: 10px;"))
            self.last_value = value
    
    def set_display_id(self, display_id):
        """表示用IDを設定"""
        self.display_id = display_id
        self.label.setText(f"ノブ {display_id}")


class DOIOKB16Visualizer(QMainWindow):
    """DOIO KB16 可視化メインウィンドウ"""
    
    # シグナル定義
    key_state_changed = Signal(int, bool)  # キーID, 状態
    knob_pressed = Signal(int, bool)       # ノブID, 状態
    knob_rotated = Signal(int, int)        # ノブID, 値
    
    def __init__(self):
        super().__init__()
        
        self.setWindowTitle("DOIO KB16 キー可視化")
        self.setMinimumSize(400, 500)
        
        # キーマッピング設定
        self.key_mapping = {}
        self.knob_mapping = {}
        self.reverse_key_mapping = {}
        self.reverse_knob_mapping = {}
        self.load_mapping()
        
        # メニューバーの設定
        self.setup_menu()
        
        # メインウィジェット
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # メインレイアウト
        main_layout = QVBoxLayout(central_widget)
        
        # キーグリッド
        grid_frame = QFrame()
        grid_frame.setFrameShape(QFrame.StyledPanel)
        grid_frame.setStyleSheet("background-color: #1E1E1E; border-radius: 10px;")
        grid_layout = QGridLayout(grid_frame)
        
        # キーボタンの作成
        self.key_buttons = {}
        key_id = 0
        for row in range(GRID_ROWS):
            for col in range(GRID_COLS):
                key_id += 1
                display_id = key_id  # デフォルトでは表示IDは位置IDと同じ
                button = KeyButton(key_id, display_id)
                self.key_buttons[key_id] = button
                grid_layout.addWidget(button, row, col)
        
        main_layout.addWidget(grid_frame)
        
        # ノブレイアウト
        knobs_frame = QFrame()
        knobs_frame.setFrameShape(QFrame.StyledPanel)
        knobs_frame.setStyleSheet("background-color: #1E1E1E; border-radius: 10px;")
        knobs_layout = QHBoxLayout(knobs_frame)
        
        # ノブの作成
        self.knobs = {}
        for knob_id in range(1, 4):
            display_id = knob_id  # デフォルトでは表示IDは位置IDと同じ
            knob = KnobDial(knob_id, display_id)
            self.knobs[knob_id] = knob
            knobs_layout.addWidget(knob)
        
        main_layout.addWidget(knobs_frame)
        
        # メインウィンドウの背景色設定
        palette = self.palette()
        palette.setColor(QPalette.Window, QColor("#121212"))
        self.setPalette(palette)
        
        # シグナル接続
        self.key_state_changed.connect(self.update_key_state)
        self.knob_pressed.connect(self.update_knob_press)
        self.knob_rotated.connect(self.update_knob_rotation)
        
        # HIDキャプチャスレッド
        self.hid_capture_thread = None
        self.running = False
        
        # ステータスバー
        self.statusBar().showMessage("準備完了")
        self.statusBar().setStyleSheet("color: white;")
        
        # マッピングの適用
        self.apply_mapping()
        
        # キャプチャ開始
        self.start_capture()
    
    def setup_menu(self):
        """メニューバーの設定"""
        menubar = QMenuBar(self)
        self.setMenuBar(menubar)
        
        # ファイルメニュー
        file_menu = menubar.addMenu("ファイル")
        
        # マッピング設定の読み込み
        load_action = QAction("マッピング設定の読み込み", self)
        load_action.triggered.connect(self.load_mapping_from_file)
        file_menu.addAction(load_action)
        
        # マッピング設定の保存
        save_action = QAction("マッピング設定の保存", self)
        save_action.triggered.connect(self.save_mapping_to_file)
        file_menu.addAction(save_action)
        
        # デフォルト設定に戻す
        reset_action = QAction("デフォルト設定に戻す", self)
        reset_action.triggered.connect(self.reset_to_default_mapping)
        file_menu.addAction(reset_action)
        
        file_menu.addSeparator()
        
        # 終了
        exit_action = QAction("終了", self)
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)
    
    def load_mapping(self, config_path=DEFAULT_CONFIG_PATH):
        """マッピング設定を読み込む"""
        try:
            if os.path.exists(config_path):
                with open(config_path, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                    self.key_mapping = config.get('key_mapping', DEFAULT_KEY_MAPPING)
                    self.knob_mapping = config.get('knob_mapping', DEFAULT_KNOB_MAPPING)
                    self.statusBar().showMessage(f"マッピング設定を読み込みました: {config_path}")
            else:
                # デフォルト設定を使用
                self.key_mapping = DEFAULT_KEY_MAPPING.copy()
                self.knob_mapping = DEFAULT_KNOB_MAPPING.copy()
                # デフォルト設定を保存
                self.save_mapping(config_path)
                self.statusBar().showMessage(f"デフォルトマッピング設定を作成しました: {config_path}")
        except Exception as e:
            self.statusBar().showMessage(f"マッピング設定の読み込みエラー: {str(e)}")
            # エラー時はデフォルト設定を使用
            self.key_mapping = DEFAULT_KEY_MAPPING.copy()
            self.knob_mapping = DEFAULT_KNOB_MAPPING.copy()
        
        # 逆引きマッピングの作成
        self.update_reverse_mapping()
    
    def save_mapping(self, config_path=DEFAULT_CONFIG_PATH):
        """マッピング設定を保存する"""
        try:
            config = {
                'key_mapping': self.key_mapping,
                'knob_mapping': self.knob_mapping
            }
            with open(config_path, 'w', encoding='utf-8') as f:
                json.dump(config, f, indent=2, ensure_ascii=False)
            self.statusBar().showMessage(f"マッピング設定を保存しました: {config_path}")
            return True
        except Exception as e:
            self.statusBar().showMessage(f"マッピング設定の保存エラー: {str(e)}")
            return False
    
    def update_reverse_mapping(self):
        """逆引きマッピングの更新"""
        # キーの逆引きマッピング
        self.reverse_key_mapping = {}
        for physical_id, display_id in self.key_mapping.items():
            physical_id_str = str(physical_id)  # JSONではキーが文字列になることがあるため
            self.reverse_key_mapping[int(display_id)] = int(physical_id_str)
        
        # ノブの逆引きマッピング
        self.reverse_knob_mapping = {}
        for physical_id, display_id in self.knob_mapping.items():
            physical_id_str = str(physical_id)
            self.reverse_knob_mapping[int(display_id)] = int(physical_id_str)
    
    def apply_mapping(self):
        """マッピングをUIに適用する"""
        # キーマッピングの適用
        for grid_id, button in self.key_buttons.items():
            # このグリッド位置に対応する物理キーIDを取得
            if grid_id in self.reverse_key_mapping:
                physical_id = self.reverse_key_mapping[grid_id]
                button.set_display_id(physical_id)
            else:
                button.set_display_id(grid_id)
        
        # ノブマッピングの適用
        for ui_id, knob in self.knobs.items():
            # このUI位置に対応する物理ノブIDを取得
            if ui_id in self.reverse_knob_mapping:
                physical_id = self.reverse_knob_mapping[ui_id]
                knob.set_display_id(physical_id)
            else:
                knob.set_display_id(ui_id)
    
    def load_mapping_from_file(self):
        """ファイル選択ダイアログからマッピング設定を読み込む"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "マッピング設定ファイルを開く", CONFIG_DIR, "JSON Files (*.json)"
        )
        if file_path:
            self.load_mapping(file_path)
            self.apply_mapping()
    
    def save_mapping_to_file(self):
        """ファイル選択ダイアログでマッピング設定を保存する"""
        file_path, _ = QFileDialog.getSaveFileName(
            self, "マッピング設定ファイルを保存", DEFAULT_CONFIG_PATH, "JSON Files (*.json)"
        )
        if file_path:
            self.save_mapping(file_path)
    
    def reset_to_default_mapping(self):
        """デフォルトマッピングに戻す"""
        reply = QMessageBox.question(
            self, "デフォルト設定の確認", 
            "マッピング設定をデフォルトに戻しますか？",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No
        )
        
        if reply == QMessageBox.Yes:
            self.key_mapping = DEFAULT_KEY_MAPPING.copy()
            self.knob_mapping = DEFAULT_KNOB_MAPPING.copy()
            self.update_reverse_mapping()
            self.apply_mapping()
            self.save_mapping()
            self.statusBar().showMessage("デフォルトマッピングを適用しました")
    
    @Slot(int, bool)
    def update_key_state(self, key_id, state):
        """キーの状態を更新"""
        # 物理キーIDを表示用キーIDに変換
        if str(key_id) in self.key_mapping or key_id in self.key_mapping:
            display_id = self.key_mapping.get(str(key_id), self.key_mapping.get(key_id, key_id))
            if display_id in self.key_buttons:
                self.key_buttons[display_id].set_pressed(state)
    
    @Slot(int, bool)
    def update_knob_press(self, knob_id, state):
        """ノブの押下状態を更新"""
        # 物理ノブIDを表示用ノブIDに変換
        if str(knob_id) in self.knob_mapping or knob_id in self.knob_mapping:
            display_id = self.knob_mapping.get(str(knob_id), self.knob_mapping.get(knob_id, knob_id))
            if display_id in self.knobs:
                self.knobs[display_id].set_pressed(state)
    
    @Slot(int, int)
    def update_knob_rotation(self, knob_id, value):
        """ノブの回転状態を更新"""
        # 物理ノブIDを表示用ノブIDに変換
        if str(knob_id) in self.knob_mapping or knob_id in self.knob_mapping:
            display_id = self.knob_mapping.get(str(knob_id), self.knob_mapping.get(knob_id, knob_id))
            if display_id in self.knobs:
                self.knobs[display_id].set_rotated(value)
    
    def keyboard_callback(self, report, updated_keys, updated_encoders, updated_encoder_buttons):
        """キーボードマネージャーからのコールバック"""
        # キーの状態を更新
        for key_id, state in updated_keys.items():
            self.key_state_changed.emit(key_id, state)
        
        # エンコーダー（ノブ）の回転状態を更新
        for encoder_id, value in updated_encoders.items():
            self.knob_rotated.emit(encoder_id, value)
        
        # エンコーダー（ノブ）のボタン状態を更新
        for encoder_id, state in updated_encoder_buttons.items():
            self.knob_pressed.emit(encoder_id, state)
    
    def start_capture(self):
        """HIDキャプチャを開始"""
        try:
            # キーボードマネージャーの初期化
            if not hasattr(self, 'keyboard_manager'):
                self.keyboard_manager = KeyboardManager(
                    vendor_id=DOIO_VENDOR_ID,
                    product_id=DOIO_PRODUCT_ID,
                    callback=self.keyboard_callback
                )
            
            # キーボードに接続して、キャプチャを開始
            if self.keyboard_manager.connect():
                self.keyboard_manager.start_capture()
                self.statusBar().showMessage(f"DOIO KB16に接続しました (VID: 0x{DOIO_VENDOR_ID:04X}, PID: 0x{DOIO_PRODUCT_ID:04X})")
            else:
                self.statusBar().showMessage("キーボードへの接続に失敗しました")
        
        except KeyboardNotFoundError:
            self.statusBar().showMessage(f"DOIO KB16 (VID: 0x{DOIO_VENDOR_ID:04X}, PID: 0x{DOIO_PRODUCT_ID:04X}) が見つかりません")
        except Exception as e:
            self.statusBar().showMessage(f"エラー: {str(e)}")
            print(f"キャプチャエラー: {e}")
    
    def stop_capture(self):
        """HIDキャプチャを停止"""
        if hasattr(self, 'keyboard_manager'):
            self.keyboard_manager.stop_capture()
            self.statusBar().showMessage("キャプチャ停止")
    
    def closeEvent(self, event):
        """ウィンドウが閉じられる時の処理"""
        self.stop_capture()
        if hasattr(self, 'keyboard_manager'):
            self.keyboard_manager.close()
        super().closeEvent(event)


def main():
    app = QApplication(sys.argv)
    window = DOIOKB16Visualizer()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
