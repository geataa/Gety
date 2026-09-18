import time
import subprocess
import ctypes
from ctypes import wintypes
from PIL import Image
import os

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

PW_RENDERFULLCONTENT = 0x00000002

def capture_window_gdi(hwnd, save_path):
    rect = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    
    hdc_window = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_window)
    hbm = gdi32.CreateCompatibleBitmap(hdc_window, w, h)
    old_bm = gdi32.SelectObject(hdc_mem, hbm)
    
    # Try PrintWindow
    res = user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT)
    if not res:
        # Fallback to normal PrintWindow
        user32.PrintWindow(hwnd, hdc_mem, 0)
        
    # Get bitmap bits into PIL
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
    bmi.biHeight = -h # top-down
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0 # BI_RGB

    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(hdc_mem, hbm, 0, h, buf, ctypes.byref(bmi), 0)

    # Cleanup
    gdi32.SelectObject(hdc_mem, old_bm)
    gdi32.DeleteObject(hbm)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(hwnd, hdc_window)

    img = Image.frombuffer('RGBA', (w, h), buf, 'raw', 'BGRA', 0, 1)
    img.save(save_path)
    print(f"GDI PrintWindow screenshot saved to {save_path} ({w}x{h})")

def main():
    print("Launching Gety.exe...")
    proc = subprocess.Popen(["E:\\0_SkySoft\\Gety\\Gety.exe"])
    time.sleep(2.0)
    
    hwnd = user32.FindWindowW("GetyMainWindow", None)
    if not hwnd:
        print("ERROR: Window not found!")
        proc.terminate()
        return
        
    out_dir = r"C:\Users\ilter_zbhki5f\.gemini\antigravity\brain\4b578865-176b-4b39-a798-9f894b2fdd9d"
    out_path = os.path.join(out_dir, "gety_screenshot.png")
    
    capture_window_gdi(hwnd, out_path)
    
    proc.terminate()
    print("Done.")

if __name__ == "__main__":
    main()
