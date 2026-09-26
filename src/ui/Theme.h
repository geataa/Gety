#pragma once
#include <windows.h>

namespace Gety {
namespace Theme {

// Dark Obsidian & Acrylic Palette (PhotoViewer / GhostView style)
constexpr COLORREF BgMain       = RGB(19, 21, 27);     // #13151b
constexpr COLORREF BgSurface    = RGB(26, 29, 38);     // #1a1d26
constexpr COLORREF BgCard       = RGB(22, 25, 33);     // #161921
constexpr COLORREF BgInput      = RGB(15, 17, 23);     // #0f1117
constexpr COLORREF BgHover      = RGB(42, 51, 70);     // #2a3346
constexpr COLORREF BgSelected   = RGB(35, 45, 66);     // #232d42

constexpr COLORREF BorderNormal = RGB(42, 47, 61);     // #2a2f3d
constexpr COLORREF BorderLight  = RGB(55, 62, 80);     // #373e50
constexpr COLORREF BorderGlow   = RGB(0, 162, 255);    // #00a2ff

constexpr COLORREF TextPrimary  = RGB(241, 245, 249);  // #f1f5f9
constexpr COLORREF TextMuted    = RGB(148, 163, 184);  // #94a3b8
constexpr COLORREF TextDim      = RGB(100, 116, 139);  // #64748b

constexpr COLORREF AccentCyan   = RGB(56, 189, 248);   // #38bdf8
constexpr COLORREF AccentBlue   = RGB(0, 162, 255);    // #00a2ff
constexpr COLORREF AccentGreen  = RGB(34, 197, 94);    // #22c55e
constexpr COLORREF AccentAmber  = RGB(245, 158, 11);   // #f59e0b
constexpr COLORREF AccentRed    = RGB(239, 68, 68);    // #ef4444

inline int GetWindowDpi(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);
        static auto pfnGetDpiForWindow = (GetDpiForWindowFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        if (pfnGetDpiForWindow) {
            UINT dpi = pfnGetDpiForWindow(hwnd);
            if (dpi != 0) return (int)dpi;
        }
        HDC hdc = GetDC(hwnd);
        if (hdc) {
            int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hwnd, hdc);
            if (dpi != 0) return dpi;
        }
    }
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    return (dpi != 0) ? dpi : 96;
}

