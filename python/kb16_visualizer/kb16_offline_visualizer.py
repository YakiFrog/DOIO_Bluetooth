#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_visualizer/kb16_offline_visualizer.py
# DOIO KB16 オフライン可視化ツール
# 保存されたキャプチャファイルを使用してDOIO KB16の操作を視覚化する

import sys
import os
import time
import json
import csv
import datetime
import argparse
from collections import defaultdict
import threading

# 親ディレクトリをPythonパスに追加して、既存のモジュールをインポートできるようにする
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# PySide6のインポート
try:
    from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QGridLayout,
                                QVBoxLayout, QHBoxLayout, QPushButton, QLabel,
                                QFrame, QSizePolicy, QDial, QFileDialog, QSlider,
                                QToolBar, QStatusBar, QStyle)
    from PySide6.QtCore import Qt, QTimer, Signal, Slot, QSize
    from PySide6.QtGui import QColor, QPalette, QAction, QIcon
except ImportError:
    print("PySide6がインストールされていません。pip install PySide6でインストールしてください。")
    sys.exit(1)

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


class DOIOOfflineVisualizer(QMainWindow):
    """DOIO KB16 オフライン可視化メインウィンドウ"""
    
    # シグナル定義
    key_state_changed = Signal(int, bool)  # キーID, 状態
    knob_pressed = Signal(int, bool)       # ノブID, 状態
    knob_rotated = Signal(int, int)        # ノブID, 値
    
    def __init__(self):
        super().__init__()
        
        self.setWindowTitle("DOIO KB16 オフライン可視化")
        self.setMinimumSize(500, 600)
        
        # メインウィジェット
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # メインレイアウト
        main_layout = QVBoxLayout(central_widget)
        
        # ツールバー
        self.create_toolbar()
        
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
        
        # 再生コントロール
        control_layout = QHBoxLayout()
        
        # 再生/一時停止ボタン
        self.play_button = QPushButton("再生")
        self.play_button.clicked.connect(self.toggle_playback)
        control_layout.addWidget(self.play_button)
        
        # スライダー
        self.timeline_slider = QSlider(Qt.Horizontal)
        self.timeline_slider.setRange(0, 100)
        self.timeline_slider.setValue(0)
        self.timeline_slider.sliderMoved.connect(self.seek_to_position)
        control_layout.addWidget(self.timeline_slider)
        
        # 再生速度
        self.speed_label = QLabel("速度: 1x")
        control_layout.addWidget(self.speed_label)
        
        main_layout.addLayout(control_layout)
        
        # メインウィンドウの背景色設定
        palette = self.palette()
        palette.setColor(QPalette.Window, QColor("#121212"))
        self.setPalette(palette)
        
        # シグナル接続
        self.key_state_changed.connect(self.update_key_state)
        self.knob_pressed.connect(self.update_knob_press)
        self.knob_rotated.connect(self.update_knob_rotation)
        
        # 再生関連の変数
        self.reports = []
        self.current_index = 0
        self.playing = False
        self.playback_timer = QTimer()
        self.playback_timer.timeout.connect(self.play_next_report)
        self.playback_speed = 1.0
        
        # ステータスバー
        self.statusBar().showMessage("ファイルを開いてください")
        self.statusBar().setStyleSheet("color: white;")
    
    def create_toolbar(self):
        """ツールバーの作成"""
        toolbar = QToolBar("メインツールバー")
        self.addToolBar(toolbar)
        
        # ファイルを開くアクション
        open_action = QAction("ファイルを開く", self)
        open_action.triggered.connect(self.open_file)
        toolbar.addAction(open_action)
        
        # 速度調整アクション
        speed_down_action = QAction("速度ダウン", self)
        speed_down_action.triggered.connect(lambda: self.adjust_speed(-0.25))
        toolbar.addAction(speed_down_action)
        
        speed_up_action = QAction("速度アップ", self)
        speed_up_action.triggered.connect(lambda: self.adjust_speed(0.25))
        toolbar.addAction(speed_up_action)
    
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
    
    def open_file(self):
        """キャプチャファイルを開く"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "キャプチャファイルを開く", "", "JSON Files (*.json);;CSV Files (*.csv);;All Files (*)"
        )
        
        if file_path:
            self.load_capture_file(file_path)
    
    def load_capture_file(self, file_path):
        """キャプチャファイルを読み込む"""
        try:
            self.reports = []
            
            # JSONファイル形式
            if file_path.endswith('.json'):
                with open(file_path, 'r') as f:
                    data = json.load(f)
                    if 'reports' in data:
                        self.reports = data['reports']
            # CSVファイル形式
            elif file_path.endswith('.csv'):
                with open(file_path, 'r') as f:
                    reader = csv.reader(f)
                    next(reader)  # ヘッダーをスキップ
                    for row in reader:
                        if len(row) >= 3:  # Hex形式のデータを含む列がある場合
                            hex_data = row[2]
                            try:
                                data_bytes = bytes([int(b, 16) for b in hex_data.split()])
                                self.reports.append({'data': list(data_bytes), 'timestamp': row[0]})
                            except ValueError:
                                pass  # 不正な16進データを無視
            
            # レポートが読み込まれたらUIを更新
            if self.reports:
                self.current_index = 0
                self.timeline_slider.setRange(0, len(self.reports) - 1)
                self.timeline_slider.setValue(0)
                self.statusBar().showMessage(f"{len(self.reports)}個のレポートを読み込みました ({os.path.basename(file_path)})")
                
                # 最初のレポートを表示
                self.show_report(0)
            else:
                self.statusBar().showMessage(f"読み込み可能なレポートがありません ({os.path.basename(file_path)})")
        
        except Exception as e:
            self.statusBar().showMessage(f"ファイル読み込みエラー: {str(e)}")
    
    def show_report(self, index):
        """指定したインデックスのレポートを表示"""
        if 0 <= index < len(self.reports):
            report = self.reports[index]
            
            # レポートデータを解析して表示
            data = report.get('data', [])
            
            # 全てのキーとノブをリセット
            for key_id in self.key_buttons:
                self.key_buttons[key_id].set_pressed(False)
            
            for knob_id in self.knobs:
                self.knobs[knob_id].set_pressed(False)
            
            # レポートの長さが十分であることを確認
            if data and len(data) >= 8:
                # キーマトリックスの状態を処理
                for key_id in range(1, 17):  # 16キー
                    key_byte = (key_id - 1) // 8
                    key_bit = (key_id - 1) % 8
                    
                    if key_byte < len(data):
                        key_state = bool(data[key_byte] & (1 << key_bit))
                        self.key_state_changed.emit(key_id, key_state)
                
                # ノブの状態を処理（仮のマッピング）
                if len(data) >= 10:
                    # ノブ1
                    knob_value = min(100, data[8] * 100 // 255)
                    self.knob_rotated.emit(1, knob_value)
                    
                    # ノブ2
                    if len(data) >= 11:
                        knob_value = min(100, data[9] * 100 // 255)
                        self.knob_rotated.emit(2, knob_value)
                    
                    # ノブ3
                    if len(data) >= 12:
                        knob_value = min(100, data[10] * 100 // 255)
                        self.knob_rotated.emit(3, knob_value)
                    
                    # ノブのプッシュ状態
                    if len(data) >= 13:
                        for knob_id in range(1, 4):
                            knob_bit = knob_id - 1
                            state = bool(data[11] & (1 << knob_bit))
                            self.knob_pressed.emit(knob_id, state)
            
            # タイムライン更新
            self.timeline_slider.setValue(index)
    
    def toggle_playback(self):
        """再生/一時停止を切り替える"""
        if self.playing:
            self.pause_playback()
        else:
            self.start_playback()
    
    def start_playback(self):
        """再生を開始"""
        if not self.reports:
            return
        
        self.playing = True
        self.play_button.setText("一時停止")
        self.playback_timer.start(50 / self.playback_speed)  # 50msごとに次のレポートを表示
    
    def pause_playback(self):
        """再生を一時停止"""
        self.playing = False
        self.play_button.setText("再生")
        self.playback_timer.stop()
    
    def play_next_report(self):
        """次のレポートを表示"""
        if not self.reports:
            return
        
        self.current_index += 1
        if self.current_index >= len(self.reports):
            self.current_index = 0  # ループ再生
        
        self.show_report(self.current_index)
    
    def seek_to_position(self, position):
        """指定した位置にシークする"""
        if 0 <= position < len(self.reports):
            self.current_index = position
            self.show_report(position)
    
    def adjust_speed(self, delta):
        """再生速度を調整"""
        self.playback_speed = max(0.25, min(4.0, self.playback_speed + delta))
        self.speed_label.setText(f"速度: {self.playback_speed:.2f}x")
        if self.playing:
            self.playback_timer.setInterval(int(50 / self.playback_speed))


def main():
    parser = argparse.ArgumentParser(description='DOIO KB16 オフライン可視化ツール')
    parser.add_argument('--file', help='キャプチャファイルのパス（JSONまたはCSV）')
    args = parser.parse_args()
    
    app = QApplication(sys.argv)
    app.setStyle('Fusion')  # Fusionスタイルを使用
    window = DOIOOfflineVisualizer()
    
    # コマンドライン引数からファイルが指定された場合、そのファイルを開く
    if args.file and os.path.exists(args.file):
        window.load_capture_file(args.file)
    
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
