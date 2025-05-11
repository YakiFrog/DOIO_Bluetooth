#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_visualizer/kb16_mapping_calibrator.py
# DOIO KB16 キーマッピング校正ツール
# PySide6を使用してDOIO KB16のキーマッピングを設定する

import sys
import os
import json
import time
from collections import defaultdict

# 親ディレクトリをPythonパスに追加
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# PySide6のインポート
try:
    from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QGridLayout,
                                QVBoxLayout, QHBoxLayout, QPushButton, QLabel,
                                QFrame, QSizePolicy, QComboBox, QFileDialog, QMessageBox)
    from PySide6.QtCore import Qt, QTimer, Signal, Slot
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
DEFAULT_COLOR = "#2E2E2E"
PRESSED_COLOR = "#FF5733"
SELECTED_COLOR = "#33FF57"
HIGHLIGHT_COLOR = "#57ADFF"

# 設定ファイルの保存パス
CONFIG_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CONFIG_PATH = os.path.join(CONFIG_DIR, "kb16_mapping.json")
CUSTOM_CONFIG_PATH = os.path.join(CONFIG_DIR, "kb16_mapping_custom.json")


class CalibrationButton(QPushButton):
    """マッピング校正用のボタン"""
    def __init__(self, grid_id, parent=None):
        super().__init__(parent)
        self.grid_id = grid_id  # UIグリッド上の位置ID
        self.physical_id = grid_id  # 物理キーID（デフォルトは同じ）
        self.setText(f"位置: {grid_id}\n物理ID: {self.physical_id}")
        self.setMinimumSize(80, 80)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setStyleSheet(f"background-color: {DEFAULT_COLOR}; color: white; border-radius: 5px;")
        self.pressed_state = False
        self.selected = False
    
    def set_pressed(self, is_pressed):
        """キーの押下状態を設定"""
        if self.pressed_state != is_pressed:
            self.pressed_state = is_pressed
            self.update_style()
    
    def set_selected(self, is_selected):
        """選択状態を設定"""
        self.selected = is_selected
        self.update_style()
    
    def set_physical_id(self, physical_id):
        """物理的なキーIDを設定"""
        self.physical_id = physical_id
        self.setText(f"位置: {self.grid_id}\n物理ID: {self.physical_id}")
    
    def update_style(self):
        """ボタンのスタイルを更新"""
        if self.selected:
            color = SELECTED_COLOR
        elif self.pressed_state:
            color = PRESSED_COLOR
        else:
            color = DEFAULT_COLOR
        self.setStyleSheet(f"background-color: {color}; color: white; border-radius: 5px;")


class CalibrationKnob(QWidget):
    """マッピング校正用のノブ表示"""
    def __init__(self, ui_id, parent=None):
        super().__init__(parent)
        self.ui_id = ui_id  # UI上の位置ID
        self.physical_id = ui_id  # 物理的なノブID（デフォルトは同じ）
        self.pressed = False
        self.selected = False
        
        layout = QVBoxLayout(self)
        
        # ノブ表示用ラベル
        self.knob_label = QLabel("◉")
        self.knob_label.setAlignment(Qt.AlignCenter)
        self.knob_label.setStyleSheet("font-size: 30pt; color: white;")
        
        # ノブIDラベル
        self.id_label = QLabel(f"位置: {ui_id}\n物理ID: {self.physical_id}")
        self.id_label.setAlignment(Qt.AlignCenter)
        self.id_label.setStyleSheet("color: white;")
        
        # レイアウトに追加
        layout.addWidget(self.knob_label)
        layout.addWidget(self.id_label)
        
        self.setMinimumSize(80, 100)
        self.setStyleSheet(f"background-color: {DEFAULT_COLOR}; border-radius: 10px;")
    
    def set_pressed(self, is_pressed):
        """ノブの押下状態を設定"""
        if self.pressed != is_pressed:
            self.pressed = is_pressed
            self.update_style()
    
    def set_selected(self, is_selected):
        """選択状態を設定"""
        self.selected = is_selected
        self.update_style()
    
    def set_physical_id(self, physical_id):
        """物理的なノブIDを設定"""
        self.physical_id = physical_id
        self.id_label.setText(f"位置: {self.ui_id}\n物理ID: {self.physical_id}")
    
    def highlight_rotation(self):
        """回転を一時的にハイライト表示"""
        self.setStyleSheet(f"background-color: {HIGHLIGHT_COLOR}; border-radius: 10px;")
        QTimer.singleShot(150, self.update_style)
    
    def update_style(self):
        """スタイルを更新"""
        if self.selected:
            color = SELECTED_COLOR
        elif self.pressed:
            color = PRESSED_COLOR
        else:
            color = DEFAULT_COLOR
        self.setStyleSheet(f"background-color: {color}; border-radius: 10px;")