inline HFONT CreateAppFontForDpi(int dpi, int sizePt = 10, int weight = FW_NORMAL, const wchar_t* face = L"Segoe UI") {
    if (dpi <= 0) dpi = 96;
    int height = -MulDiv(sizePt, dpi, 72);
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

inline HFONT CreateAppFont(HWND hwnd, int sizePt = 10, int weight = FW_NORMAL, const wchar_t* face = L"Segoe UI") {
    int dpi = GetWindowDpi(hwnd);
    return CreateAppFontForDpi(dpi, sizePt, weight, face);
}

inline HFONT CreateAppFont(int sizePt = 10, int weight = FW_NORMAL, const wchar_t* face = L"Segoe UI") {
    return CreateAppFont((HWND)NULL, sizePt, weight, face);
}

inline void DrawModernButton(LPDRAWITEMSTRUCT dis, bool isPrimary, const std::wstring& text, HFONT hFont = NULL) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED);
    bool isDisabled = (dis->itemState & ODS_DISABLED);

    COLORREF bgCol;
    COLORREF borderCol;
    COLORREF textCol;

    if (isPrimary) {
        if (isDisabled) {
            bgCol = RGB(30, 45, 60);
            borderCol = RGB(40, 60, 80);
            textCol = RGB(100, 120, 140);
        } else if (isPressed) {
            bgCol = RGB(0, 120, 205);
            borderCol = RGB(0, 162, 255);
            textCol = RGB(255, 255, 255);
        } else {
            bgCol = RGB(0, 140, 235);
            borderCol = RGB(56, 189, 248);
            textCol = RGB(255, 255, 255);
        }
    } else {
        if (isDisabled) {
            bgCol = RGB(20, 23, 30);
            borderCol = RGB(35, 40, 52);
            textCol = RGB(80, 90, 105);
        } else if (isPressed) {
            bgCol = RGB(35, 45, 66);
            borderCol = RGB(0, 162, 255);
            textCol = RGB(255, 255, 255);
        } else {
            bgCol = RGB(26, 29, 38);
            borderCol = RGB(45, 52, 68);
            textCol = RGB(220, 228, 240);
        }
    }

    HBRUSH hBgBrush = CreateSolidBrush(bgCol);
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, borderCol);
    HGDIOBJ oldBrush = SelectObject(hdc, hBgBrush);
    HGDIOBJ oldPen = SelectObject(hdc, hBorderPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hBgBrush);
    DeleteObject(hBorderPen);

    HGDIOBJ oldFont = NULL;
    HFONT tempFont = NULL;
    if (hFont) {
        oldFont = SelectObject(hdc, hFont);
    } else {
        tempFont = CreateAppFont(dis->hwndItem, 9, FW_SEMIBOLD);
        oldFont = SelectObject(hdc, tempFont);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    DrawTextW(hdc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (oldFont) SelectObject(hdc, oldFont);
    if (tempFont) DeleteObject(tempFont);
}

inline void DrawModernDangerButton(LPDRAWITEMSTRUCT dis, const std::wstring& text, HFONT hFont = NULL) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED);
    bool isDisabled = (dis->itemState & ODS_DISABLED);

    COLORREF bgCol;
    COLORREF borderCol;
    COLORREF textCol;

    if (isDisabled) {
        bgCol = RGB(35, 20, 20);
        borderCol = RGB(55, 30, 30);
        textCol = RGB(120, 80, 80);
    } else if (isPressed) {
        bgCol = RGB(185, 28, 28);
        borderCol = RGB(239, 68, 68);
        textCol = RGB(255, 255, 255);
    } else {
        bgCol = RGB(220, 38, 38);
        borderCol = RGB(248, 113, 113);
        textCol = RGB(255, 255, 255);
    }

    HBRUSH hBgBrush = CreateSolidBrush(bgCol);
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, borderCol);
    HGDIOBJ oldBrush = SelectObject(hdc, hBgBrush);
    HGDIOBJ oldPen = SelectObject(hdc, hBorderPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hBgBrush);
    DeleteObject(hBorderPen);

    HGDIOBJ oldFont = NULL;
    HFONT tempFont = NULL;
    if (hFont) {
        oldFont = SelectObject(hdc, hFont);
    } else {
        tempFont = CreateAppFont(dis->hwndItem, 9, FW_SEMIBOLD);
        oldFont = SelectObject(hdc, tempFont);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    DrawTextW(hdc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (oldFont) SelectObject(hdc, oldFont);
    if (tempFont) DeleteObject(tempFont);
}

inline void DrawSegmentedTab(LPDRAWITEMSTRUCT dis, bool isSelected, const std::wstring& text, HFONT hFont = NULL) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED);

    COLORREF bgCol;
    COLORREF borderCol;
    COLORREF textCol;

    if (isSelected) {
        bgCol = RGB(0, 115, 210);
        borderCol = RGB(56, 189, 248);
        textCol = RGB(255, 255, 255);
    } else if (isPressed) {
        bgCol = RGB(35, 45, 66);
        borderCol = RGB(0, 162, 255);
        textCol = RGB(240, 245, 255);
    } else {
        bgCol = RGB(22, 26, 36);
        borderCol = RGB(45, 52, 68);
        textCol = RGB(150, 165, 185);
    }

    HBRUSH hBgBrush = CreateSolidBrush(bgCol);
    HPEN hBorderPen = CreatePen(PS_SOLID, isSelected ? 2 : 1, borderCol);
    HGDIOBJ oldBrush = SelectObject(hdc, hBgBrush);
    HGDIOBJ oldPen = SelectObject(hdc, hBorderPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hBgBrush);
    DeleteObject(hBorderPen);

    HGDIOBJ oldFont = NULL;
    HFONT tempFont = NULL;
    if (hFont) {
        oldFont = SelectObject(hdc, hFont);
    } else {
        tempFont = CreateAppFont(dis->hwndItem, 9, isSelected ? FW_BOLD : FW_NORMAL);
        oldFont = SelectObject(hdc, tempFont);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    DrawTextW(hdc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (oldFont) SelectObject(hdc, oldFont);
    if (tempFont) DeleteObject(tempFont);
}

} // namespace Theme
} // namespace Gety
