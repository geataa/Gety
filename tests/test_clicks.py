import time
import subprocess
import ctypes
from ctypes import wintypes
import sys

user32 = ctypes.windll.user32

WM_NCHITTEST = 0x0084
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
WM_COMMAND = 0x0111
WM_CLOSE = 0x0010
HTCLIENT = 1
HTCAPTION = 2

def find_window(class_name, title=None):
    return user32.FindWindowW(class_name, title)

def main():
    print("Launching Gety.exe...")
    proc = subprocess.Popen(["E:\\0_SkySoft\\Gety\\Gety.exe"])
    time.sleep(1.2)

    # 1. Verify DropZone is NOT visible
    hDropZone = find_window("GetyFloatingDropZone", None)
    if hDropZone and user32.IsWindowVisible(hDropZone):
        print("ERROR: Floating DropZone is visible on desktop!")
        proc.terminate()
        sys.exit(1)
    print("[PASS] DropZone box is NOT visible on desktop!")

    # 2. Find Gety main window
    hMain = find_window("GetyMainWindow", None)
    if not hMain:
        print("ERROR: GetyMainWindow not found!")
        proc.terminate()
        sys.exit(1)

    rect = wintypes.RECT()
    user32.GetWindowRect(hMain, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    print(f"Gety Window: HWND={hex(hMain)}, Size={w}x{h}")

    # 3. Test WM_NCHITTEST on close button [X] (approx x=w-20, y=19)
    screenX = rect.left + w - 20
    screenY = rect.top + 19
    lParam = (screenY << 16) | (screenX & 0xFFFF)
    hitClose = user32.SendMessageW(hMain, WM_NCHITTEST, 0, lParam)
    print(f"NCHITTEST over Close button: {hitClose} (Expected {HTCLIENT} HTCLIENT)")
    assert hitClose == HTCLIENT, f"Close button returned {hitClose} instead of HTCLIENT!"

    # 4. Test WM_NCHITTEST on 'Yeni' button (approx x=150, y=19)
    screenX_btn = rect.left + 150
    screenY_btn = rect.top + 19
    lParam_btn = (screenY_btn << 16) | (screenX_btn & 0xFFFF)
    hitBtn = user32.SendMessageW(hMain, WM_NCHITTEST, 0, lParam_btn)
    print(f"NCHITTEST over Action Button 'Yeni': {hitBtn} (Expected {HTCLIENT} HTCLIENT)")
    assert hitBtn == HTCLIENT, f"Action button returned {hitBtn} instead of HTCLIENT!"

    # 5. Test Click on Close button [X] -> Should hide window to tray!
    clientCloseX = w - 20
    clientCloseY = 19
    clientLParam = (clientCloseY << 16) | (clientCloseX & 0xFFFF)
    print("Clicking Close [X] button via WM_LBUTTONDOWN...")
    user32.PostMessageW(hMain, WM_LBUTTONDOWN, 0x0001, clientLParam)
    time.sleep(0.5)

    isVis = user32.IsWindowVisible(hMain)
    print(f"Window visible after clicking [X]: {bool(isVis)} (Expected False - hidden in tray)")
    assert not isVis, "Window failed to hide to tray on [X] click!"

    # Clean up
    proc.terminate()
    print(">>> ALL CLICK & UI INTERACTIVITY TESTS PASSED 100%! <<<")

if __name__ == "__main__":
    main()
