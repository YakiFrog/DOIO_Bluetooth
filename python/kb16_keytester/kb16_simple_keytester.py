#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
DOIO KB16 シンプルキーテスター
PySide6を使用してキー入力を視覚的に表示します
"""

import sys
import os
import json
from pathlib import Path
from PySide6.QtCore import Qt, QTimer, Signal, QObject
from PySide6.QtGui import QKeyEvent, QFont
from PySide6.QtWidgets import (QApplication, QMainWindow, QGridLayout, 
                               QPushButton, QVBoxLayout, QHBoxLayout, 
                               QLabel, QWidget, QFileDialog, QMenuBar, QMenu, 
                               QMessageBox)


class KeyboardConfig:
    """キーボード設定を扱うクラス"""
    
    def __init__(self):
        # デフォルトのキー配列
        self.key_layout = [
            ['a', 'b', 'c', 'd'],
            ['e', 'f', 'g', 'h'],
            ['i', 'j', 'k', 'l'],
            ['m', 'n', 'o', 'p']
        ]
        
        # デフォルトのキーマッピング
        self.key_mapping = {
            "a": "Qt.Key_A",
            "b": "Qt.Key_B",
            "c": "Qt.Key_C",
            "d": "Qt.Key_D",
            "e": "Qt.Key_E",
            "f": "Qt.Key_F",
            "g": "Qt.Key_G",
            "h": "Qt.Key_H",
            "i": "Qt.Key_I",
            "j": "Qt.Key_J",
            "k": "Qt.Key_K",
            "l": "Qt.Key_L",
            "m": "Qt.Key_M",
            "n": "Qt.Key_N",
            "o": "Qt.Key_O",
            "p": "Qt.Key_P"
        }
    
    def save_config(self, filepath):
        """設定をJSONファイルに保存"""
        config = {
            "key_layout": self.key_layout,
            "key_mapping": self.key_mapping
        }
        
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                json.dump(config, f, indent=4)
            return True
        except Exception as e:
            print(f"設定の保存に失敗: {e}")
            return False
    
    def load_config(self, filepath):
        """JSONファイルから設定を読み込み"""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                config = json.load(f)
                
            if "key_layout" in config:
                self.key_layout = config["key_layout"]
            if "key_mapping" in config:
                self.key_mapping = config["key_mapping"]
            
            return True
        except Exception as e:
            print(f"設定の読み込みに失敗: {e}")
            return False
            
    def create_default_config(self, filepath):
        """デフォルト設定ファイルを作成"""
        return self.save_config(filepath)


class KB16KeyTester(QMainWindow):
    """DOIO KB16用のキーテスターウィンドウ"""
    
    def __init__(self):
        super().__init__()
        self.setWindowTitle("DOIO KB16 キーテスター")
        self.setGeometry(300, 300, 500, 450)
        
        # キーボード設定
        self.config = KeyboardConfig()
        self.config_path = None
        
        # メニューバーを設定
        self.setup_menu_bar()
        
        # メインウィジェットとレイアウトのセットアップ
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)
        
        # 情報表示エリア
        info_layout = QHBoxLayout()
        self.last_key_label = QLabel("最後に押されたキー: なし")
        self.last_key_code_label = QLabel("キーコード: なし")
        self.last_key_name_label = QLabel("キー名: なし")
        
        info_layout.addWidget(self.last_key_label)
        info_layout.addWidget(self.last_key_code_label)
        info_layout.addWidget(self.last_key_name_label)
        
        main_layout.addLayout(info_layout)
        
        # キーグリッドの設定（4x4グリッド）
        grid_layout = QGridLayout()
        self.key_buttons = []
        
        # 4x4のキーボタンを作成
        for row in range(4):
            row_buttons = []
            for col in range(4):
                # 設定に基づいたキーラベルを使用
                key_label = self.config.key_layout[row][col].upper() if row < len(self.config.key_layout) and col < len(self.config.key_layout[row]) else f"{row}{col}"
                button = QPushButton(key_label)
                button.setFixedSize(70, 70)
                button.setCheckable(True)
                font = QFont()
                font.setPointSize(14)
                button.setFont(font)
                grid_layout.addWidget(button, row, col)
                row_buttons.append(button)
            self.key_buttons.append(row_buttons)
        
        main_layout.addLayout(grid_layout)
        
        # ステータス表示エリア
        self.status_label = QLabel("キーを押してください...")
        main_layout.addWidget(self.status_label)
        
        # キーリセットタイマー（キーが押されて一定時間後にリセット）
        self.reset_timer = QTimer(self)
        self.reset_timer.setSingleShot(True)
        self.reset_timer.timeout.connect(self.reset_key_highlight)
        
        # キーマッピング（キーコードからボタン位置へのマッピング）
        # QT Key code -> (row, col) のマッピング
        # KB16の実際のキー配置: 'abcd efgh ijkl mnop'に対応
        self.key_mapping = {
            # アルファベットキー (1行1列目から abcd efgh ijkl mnop)
            Qt.Key_A: (0, 0), Qt.Key_B: (0, 1), Qt.Key_C: (0, 2), Qt.Key_D: (0, 3),
            Qt.Key_E: (1, 0), Qt.Key_F: (1, 1), Qt.Key_G: (1, 2), Qt.Key_H: (1, 3),
            Qt.Key_I: (2, 0), Qt.Key_J: (2, 1), Qt.Key_K: (2, 2), Qt.Key_L: (2, 3),
            Qt.Key_M: (3, 0), Qt.Key_N: (3, 1), Qt.Key_O: (3, 2), Qt.Key_P: (3, 3),
            
            # 数字キー（テンキー以外）- 実際のキーボードに応じて調整が必要
            Qt.Key_1: (0, 0), Qt.Key_2: (0, 1), Qt.Key_3: (0, 2), Qt.Key_4: (0, 3),
            Qt.Key_5: (1, 0), Qt.Key_6: (1, 1), Qt.Key_7: (1, 2), Qt.Key_8: (1, 3),
            Qt.Key_9: (2, 0), Qt.Key_0: (2, 1),
            
            # テンキー - 実際のキーボードに応じて調整が必要
            Qt.Key_7 + Qt.KeypadModifier: (0, 0), Qt.Key_8 + Qt.KeypadModifier: (0, 1),
            Qt.Key_9 + Qt.KeypadModifier: (0, 2), Qt.Key_Slash: (0, 3),
            Qt.Key_4 + Qt.KeypadModifier: (1, 0), Qt.Key_5 + Qt.KeypadModifier: (1, 1),
            Qt.Key_6 + Qt.KeypadModifier: (1, 2), Qt.Key_Asterisk: (1, 3),
            Qt.Key_1 + Qt.KeypadModifier: (2, 0), Qt.Key_2 + Qt.KeypadModifier: (2, 1),
            Qt.Key_3 + Qt.KeypadModifier: (2, 2), Qt.Key_Minus: (2, 3),
            Qt.Key_0 + Qt.KeypadModifier: (3, 0), Qt.Key_Period + Qt.KeypadModifier: (3, 1),
            Qt.Key_Enter + Qt.KeypadModifier: (3, 2), Qt.Key_Plus: (3, 3),
            
            # ファンクションキー - 実際のキーボードに応じて調整が必要
            Qt.Key_F1: (0, 0), Qt.Key_F2: (0, 1), Qt.Key_F3: (0, 2), Qt.Key_F4: (0, 3),
            Qt.Key_F5: (1, 0), Qt.Key_F6: (1, 1), Qt.Key_F7: (1, 2), Qt.Key_F8: (1, 3),
            Qt.Key_F9: (2, 0), Qt.Key_F10: (2, 1), Qt.Key_F11: (2, 2), Qt.Key_F12: (2, 3),
            Qt.Key_F13: (3, 0), Qt.Key_F14: (3, 1), Qt.Key_F15: (3, 2), Qt.Key_F16: (3, 3),
        }
        
    def setup_menu_bar(self):
        """メニューバーをセットアップ"""
        menubar = QMenuBar(self)
        self.setMenuBar(menubar)
        
        # ファイルメニュー
        file_menu = QMenu("ファイル", self)
        menubar.addMenu(file_menu)
        
        # 設定メニュー項目
        load_config_action = file_menu.addAction("設定を読み込む")
        load_config_action.triggered.connect(self.load_config_dialog)
        
        save_config_action = file_menu.addAction("設定を保存")
        save_config_action.triggered.connect(self.save_config_dialog)
        
        create_default_action = file_menu.addAction("デフォルト設定を作成")
        create_default_action.triggered.connect(self.create_default_config_dialog)
        
        file_menu.addSeparator()
        
        exit_action = file_menu.addAction("終了")
        exit_action.triggered.connect(self.close)
    
    def load_config_dialog(self):
        """設定ファイルを読み込むダイアログ"""
        filepath, _ = QFileDialog.getOpenFileName(
            self, "設定ファイルを開く", "", "JSON ファイル (*.json)"
        )
        
        if filepath:
            if self.config.load_config(filepath):
                self.config_path = filepath
                self.update_keyboard_layout()
                QMessageBox.information(self, "成功", "設定を読み込みました")
            else:
                QMessageBox.warning(self, "エラー", "設定の読み込みに失敗しました")
    
    def save_config_dialog(self):
        """設定ファイルを保存するダイアログ"""
        filepath, _ = QFileDialog.getSaveFileName(
            self, "設定ファイルを保存", "", "JSON ファイル (*.json)"
        )
        
        if filepath:
            if not filepath.endswith('.json'):
                filepath += '.json'
                
            if self.config.save_config(filepath):
                self.config_path = filepath
                QMessageBox.information(self, "成功", "設定を保存しました")
            else:
                QMessageBox.warning(self, "エラー", "設定の保存に失敗しました")
    
    def create_default_config_dialog(self):
        """デフォルト設定ファイルを作成するダイアログ"""
        filepath, _ = QFileDialog.getSaveFileName(
            self, "デフォルト設定ファイルを作成", "", "JSON ファイル (*.json)"
        )
        
        if filepath:
            if not filepath.endswith('.json'):
                filepath += '.json'
                
            if self.config.create_default_config(filepath):
                self.config_path = filepath
                QMessageBox.information(self, "成功", "デフォルト設定ファイルを作成しました")
            else:
                QMessageBox.warning(self, "エラー", "設定ファイルの作成に失敗しました")
    
    def update_keyboard_layout(self):
        """設定に基づいてキーボードレイアウトを更新"""
        # キーボタンのラベルを更新
        for row in range(4):
            for col in range(4):
                if row < len(self.config.key_layout) and col < len(self.config.key_layout[row]):
                    key_label = self.config.key_layout[row][col].upper()
                    self.key_buttons[row][col].setText(key_label)
    
    def keyPressEvent(self, event: QKeyEvent):
        """キー押下イベント処理"""
        key_code = event.key()
        key_text = event.text()
        key_name = self._get_key_name(key_code)
        
        # キーコードとテキストを表示
        self.last_key_label.setText(f"最後に押されたキー: {key_text}")
        self.last_key_code_label.setText(f"キーコード: {key_code} (0x{key_code:04X})")
        self.last_key_name_label.setText(f"キー名: {key_name}")
        
        # NumPadキーの場合の特別処理
        if event.modifiers() & Qt.KeypadModifier:
            key_code += Qt.KeypadModifier
        
        # マッピングからボタン位置を取得
        if key_code in self.key_mapping:
            row, col = self.key_mapping[key_code]
            if 0 <= row < 4 and 0 <= col < 4:
                button = self.key_buttons[row][col]
                button.setChecked(True)
                self.status_label.setText(f"キーが検出されました: {key_name} (位置: {row+1}行 {col+1}列)")
                
                # リセットタイマーをスタート
                self.reset_timer.start(500)
        
        # 親クラスのイベント処理も呼び出す
        super().keyPressEvent(event)

    def reset_key_highlight(self):
        """ハイライト表示をリセット"""
        for row_buttons in self.key_buttons:
            for button in row_buttons:
                button.setChecked(False)
    
    def _get_key_name(self, key_code):
        """キーコードからキー名を取得"""
        # QT KeyのEnumから名前を取得
        for key, value in vars(Qt).items():
            if isinstance(value, int) and value == key_code and key.startswith('Key_'):
                return key
        return f"不明なキー ({key_code})"


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = KB16KeyTester()
    window.show()
    sys.exit(app.exec())
