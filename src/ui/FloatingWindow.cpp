#include "FloatingWindow.h"
#include "Theme.h"
#include "resource.h"
#include "../core/Config.h"
#include "../core/I18n.h"
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <wincodec.h>

namespace Gety {

static const wchar_t* FLOATING_WINDOW_CLASS = L"GetyFloatingDropZone";

FloatingWindow::FloatingWindow() = default;

FloatingWindow::~FloatingWindow() {
    if (m_hTitleFont) { DeleteObject(m_hTitleFont); m_hTitleFont = NULL; }
    if (m_hSpeedFont) { DeleteObject(m_hSpeedFont); m_hSpeedFont = NULL; }
    if (m_hSubFont) { DeleteObject(m_hSubFont); m_hSubFont = NULL; }
    if (m_hIdleFont) { DeleteObject(m_hIdleFont); m_hIdleFont = NULL; }
}

void FloatingWindow::RecreateFonts() {
    if (m_hTitleFont) { DeleteObject(m_hTitleFont); m_hTitleFont = NULL; }
    if (m_hSpeedFont) { DeleteObject(m_hSpeedFont); m_hSpeedFont = NULL; }
    if (m_hSubFont) { DeleteObject(m_hSubFont); m_hSubFont = NULL; }
    if (m_hIdleFont) { DeleteObject(m_hIdleFont); m_hIdleFont = NULL; }

    m_hTitleFont = Theme::CreateAppFontForDpi(m_dpi, 8, FW_BOLD);
    m_hSpeedFont = Theme::CreateAppFontForDpi(m_dpi, 11, FW_BOLD);
    m_hSubFont = Theme::CreateAppFontForDpi(m_dpi, 7, FW_NORMAL);
    m_hIdleFont = Theme::CreateAppFontForDpi(m_dpi, 8, FW_SEMIBOLD);
}

void FloatingWindow::UpdateShape() {
    if (!m_hwnd) return;
    int r = MulDiv(10, m_dpi, 96);
    HRGN hRgn = CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, r * 2, r * 2);
    if (hRgn) {
        SetWindowRgn(m_hwnd, hRgn, TRUE);
    }
}

bool FloatingWindow::Create(HWND hMainWnd, int x, int y, int opacity) {
    m_hMainWnd = hMainWnd;
    m_opacity = opacity;

    m_dpi = Theme::GetWindowDpi(hMainWnd);
    m_width = MulDiv(m_baseWidth, m_dpi, 96);
    m_height = MulDiv(m_baseHeight, m_dpi, 96);
    RecreateFonts();

    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = FLOATING_WINDOW_CLASS;

        RegisterClassExW(&wc);
        classRegistered = true;
    }

    // Ensure within virtual screen bounds
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (x < 0 || x > screenW - m_width) x = screenW - m_width - 40;
    if (y < 0 || y > screenH - m_height) y = 100;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_ACCEPTFILES,
        FLOATING_WINDOW_CLASS,
        L"Gety",
        WS_POPUP,
        x, y, m_width, m_height,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        this
    );

    if (m_hwnd) {
        DragAcceptFiles(m_hwnd, TRUE);
        SetLayeredWindowAttributes(m_hwnd, 0, (BYTE)m_opacity, LWA_ALPHA);
        UpdateShape();
    }

    return (m_hwnd != NULL);
}

void FloatingWindow::Show(bool show) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, show ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}

bool FloatingWindow::IsVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void FloatingWindow::SetOpacity(int opacity) {
    m_opacity = std::clamp(opacity, 50, 255);
    if (m_hwnd) {
        SetLayeredWindowAttributes(m_hwnd, 0, (BYTE)m_opacity, LWA_ALPHA);
    }
}

