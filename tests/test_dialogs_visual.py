import ctypes
from ctypes import wintypes
import subprocess
import time
import sys
import os

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_COMMAND = 0x0111
WM_CLOSE = 0x0010
ID_FILE_NEW = 201
ID_FILE_BATCH = 202
ID_TOOLS_OPTIONS = 240
ID_HELP_ABOUT = 250

def main():
    print("Starting Gety for dialog testing...")
    proc = subprocess.Popen(["E:\\0_SkySoft\\Gety\\Gety.exe"])
    time.sleep(1.0)

    hMain = user32.FindWindowW("GetyMainWindow", None)
    if not hMain:
        print("ERROR: GetyMainWindow not found!")
        proc.kill()
        sys.exit(1)

    print(f"GetyMainWindow found: HWND={hex(hMain)}")

    # Test 1: New Download Dialog
    print("\n[Test 1] Opening New Download Dialog...")
    user32.PostMessageW(hMain, WM_COMMAND, ID_FILE_NEW, 0)
    time.sleep(0.5)

    hNewDlg = user32.FindWindowW("GetyNewDownloadDlg", None)
    if not hNewDlg:
        print("FAILED: GetyNewDownloadDlg not found!")
        proc.kill()
        sys.exit(1)
    
    rc = wintypes.RECT()
    user32.GetWindowRect(hNewDlg, ctypes.byref(rc))
    print(f"GetyNewDownloadDlg active! Pos: ({rc.left},{rc.top}) Size: {rc.right - rc.left}x{rc.bottom - rc.top}")
    user32.PostMessageW(hNewDlg, WM_CLOSE, 0, 0)
    time.sleep(0.3)

    # Test 2: Batch Download Dialog
    print("\n[Test 2] Opening Batch Download Dialog...")
    user32.PostMessageW(hMain, WM_COMMAND, ID_FILE_BATCH, 0)
    time.sleep(0.5)

    hBatchDlg = user32.FindWindowW("GetyBatchDownloadDlg", None)
    if not hBatchDlg:
        print("FAILED: GetyBatchDownloadDlg not found!")
        proc.kill()
        sys.exit(1)

    user32.GetWindowRect(hBatchDlg, ctypes.byref(rc))
    print(f"GetyBatchDownloadDlg active! Pos: ({rc.left},{rc.top}) Size: {rc.right - rc.left}x{rc.bottom - rc.top}")
    user32.PostMessageW(hBatchDlg, WM_CLOSE, 0, 0)
    time.sleep(0.3)

    # Test 3: Options Dialog
    print("\n[Test 3] Opening Options Dialog...")
    user32.PostMessageW(hMain, WM_COMMAND, ID_TOOLS_OPTIONS, 0)
    time.sleep(0.5)

    hOptDlg = user32.FindWindowW("GetyOptionsDlg", None)
    if not hOptDlg:
        print("FAILED: GetyOptionsDlg not found!")
        proc.kill()
        sys.exit(1)

    user32.GetWindowRect(hOptDlg, ctypes.byref(rc))
    print(f"GetyOptionsDlg active! Pos: ({rc.left},{rc.top}) Size: {rc.right - rc.left}x{rc.bottom - rc.top}")
    user32.PostMessageW(hOptDlg, WM_CLOSE, 0, 0)
    time.sleep(0.3)

    # Test 4: About Dialog
    print("\n[Test 4] Opening About Dialog...")
    user32.PostMessageW(hMain, WM_COMMAND, ID_HELP_ABOUT, 0)
    time.sleep(0.5)

    hAboutDlg = user32.FindWindowW("GetyAboutDlg", None)
    if not hAboutDlg:
        print("FAILED: GetyAboutDlg not found!")
        proc.kill()
        sys.exit(1)

    user32.GetWindowRect(hAboutDlg, ctypes.byref(rc))
    print(f"GetyAboutDlg active! Pos: ({rc.left},{rc.top}) Size: {rc.right - rc.left}x{rc.bottom - rc.top}")
    user32.PostMessageW(hAboutDlg, WM_CLOSE, 0, 0)
    time.sleep(0.3)

    # Clean exit
    user32.PostMessageW(hMain, WM_CLOSE, 0, 0)
    proc.terminate()
    print("\n>>> ALL 4 CUSTOM DIALOGS OPENED AND CLOSED PERFECTLY! <<<")

if __name__ == "__main__":
    main()
