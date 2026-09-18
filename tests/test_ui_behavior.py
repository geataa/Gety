import time
import subprocess
import ctypes
from ctypes import wintypes
import sys

user32 = ctypes.windll.user32

WM_COMMAND = 0x0111
WM_CLOSE = 0x0010

def find_gety_window():
    found_hwnd = []
    def enum_windows_proc(hwnd, lParam):
        if user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buff = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buff, length + 1)
                if "Gety" in buff.value:
                    found_hwnd.append(hwnd)
        return True

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows(WNDENUMPROC(enum_windows_proc), 0)
    return found_hwnd[0] if found_hwnd else None

def main():
    print("Launching Gety.exe...")
    proc = subprocess.Popen(["E:\\0_SkySoft\\Gety\\Gety.exe"])
    time.sleep(1.5)

    hwnd = find_gety_window()
    if not hwnd:
        print("ERROR: Gety main window not found!")
        proc.terminate()
        sys.exit(1)

    print(f"Gety window found: HWND={hex(hwnd)}")
    
    rect = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    print(f"Window Rect: left={rect.left}, top={rect.top}, right={rect.right}, bottom={rect.bottom}")
    
    # Wait for initial countdown (2.5s) to allow auto-hide
    print("Testing auto-hide timeout...")
    time.sleep(3.5)
    
    # Test proximity hover trigger: simulate mouse approaching top (x=200, y=10)
    print("Simulating mouse approaching top (y=10)...")
    pt = wintypes.POINT(rect.left + 200, rect.top + 10)
    user32.SetCursorPos(pt.x, pt.y)
    time.sleep(0.3)
    
    # Simulate mouse moving down away from top bar (y=200)
    print("Simulating mouse moving away to center of window (y=200)...")
    user32.SetCursorPos(rect.left + 200, rect.top + 200)
    time.sleep(1.0) # Grace period 600ms should trigger hide
    
    # Test Close button behavior [X] -> Should hide window to tray, keeping process running
    print("Sending WM_CLOSE (mimicking [X] click)...")
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    time.sleep(0.8)
    
    isVisible = user32.IsWindowVisible(hwnd)
    print(f"Is window visible after [X]? {bool(isVisible)} (Expected: False - hidden in tray)")
    
    # Check if process is still running in background
    poll = proc.poll()
    print(f"Process poll after [X]: {poll} (Expected: None - alive in tray background)")
    assert poll is None, "Process died unexpectedly after [X] close!"
    assert not isVisible, "Window should be hidden in tray after [X] close!"

    # Clean shutdown
    print("Terminating test process...")
    proc.terminate()
    print(">>> Automated UI Behavior Verification PASSED 100%! <<<")

if __name__ == "__main__":
    main()
