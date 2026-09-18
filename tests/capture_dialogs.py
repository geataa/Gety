import ctypes
from ctypes import wintypes
import subprocess
import time
import sys
import os
from PIL import ImageGrab

user32 = ctypes.windll.user32
shcore = ctypes.windll.shcore

# Enable Per-Monitor DPI awareness so coordinates match screen exactly
try:
    shcore.SetProcessDpiAwareness(2)
except Exception:
    user32.SetProcessDPIAware()

WM_COMMAND = 0x0111
WM_CLOSE = 0x0010
ID_FILE_NEW = 201
ID_FILE_BATCH = 202
ID_TOOLS_OPTIONS = 240
ID_HELP_ABOUT = 250

def capture_window(hwnd, output_path):
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.2)
    rc = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rc))
    bbox = (rc.left, rc.top, rc.right, rc.bottom)
    img = ImageGrab.grab(bbox=bbox)
    img.save(output_path)
    print(f"Captured {output_path} ({rc.right - rc.left}x{rc.bottom - rc.top})")

def main():
    print("Starting Gety...")
    proc = subprocess.Popen(["E:\\0_SkySoft\\Gety\\Gety.exe"])
    time.sleep(1.0)

    hMain = user32.FindWindowW("GetyMainWindow", None)
    if not hMain:
        print("ERROR: GetyMainWindow not found!")
        proc.kill()
        sys.exit(1)

    # 1. New Download Dialog
    user32.PostMessageW(hMain, WM_COMMAND, ID_FILE_NEW, 0)
    time.sleep(0.5)
    hDlg = user32.FindWindowW("GetyNewDownloadDlg", None)
    if hDlg:
        capture_window(hDlg, "C:\\Users\\ilter_zbhki5f\\.gemini\\antigravity\\brain\\4b578865-176b-4b39-a798-9f894b2fdd9d\\dlg_new.png")
        user32.PostMessageW(hDlg, WM_CLOSE, 0, 0)
        time.sleep(0.3)

    # 2. Batch Dialog
    user32.PostMessageW(hMain, WM_COMMAND, ID_FILE_BATCH, 0)
    time.sleep(0.5)
    hDlg = user32.FindWindowW("GetyBatchDownloadDlg", None)
    if hDlg:
        capture_window(hDlg, "C:\\Users\\ilter_zbhki5f\\.gemini\\antigravity\\brain\\4b578865-176b-4b39-a798-9f894b2fdd9d\\dlg_batch.png")
        user32.PostMessageW(hDlg, WM_CLOSE, 0, 0)
        time.sleep(0.3)

    # 3. Options Dialog
    user32.PostMessageW(hMain, WM_COMMAND, ID_TOOLS_OPTIONS, 0)
    time.sleep(0.5)
    hDlg = user32.FindWindowW("GetyOptionsDlg", None)
    if hDlg:
        capture_window(hDlg, "C:\\Users\\ilter_zbhki5f\\.gemini\\antigravity\\brain\\4b578865-176b-4b39-a798-9f894b2fdd9d\\dlg_options.png")
        user32.PostMessageW(hDlg, WM_CLOSE, 0, 0)
        time.sleep(0.3)

    # 4. About Dialog
    user32.PostMessageW(hMain, WM_COMMAND, ID_HELP_ABOUT, 0)
    time.sleep(0.5)
    hDlg = user32.FindWindowW("GetyAboutDlg", None)
    if hDlg:
        capture_window(hDlg, "C:\\Users\\ilter_zbhki5f\\.gemini\\antigravity\\brain\\4b578865-176b-4b39-a798-9f894b2fdd9d\\dlg_about.png")
        user32.PostMessageW(hDlg, WM_CLOSE, 0, 0)
        time.sleep(0.3)

    user32.PostMessageW(hMain, WM_CLOSE, 0, 0)
    proc.terminate()
    print("Done capturing dialogs!")

if __name__ == "__main__":
    main()
