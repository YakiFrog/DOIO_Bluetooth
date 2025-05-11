#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_visualizer_launcher.py
"""
DOIO KB16 可視化ツール ランチャー
可視化ツールを簡単に起動するためのスクリプト
"""

import os
import sys
import subprocess
import argparse

def main():
    """メイン関数"""
    parser = argparse.ArgumentParser(description='DOIO KB16 可視化ツールランチャー')
    parser.add_argument('--mode', choices=['realtime', 'offline'], default='realtime',
                      help='実行モード: "realtime"（リアルタイム可視化）または "offline"（オフライン可視化）')
    parser.add_argument('--file', help='オフラインモードで使用するキャプチャファイルのパス')
    args = parser.parse_args()
    
    # 可視化ツールのディレクトリパス
    visualizer_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'kb16_visualizer')
    
    # 依存関係インストーラーの実行
    setup_script = os.path.join(visualizer_dir, 'setup_requirements.py')
    if os.path.exists(setup_script):
        print("依存パッケージの確認中...")
        try:
            subprocess.run([sys.executable, setup_script], check=True)
        except subprocess.CalledProcessError:
            print("依存パッケージのインストールに問題が発生しました。")
            return 1
    
    # 選択されたモードに基づいて適切なスクリプトを起動
    if args.mode == 'realtime':
        script_path = os.path.join(visualizer_dir, 'kb16_visual_ui.py')
        print("\nDOIO KB16 リアルタイム可視化ツールを起動中...\n")
        try:
            subprocess.run([sys.executable, script_path], check=True)
        except KeyboardInterrupt:
            print("\n可視化ツールを終了しました。")
        except subprocess.CalledProcessError as e:
            print(f"エラー: 可視化ツールの実行中にエラーが発生しました。 (コード: {e.returncode})")
            return e.returncode
    
    else:  # offlineモード
        script_path = os.path.join(visualizer_dir, 'kb16_offline_visualizer.py')
        print("\nDOIO KB16 オフライン可視化ツールを起動中...\n")
        
        cmd = [sys.executable, script_path]
        if args.file:
            cmd.extend(['--file', args.file])
        
        try:
            subprocess.run(cmd, check=True)
        except KeyboardInterrupt:
            print("\n可視化ツールを終了しました。")
        except subprocess.CalledProcessError as e:
            print(f"エラー: 可視化ツールの実行中にエラーが発生しました。 (コード: {e.returncode})")
            return e.returncode
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