void FloatingWindow::UpdateStats(double totalSpeedBps, int activeTasks, double totalProgress) {
    m_speedBps = totalSpeedBps;
    m_activeCount = activeTasks;
    m_progress = totalProgress;
    m_animTick = (m_animTick + 1) % 8;

    if (m_hwnd && IsWindowVisible(m_hwnd)) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

LRESULT CALLBACK FloatingWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FloatingWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (FloatingWindow*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (FloatingWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        switch (msg) {
            case WM_LBUTTONDOWN:
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;

            case WM_LBUTTONDBLCLK:
                if (pThis->m_hMainWnd) {
                    ShowWindow(pThis->m_hMainWnd, SW_RESTORE);
                    SetForegroundWindow(pThis->m_hMainWnd);
                }
                return 0;

            case WM_RBUTTONUP: {
                POINT pt;
                GetCursorPos(&pt);
                pThis->ShowContextMenu(pt.x, pt.y);
                return 0;
            }

            case WM_DROPFILES: {
                HDROP hDrop = (HDROP)wParam;
                wchar_t filePath[MAX_PATH];
                if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH)) {
                    wchar_t* pCopy = _wcsdup(filePath);
                    PostMessageW(pThis->m_hMainWnd, WM_APP_DROPZONE_DROP, 0, (LPARAM)pCopy);
                }
                DragFinish(hDrop);
                return 0;
            }

            case WM_MOVE: {
                RECT rc;
                GetWindowRect(hwnd, &rc);
                Config::Instance().dropZoneX = rc.left;
                Config::Instance().dropZoneY = rc.top;
                Config::Instance().Save();
                return 0;
            }

            case WM_SIZE: {
                pThis->m_width = LOWORD(lParam);
                pThis->m_height = HIWORD(lParam);
                pThis->UpdateShape();
                return 0;
            }

            case WM_DPICHANGED: {
                RECT* prc = (RECT*)lParam;
                pThis->m_dpi = LOWORD(wParam);
                pThis->m_width = MulDiv(pThis->m_baseWidth, pThis->m_dpi, 96);
                pThis->m_height = MulDiv(pThis->m_baseHeight, pThis->m_dpi, 96);
                if (prc) {
                    SetWindowPos(hwnd, NULL, prc->left, prc->top, pThis->m_width, pThis->m_height,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                } else {
                    SetWindowPos(hwnd, NULL, 0, 0, pThis->m_width, pThis->m_height,
                                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                }
                pThis->RecreateFonts();
                pThis->UpdateShape();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            case WM_ERASEBKGND:
                return 1;

            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                pThis->OnPaint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void FloatingWindow::ShowContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_FILE_NEW, LStr(StrId::MenuFileNew));
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESTORE, LStr(StrId::TrayRestore));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    HMENU hOpacity = CreatePopupMenu();
    AppendMenuW(hOpacity, MF_STRING | (m_opacity <= 130 ? MF_CHECKED : 0), ID_DROPZONE_OPACITY50, LStr(StrId::DropZoneOpacity50));
    AppendMenuW(hOpacity, MF_STRING | (m_opacity > 130 && m_opacity < 230 ? MF_CHECKED : 0), ID_DROPZONE_OPACITY75, LStr(StrId::DropZoneOpacity75));
    AppendMenuW(hOpacity, MF_STRING | (m_opacity >= 230 ? MF_CHECKED : 0), ID_DROPZONE_OPACITY100, LStr(StrId::DropZoneOpacity100));
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hOpacity, LStr(StrId::DropZoneOpacity));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_DROPZONE_HIDE, LStr(StrId::DropZoneHide));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_FILE_EXIT, LStr(StrId::MenuFileExit));

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);

    if (cmd == ID_FILE_NEW || cmd == ID_FILE_EXIT) {
        PostMessageW(m_hMainWnd, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
    } else if (cmd == ID_TRAY_RESTORE) {
        ShowWindow(m_hMainWnd, SW_RESTORE);
        SetForegroundWindow(m_hMainWnd);
    } else if (cmd == ID_DROPZONE_OPACITY50) {
        SetOpacity(128);
        Config::Instance().dropZoneOpacity = 128;
        Config::Instance().Save();
    } else if (cmd == ID_DROPZONE_OPACITY75) {
        SetOpacity(192);
        Config::Instance().dropZoneOpacity = 192;
        Config::Instance().Save();
    } else if (cmd == ID_DROPZONE_OPACITY100) {
        SetOpacity(255);
        Config::Instance().dropZoneOpacity = 255;
        Config::Instance().Save();
    } else if (cmd == ID_DROPZONE_HIDE) {
        Show(false);
        Config::Instance().showDropZone = false;
        Config::Instance().Save();
        PostMessageW(m_hMainWnd, WM_COMMAND, MAKEWPARAM(ID_VIEW_DROPZONE, 0), 0);
    }
}

