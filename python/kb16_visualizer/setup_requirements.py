#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_visualizer/setup_requirements.py
"""
DOIO KB16 可視化ツールの依存パッケージをインストールするスクリプト
"""

import sys
import os
import subprocess
import platform

def check_and_install_package(package_name):
    """パッケージがインストールされているかチェックし、なければインストールする"""
    print(f"{package_name}の状態を確認中...")
    
    try:
        # パッケージのインポートを試みる
        if package_name == 'PySide6':
            try:
                import PySide6
                print(f"✓ {package_name}はすでにインストールされています (バージョン: {PySide6.__version__})")
                return True
            except ImportError:
                pass
        elif package_name == 'hidapi':
            try:
                import hid
                print(f"✓ {package_name}はすでにインストールされています")
                return True
            except ImportError:
                pass
        
        # インストールを実行
        print(f"{package_name}をインストール中...")
        pip_command = [sys.executable, "-m", "pip", "install", package_name]
        subprocess.check_call(pip_command)
        print(f"✓ {package_name}のインストールが完了しました")
        return True
    
    except Exception as e:
        print(f"✗ {package_name}のインストールに失敗しました: {e}")
        return False

def main():
    """メイン関数"""
    print("DOIO KB16 可視化ツールの依存パッケージインストーラー")
    print("=" * 60)
    print(f"Python バージョン: {sys.version}")
    print(f"OS: {platform.system()} {platform.release()}")
    print("=" * 60)
    
    # 必要なパッケージのリスト
    packages = ['PySide6', 'hidapi']
    
    # インストール状況をチェック
    all_installed = True
    for package in packages:
        if not check_and_install_package(package):
            all_installed = False
    
    if all_installed:
        print("\n✓ すべての依存パッケージがインストールされています。")
        print("可視化ツールを実行するには以下のコマンドを使用してください:")
        print("\nリアルタイム可視化:")
        print(f"  python {os.path.join(os.path.dirname(__file__), 'kb16_visual_ui.py')}")
        print("\nオフライン可視化:")
        print(f"  python {os.path.join(os.path.dirname(__file__), 'kb16_offline_visualizer.py')}")
    else:
        print("\n✗ 一部のパッケージのインストールに失敗しました。")
        print("手動でインストールを試みてください:")
        print("  pip install PySide6 hidapi")

if __name__ == "__main__":
    main()
