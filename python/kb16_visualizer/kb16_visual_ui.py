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
                                QFrame, QSizePolicy, QDial)
    from PySide6.QtCore import Qt, QTimer, Signal, Slot, QSize
    from PySide6.QtGui import QColor, QPalette
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


class KeyButton(QPushButton):
    """キーボードのキーを表すボタン"""
    def __init__(self, key_id, parent=None):
        super().__init__(parent)
        self.key_id = key_id
        self.setText(f"{key_id}")
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


class KnobDial(QWidget):
    """ノブを表すダイアル"""
    def __init__(self, knob_id, parent=None):
        super().__init__(parent)
        self.knob_id = knob_id
        self.pressed = False
        
        layout = QVBoxLayout(self)
        
        # ダイアルウィジェット
        self.dial = QDial()
        self.dial.setMinimumSize(60, 60)
        self.dial.setRange(0, 100)
        self.dial.setValue(50)
        self.dial.setWrapping(True)
        
        # ラベル
        self.label = QLabel(f"ノブ {knob_id}")
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
                button = KeyButton(key_id)
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
            knob = KnobDial(knob_id)
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
        
        # キャプチャ開始
        self.start_capture()
    
    @Slot(int, bool)
    def update_key_state(self, key_id, state):
        """キーの状態を更新"""
        if key_id in self.key_buttons:
            self.key_buttons[key_id].set_pressed(state)
    
    @Slot(int, bool)
    def update_knob_press(self, knob_id, state):
        """ノブの押下状態を更新"""
        if knob_id in self.knobs:
            self.knobs[knob_id].set_pressed(state)
    
    @Slot(int, int)
    def update_knob_rotation(self, knob_id, value):
        """ノブの回転状態を更新"""
        if knob_id in self.knobs:
            self.knobs[knob_id].set_rotated(value)
    
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