void FloatingWindow::OnPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    bool isDownloading = (m_activeCount > 0 || m_speedBps > 10.0);

    // 1. Background Card (#0d1017 or #10141f)
    COLORREF bgCol = isDownloading ? RGB(14, 18, 28) : RGB(13, 16, 24);
    HBRUSH bgBrush = CreateSolidBrush(bgCol);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // 2. Outer Border (Rounded modern card)
    COLORREF borderCol = isDownloading ? RGB(0, 162, 255) : RGB(42, 50, 68);
    HPEN borderPen = CreatePen(PS_SOLID, isDownloading ? 2 : 1, borderCol);
    HGDIOBJ oldPen = SelectObject(memDC, borderPen);
    HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
    int r = MulDiv(10, m_dpi, 96);
    RoundRect(memDC, 1, 1, w - 1, h - 1, r * 2, r * 2);

    // Inner subtle glow if downloading
    if (isDownloading) {
        HPEN glowPen = CreatePen(PS_SOLID, 1, RGB(0, 80, 140));
        SelectObject(memDC, glowPen);
        RoundRect(memDC, 3, 3, w - 3, h - 3, (r - 2) * 2, (r - 2) * 2);
        DeleteObject(glowPen);
    }

    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);
    DeleteObject(borderPen);

    SetBkMode(memDC, TRANSPARENT);

    int padX = MulDiv(9, m_dpi, 96);
    int padY = MulDiv(6, m_dpi, 96);
    int topRowH = MulDiv(14, m_dpi, 96);

    // 3. Header Row: "⚡ GETY" + Status (● 2 İndiriliyor or ○ Boşta)
    RECT titleR = { padX, padY, padX + MulDiv(60, m_dpi, 96), padY + topRowH };
    SelectObject(memDC, m_hTitleFont);
    SetTextColor(memDC, isDownloading ? RGB(56, 189, 248) : RGB(0, 162, 255));
    DrawTextW(memDC, L"⚡ GETY", -1, &titleR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT statusR = { w / 2, padY, w - padX, padY + topRowH };
    SelectObject(memDC, m_hSubFont);
    if (isDownloading) {
        wchar_t actBuf[48];
        swprintf_s(actBuf, L"● %d Aktif", m_activeCount);
        SetTextColor(memDC, RGB(34, 197, 94)); // Neon emerald
        DrawTextW(memDC, actBuf, -1, &statusR, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(memDC, RGB(100, 116, 139)); // Muted slate
        DrawTextW(memDC, L"○ Boşta", -1, &statusR, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // 4. Center Main Section
    int centerTop = padY + topRowH + MulDiv(1, m_dpi, 96);
    int progH = MulDiv(4, m_dpi, 96);
    int bottomMargin = MulDiv(7, m_dpi, 96);
    int centerBottom = h - bottomMargin - progH - MulDiv(2, m_dpi, 96);

    if (isDownloading) {
        // Active downloading display: Big speed number + percentage
        RECT speedR = { padX, centerTop, w - padX, centerTop + MulDiv(19, m_dpi, 96) };
        SelectObject(memDC, m_hSpeedFont);
        std::wstring spdText = FormatSpeed(m_speedBps);
        SetTextColor(memDC, RGB(241, 245, 249)); // Pure bright white
        DrawTextW(memDC, spdText.c_str(), -1, &speedR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Subtitle: percentage
        RECT subR = { padX, speedR.bottom - MulDiv(1, m_dpi, 96), w - padX, centerBottom };
        SelectObject(memDC, m_hSubFont);
        wchar_t pctBuf[48];
        swprintf_s(pctBuf, L"%%%0.1f Tamamlandı", std::clamp(m_progress * 100.0, 0.0, 100.0));
        SetTextColor(memDC, RGB(148, 163, 184));
        DrawTextW(memDC, pctBuf, -1, &subR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Progress Bar
        int progY = h - bottomMargin - progH;
        RECT progBgR = { padX, progY, w - padX, progY + progH };

        HBRUSH trackBrush = CreateSolidBrush(RGB(22, 28, 42));
        FillRect(memDC, &progBgR, trackBrush);
        DeleteObject(trackBrush);

        int progWidth = static_cast<int>((progBgR.right - progBgR.left) * std::clamp(m_progress, 0.0, 1.0));
        if (progWidth > 0) {
            RECT progFillR = { progBgR.left, progBgR.top, progBgR.left + progWidth, progBgR.bottom };
            HBRUSH fillBrush = CreateSolidBrush(RGB(0, 162, 255)); // Cyber Cyan
            FillRect(memDC, &progFillR, fillBrush);
            DeleteObject(fillBrush);
        }
    } else {
        // Idle / Empty state: Clean drop-target HUD
        RECT idleR = { padX, centerTop + MulDiv(1, m_dpi, 96), w - padX, centerTop + MulDiv(18, m_dpi, 96) };
        SelectObject(memDC, m_hIdleFont);
        SetTextColor(memDC, RGB(160, 175, 195)); // Soft silver
        DrawTextW(memDC, L"📥 İndirme Bekleniyor", -1, &idleR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Subtitle hint
        RECT hintR = { padX, idleR.bottom, w - padX, centerBottom };
        SelectObject(memDC, m_hSubFont);
        SetTextColor(memDC, RGB(90, 105, 128));
        DrawTextW(memDC, L"Dosya veya URL Bırakın", -1, &hintR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Standby subtle line
        int progY = h - bottomMargin - progH / 2;
        HPEN linePen = CreatePen(PS_SOLID, 1, RGB(28, 36, 52));
        HGDIOBJ oldLinePen = SelectObject(memDC, linePen);
        MoveToEx(memDC, padX + MulDiv(6, m_dpi, 96), progY, NULL);
        LineTo(memDC, w - padX - MulDiv(6, m_dpi, 96), progY);
        SelectObject(memDC, oldLinePen);
        DeleteObject(linePen);
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

bool FloatingWindow::SaveSnapshot(const std::wstring& filePath, double speedBps, int activeTasks, double progress) {
    int w = m_width > 0 ? m_width : MulDiv(m_baseWidth, 96, 96);
    int h = m_height > 0 ? m_height : MulDiv(m_baseHeight, 96, 96);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

    double oldSpd = m_speedBps;
    int oldAct = m_activeCount;
    double oldProg = m_progress;

    m_speedBps = speedBps;
    m_activeCount = activeTasks;
    m_progress = progress;

    OnPaint(hdcMem);

    m_speedBps = oldSpd;
    m_activeCount = oldAct;
    m_progress = oldProg;

    SelectObject(hdcMem, oldBm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) {
        DeleteObject(hbm);
        return false;
    }

    IWICBitmap* pWicBitmap = nullptr;
    hr = pFactory->CreateBitmapFromHBITMAP(hbm, NULL, WICBitmapIgnoreAlpha, &pWicBitmap);
    DeleteObject(hbm);
    if (FAILED(hr)) {
        pFactory->Release();
        return false;
    }

    IWICStream* pStream = nullptr;
    hr = pFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) {
        hr = pStream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);
        if (SUCCEEDED(hr)) {
            IWICBitmapEncoder* pEncoder = nullptr;
            hr = pFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &pEncoder);
            if (SUCCEEDED(hr)) {
                hr = pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
                if (SUCCEEDED(hr)) {
                    IWICBitmapFrameEncode* pFrame = nullptr;
                    hr = pEncoder->CreateNewFrame(&pFrame, NULL);
                    if (SUCCEEDED(hr)) {
                        hr = pFrame->Initialize(NULL);
                        if (SUCCEEDED(hr)) hr = pFrame->SetSize(w, h);
                        if (SUCCEEDED(hr)) hr = pFrame->WriteSource(pWicBitmap, NULL);
                        if (SUCCEEDED(hr)) hr = pFrame->Commit();
                        pFrame->Release();
                    }
                    if (SUCCEEDED(hr)) hr = pEncoder->Commit();
                }
                pEncoder->Release();
            }
        }
        pStream->Release();
    }
    pWicBitmap->Release();
    pFactory->Release();
    return true;
}

} // namespace Gety
