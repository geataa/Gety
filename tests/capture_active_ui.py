import time
import subprocess
import ctypes
from ctypes import wintypes
from PIL import Image
import os

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_MOUSEMOVE = 0x0200
WM_NCMOUSEMOVE = 0x00A0
HTCLIENT = 1

def capture_window_screen(hwnd, save_path):
    rect = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    
    hdc_screen = user32.GetDC(0)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbm = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
    old_bm = gdi32.SelectObject(hdc_mem, hbm)
    
    gdi32.BitBlt(hdc_mem, 0, 0, w, h, hdc_screen, rect.left, rect.top, 0x00CC0020)
    
    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ('biSize', wintypes.DWORD),
            ('biWidth', wintypes.LONG),
            ('biHeight', wintypes.LONG),
            ('biPlanes', wintypes.WORD),
            ('biBitCount', wintypes.WORD),
            ('biCompression', wintypes.DWORD),
            ('biSizeImage', wintypes.DWORD),
            ('biXPelsPerMeter', wintypes.LONG),
            ('biYPelsPerMeter', wintypes.LONG),
            ('biClrUsed', wintypes.DWORD),
            ('biClrImportant', wintypes.DWORD)
        ]

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = w
    bmi.biHeight = -h
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0

    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(hdc_mem, hbm, 0, h, buf, ctypes.byref(bmi), 0)

    gdi32.SelectObject(hdc_mem, old_bm)
    gdi32.DeleteObject(hbm)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(0, hdc_screen)

    img = Image.frombuffer('RGBA', (w, h), buf, 'raw', 'BGRA', 0, 1)
    img.save(save_path)
    print(f"Window screen crop saved to {save_path} ({w}x{h})")

def main():
    print("Launching Gety.exe...")
    proc = subprocess.Popen(["E:\\0_SkySoft\\Gety\\Gety.exe"])
    time.sleep(1.2)
    
    hwnd = user32.FindWindowW("GetyMainWindow", None)
    if not hwnd:
        print("ERROR: Window not found!")
        proc.terminate()
        return

    # Bring to foreground
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.3)

    # Hover near top bar (x=200, y=15)
    lParam = (15 << 16) | (200 & 0xFFFF)
    user32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, lParam)
    time.sleep(0.5)
        
    out_dir = r"C:\Users\ilter_zbhki5f\.gemini\antigravity\brain\4b578865-176b-4b39-a798-9f894b2fdd9d"
    out_path = os.path.join(out_dir, "gety_ui_fixed.png")
    
    capture_window_screen(hwnd, out_path)
    
    proc.terminate()
    print("Complete!")

if __name__ == "__main__":
    main()