class MappingCalibrationTool(QMainWindow):
    """DOIO KB16 マッピング校正ツール"""
    
    # シグナル定義
    key_pressed = Signal(int)  # 押されたキーの物理ID
    knob_pressed = Signal(int)  # 押されたノブの物理ID
    knob_rotated = Signal(int)  # 回転したノブの物理ID
    
    def __init__(self):
        super().__init__()
        
        self.setWindowTitle("DOIO KB16 マッピング校正ツール")
        self.setMinimumSize(600, 700)
        
        # 現在のマッピング
        self.key_mapping = {}
        self.knob_mapping = {}
        
        # 現在選択中のUIコンポーネント
        self.selected_button = None
        self.selected_knob = None
        
        # メニューバーの設定
        self.setup_menu()
        
        # メインウィジェット
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # メインレイアウト
        main_layout = QVBoxLayout(central_widget)
        
        # 説明ラベル
        instruction_label = QLabel(
            "使用方法: マップしたいUIボタン/ノブをクリックして選択し、キーボードの対応するキー/ノブを押してマッピングします。"
        )
        instruction_label.setStyleSheet("color: white; font-size: 12pt;")
        instruction_label.setWordWrap(True)
        main_layout.addWidget(instruction_label)
        
        # キーグリッド
        grid_frame = QFrame()
        grid_frame.setFrameShape(QFrame.StyledPanel)
        grid_frame.setStyleSheet("background-color: #1E1E1E; border-radius: 10px;")
        grid_layout = QGridLayout(grid_frame)
        grid_layout.setSpacing(10)
        
        # キーボタンの作成
        self.buttons = {}
        key_id = 0
        for row in range(GRID_ROWS):
            for col in range(GRID_COLS):
                key_id += 1
                button = CalibrationButton(key_id)
                button.clicked.connect(lambda checked, btn=button: self.select_button(btn))
                self.buttons[key_id] = button
                grid_layout.addWidget(button, row, col)
        
        main_layout.addWidget(grid_frame)
        
        # ノブレイアウト
        knobs_frame = QFrame()
        knobs_frame.setFrameShape(QFrame.StyledPanel)
        knobs_frame.setStyleSheet("background-color: #1E1E1E; border-radius: 10px;")
        knobs_layout = QHBoxLayout(knobs_frame)
        knobs_layout.setSpacing(10)
        
        # ノブの作成
        self.knobs = {}
        for knob_id in range(1, 4):
            knob = CalibrationKnob(knob_id)
            knob.mousePressEvent = lambda event, k=knob: self.select_knob(k)
            self.knobs[knob_id] = knob
            knobs_layout.addWidget(knob)
        
        main_layout.addWidget(knobs_frame)
        
        # 操作ボタンレイアウト
        button_layout = QHBoxLayout()
        
        # マッピングをリセットするボタン
        reset_button = QPushButton("マッピングをリセット")
        reset_button.setStyleSheet("background-color: #CC3333; color: white;")
        reset_button.clicked.connect(self.reset_mapping)
        button_layout.addWidget(reset_button)
        
        # マッピングを保存するボタン
        save_button = QPushButton("マッピングを保存")
        save_button.setStyleSheet("background-color: #33AA33; color: white;")
        save_button.clicked.connect(self.save_mapping)
        button_layout.addWidget(save_button)
        
        main_layout.addLayout(button_layout)
        
        # メインウィンドウの背景色設定
        palette = self.palette()
        palette.setColor(QPalette.Window, QColor("#121212"))
        self.setPalette(palette)
        
        # ステータスバー
        self.statusBar().showMessage("準備完了")
        self.statusBar().setStyleSheet("color: white;")
        
        # シグナル接続
        self.key_pressed.connect(self.handle_key_press)
        self.knob_pressed.connect(self.handle_knob_press)
        self.knob_rotated.connect(self.handle_knob_rotation)
        
        # 既存のマッピングを読み込み
        self.load_mapping()
        
        # キャプチャ開始
        self.start_capture()
    
    def setup_menu(self):
        """メニューバーの設定"""
        menubar = self.menuBar()
        
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

        # カスタム設定をロード
        custom_action = QAction("カスタム設定をロード", self)
        custom_action.triggered.connect(self.load_custom_mapping)
        file_menu.addAction(custom_action)
        
        file_menu.addSeparator()
        
        # 終了
        exit_action = QAction("終了", self)
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)
    
    def select_button(self, button):
        """ボタンを選択"""
        # 前に選択されていたボタンの選択を解除
        if self.selected_button:
            self.selected_button.set_selected(False)
        
        # ノブの選択も解除
        if self.selected_knob:
            self.selected_knob.set_selected(False)
            self.selected_knob = None
        
        # 新しいボタンを選択
        self.selected_button = button
        button.set_selected(True)
        self.statusBar().showMessage(f"位置 {button.grid_id} を選択しました。キーボードの対応するキーを押してください。")
    
    def select_knob(self, knob):
        """ノブを選択"""
        # 前に選択されていたノブの選択を解除
        if self.selected_knob:
            self.selected_knob.set_selected(False)
        
        # ボタンの選択も解除
        if self.selected_button:
            self.selected_button.set_selected(False)
            self.selected_button = None
        
        # 新しいノブを選択
        self.selected_knob = knob
        knob.set_selected(True)
        self.statusBar().showMessage(f"ノブ位置 {knob.ui_id} を選択しました。キーボードの対応するノブを押してください。")
    
    def handle_key_press(self, physical_id):
        """キー押下時の処理"""
        if self.selected_button:
            # 選択中のボタンに物理IDを設定
            self.selected_button.set_physical_id(physical_id)
            
            # マッピングを更新
            ui_id = self.selected_button.grid_id
            self.key_mapping[str(physical_id)] = ui_id
            
            self.statusBar().showMessage(f"位置 {ui_id} に物理キー {physical_id} をマッピングしました")
            
            # 選択を解除
            self.selected_button.set_selected(False)
            self.selected_button = None
    
    def handle_knob_press(self, physical_id):
        """ノブ押下時の処理"""
        if self.selected_knob:
            # 選択中のノブに物理IDを設定
            self.selected_knob.set_physical_id(physical_id)
            
            # マッピングを更新
            ui_id = self.selected_knob.ui_id
            self.knob_mapping[str(physical_id)] = ui_id
            
            self.statusBar().showMessage(f"ノブ位置 {ui_id} に物理ノブ {physical_id} をマッピングしました")
            
            # 選択を解除
            self.selected_knob.set_selected(False)
            self.selected_knob = None
    
    def handle_knob_rotation(self, physical_id):
        """ノブ回転時の処理"""
        # ノブが回転された場合、対応するノブをハイライト表示
        for ui_id, knob in self.knobs.items():
            if knob.physical_id == physical_id:
                knob.highlight_rotation()
                self.statusBar().showMessage(f"物理ノブ {physical_id} が回転されました")
                break
    
    def load_mapping(self, config_path=DEFAULT_CONFIG_PATH):
        """マッピング設定を読み込む"""
        try:
            if os.path.exists(config_path):
                with open(config_path, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                    self.key_mapping = config.get('key_mapping', {})
                    self.knob_mapping = config.get('knob_mapping', {})
                    self.statusBar().showMessage(f"マッピング設定を読み込みました: {config_path}")
            else:
                # デフォルト設定
                self.key_mapping = {str(i): i for i in range(1, 17)}
                self.knob_mapping = {str(i): i for i in range(1, 4)}
                self.statusBar().showMessage("デフォルトマッピングを使用します")
        except Exception as e:
            self.statusBar().showMessage(f"マッピング設定の読み込みエラー: {str(e)}")
            # エラー時はデフォルト設定
            self.key_mapping = {str(i): i for i in range(1, 17)}
            self.knob_mapping = {str(i): i for i in range(1, 4)}
        
        # マッピングをUIに適用
        self.apply_mapping()
    
    def apply_mapping(self):
        """現在のマッピングをUIに適用"""
        # キーマッピングの適用
        for physical_id, ui_id in self.key_mapping.items():
            if int(ui_id) in self.buttons:
                self.buttons[int(ui_id)].set_physical_id(int(physical_id))
        
        # ノブマッピングの適用
        for physical_id, ui_id in self.knob_mapping.items():
            if int(ui_id) in self.knobs:
                self.knobs[int(ui_id)].set_physical_id(int(physical_id))
    
    def save_mapping(self, config_path=DEFAULT_CONFIG_PATH):
        """マッピング設定を保存"""
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
    
    def load_mapping_from_file(self):
        """ファイル選択ダイアログからマッピング設定を読み込む"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "マッピング設定ファイルを開く", CONFIG_DIR, "JSON Files (*.json)"
        )
        if file_path:
            self.load_mapping(file_path)
    
    def save_mapping_to_file(self):
        """ファイル選択ダイアログでマッピング設定を保存"""
        file_path, _ = QFileDialog.getSaveFileName(
            self, "マッピング設定ファイルを保存", DEFAULT_CONFIG_PATH, "JSON Files (*.json)"
        )
        if file_path:
            self.save_mapping(file_path)
    
    def reset_mapping(self):
        """マッピングをリセット"""
        reply = QMessageBox.question(
            self, "マッピングリセットの確認", 
            "マッピングを初期状態（位置ID = 物理ID）にリセットしますか？",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No
        )
        
        if reply == QMessageBox.Yes:
            self.key_mapping = {str(i): i for i in range(1, 17)}
            self.knob_mapping = {str(i): i for i in range(1, 4)}
            self.apply_mapping()
            self.statusBar().showMessage("マッピングをリセットしました")
    
    def reset_to_default_mapping(self):
        """デフォルトマッピングに戻す"""
        reply = QMessageBox.question(
            self, "デフォルト設定の確認", 
            "マッピング設定をデフォルトに戻しますか？",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No
        )
        
        if reply == QMessageBox.Yes:
            self.load_mapping()
            self.statusBar().showMessage("デフォルトマッピングを適用しました")
    
    def load_custom_mapping(self):
        """カスタムマッピングをロード"""
        reply = QMessageBox.question(
            self, "カスタム設定の確認", 
            "カスタムマッピング設定をロードしますか？",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No
        )
        
        if reply == QMessageBox.Yes:
            self.load_mapping(CUSTOM_CONFIG_PATH)
            self.statusBar().showMessage("カスタムマッピングを適用しました")
    
    def keyboard_callback(self, report, updated_keys, updated_encoders, updated_encoder_buttons):
        """キーボードマネージャーからのコールバック"""
        # デバッグ情報を表示
        if updated_keys or updated_encoders or updated_encoder_buttons:
            debug_info = f"レポート: {' '.join([f'{b:02x}' for b in report[:16]])}"
            if updated_keys:
                key_info = ", ".join([f"キー{k}={'押' if s else '離'}" for k, s in updated_keys.items()])
                debug_info += f" | キー変化: {key_info}"
            
            if updated_encoders:
                enc_info = ", ".join([f"ノブ{e}={v}%" for e, v in updated_encoders.items()])
                debug_info += f" | ノブ回転: {enc_info}"
            
            if updated_encoder_buttons:
                btn_info = ", ".join([f"ノブ{b}ボタン={'押' if s else '離'}" for b, s in updated_encoder_buttons.items()])
                debug_info += f" | ノブボタン: {btn_info}"
            
            print(debug_info)
            self.statusBar().showMessage(debug_info)
        
        # キーの状態を更新
        for key_id, state in updated_keys.items():
            if state:  # キーが押されたとき
                # キー押下シグナル発行
                self.key_pressed.emit(key_id)
            
            # ボタン表示も更新
            for ui_id, button in self.buttons.items():
                if button.physical_id == key_id:
                    button.set_pressed(state)
        
        # エンコーダー（ノブ）の回転を検出
        for encoder_id, value in updated_encoders.items():
            # 回転シグナル発行
            self.knob_rotated.emit(encoder_id)
        
        # エンコーダー（ノブ）のボタン状態を更新
        for encoder_id, state in updated_encoder_buttons.items():
            if state:  # ノブが押されたとき
                # ノブ押下シグナル発行
                self.knob_pressed.emit(encoder_id)
            
            # ノブ表示も更新
            for ui_id, knob in self.knobs.items():
                if knob.physical_id == encoder_id:
                    knob.set_pressed(state)
    
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
            self.statusBar().showMessage(f"DOIO KB16が見つかりません (VID: 0x{DOIO_VENDOR_ID:04X}, PID: 0x{DOIO_PRODUCT_ID:04X})")
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
    window = MappingCalibrationTool()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
