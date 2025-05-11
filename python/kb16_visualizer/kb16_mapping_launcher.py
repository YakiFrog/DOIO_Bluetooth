#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_visualizer/kb16_mapping_launcher.py
# DOIO KB16 マッピング校正ツール ランチャー

import sys
import os
import subprocess

# 現在のディレクトリを取得
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))

def main():
    """マッピング校正ツールを起動"""
    # マッピング校正ツールのパス
    calibrator_path = os.path.join(CURRENT_DIR, "kb16_mapping_calibrator.py")
    
    if os.path.exists(calibrator_path):
        print("DOIO KB16 マッピング校正ツールを起動しています...")
        # スクリプトの実行権限を確認し、必要であれば付与
        if not os.access(calibrator_path, os.X_OK):
            os.chmod(calibrator_path, 0o755)
        
        # Pythonスクリプトを直接実行
        subprocess.run([sys.executable, calibrator_path])
    else:
        print(f"エラー: マッピング校正ツールが見つかりませんでした: {calibrator_path}")
        sys.exit(1)

if __name__ == "__main__":
    main()
