import time
import subprocess
import ctypes
from ctypes import wintypes
import sys
import os

user32 = ctypes.windll.user32

WM_COMMAND = 0x0111
WM_CLOSE = 0x0010
ID_FILE_NEW = 201
IDOK = 1

def main():
    os.system("taskkill /f /im Gety.exe >nul 2>&1")
    time.sleep(0.5)

    print("Launching Gety.exe...")
    proc = subprocess.Popen(["E:\\0_SkySoft\\Gety\\Gety.exe"])
    time.sleep(1.5)

    if proc.poll() is not None:
        print(f"Gety exited immediately with code {proc.returncode}")
        return

    hMain = user32.FindWindowW("GetyMainWindow", None)
    if not hMain:
        print("ERROR: GetyMainWindow not found")
        proc.terminate()
        return

    # Set clipboard with YouTube URL
    test_url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ"
    os.system(f'powershell -command "Set-Clipboard -Value \\"{test_url}\\""')
    time.sleep(0.5)

    print("Triggering ID_FILE_NEW with YouTube URL...")
    user32.PostMessageW(hMain, WM_COMMAND, ID_FILE_NEW, 0)
    time.sleep(1.0)

    hDlg = user32.FindWindowW("GetyNewDownloadDlg", None)
    if not hDlg:
        print("ERROR: GetyNewDownloadDlg not found")
        proc.terminate()
        return

    print("Clicking OK in New Download dialog...")
    user32.PostMessageW(hDlg, WM_COMMAND, IDOK, 0)

    print("Monitoring Gety process for crashes...")
    for i in range(16):
        time.sleep(0.5)
        ret = proc.poll()
        if ret is not None:
            print(f"CRASH DETECTED! Gety terminated with exit code {hex(ret & 0xFFFFFFFF)}")
            return
        print(f"Still running at {(i+1)*0.5}s...")

    print("SUCCESS: YouTube download started without crashing!")
    user32.PostMessageW(hMain, WM_CLOSE, 0, 0)
    time.sleep(1.0)
    if proc.poll() is None:
        proc.terminate()

if __name__ == "__main__":
    main()
