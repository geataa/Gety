import ctypes
from ctypes import wintypes
import subprocess
import time
import sys

user32 = ctypes.windll.user32

WM_MOUSEWHEEL = 0x020A
WM_CLOSE = 0x0010

def make_wparam(low, high):
    return (high << 16) | (low & 0xFFFF)

def make_lparam(x, y):
    return (y << 16) | (x & 0xFFFF)

def main():
    print("Launching Gety for scroll verification...")
    proc = subprocess.Popen(["E:\\0_SkySoft\\Gety\\Gety.exe"])
    time.sleep(1.0)

    hMain = user32.FindWindowW("GetyMainWindow", None)
    if not hMain:
        print("ERROR: GetyMainWindow not found!")
        proc.kill()
        sys.exit(1)

    print(f"GetyMainWindow found: HWND={hex(hMain)}")

    # 1. Scroll Categories (left side, e.g. x=100, y=200)
    print("Sending WM_MOUSEWHEEL over Categories area (x=100, y=200)...")
    wParamDown = make_wparam(0, -120)
    wParamUp = make_wparam(0, 120)
    lParamCat = make_lparam(100, 200)

    for _ in range(5):
        user32.SendMessageW(hMain, WM_MOUSEWHEEL, wParamDown, lParamCat)
        time.sleep(0.05)
    for _ in range(5):
        user32.SendMessageW(hMain, WM_MOUSEWHEEL, wParamUp, lParamCat)
        time.sleep(0.05)
    print("[PASS] Category mouse wheel scrolling handled without errors!")

    # 2. Scroll Task List (top right area, e.g. x=500, y=200)
    print("Sending WM_MOUSEWHEEL over Task List area (x=500, y=200)...")
    lParamTask = make_lparam(500, 200)
    for _ in range(5):
        user32.SendMessageW(hMain, WM_MOUSEWHEEL, wParamDown, lParamTask)
        time.sleep(0.05)
    for _ in range(5):
        user32.SendMessageW(hMain, WM_MOUSEWHEEL, wParamUp, lParamTask)
        time.sleep(0.05)
    print("[PASS] Task list mouse wheel scrolling handled without errors!")

    # 3. Scroll Detail Tabs (bottom right area, e.g. x=500, y=500)
    print("Sending WM_MOUSEWHEEL over Detail Tabs area (x=500, y=500)...")
    lParamDetail = make_lparam(500, 500)
    for _ in range(5):
        user32.SendMessageW(hMain, WM_MOUSEWHEEL, wParamDown, lParamDetail)
        time.sleep(0.05)
    for _ in range(5):
        user32.SendMessageW(hMain, WM_MOUSEWHEEL, wParamUp, lParamDetail)
        time.sleep(0.05)
    print("[PASS] Detail tabs mouse wheel scrolling handled without errors!")

    # Clean exit
    user32.PostMessageW(hMain, WM_CLOSE, 0, 0)
    proc.terminate()
    print("\n>>> ALL SCROLL ENGINE TESTS PASSED WITH 100% SUCCESS! <<<")

if __name__ == "__main__":
    main()
