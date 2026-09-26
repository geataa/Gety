#include "Dialogs.h"
#include "Theme.h"
#include "../core/Config.h"
#include "../core/SpeedLimiter.h"
#include "../core/I18n.h"
#include "../core/BatchExpander.h"
#include "../core/ClipboardWatcher.h"
#include "../engine/DownloadManager.h"
#include "../engine/WinHttpUtils.h"
#include "../engine/MediaExtractor.h"
#include "../engine/OllamaClient.h"
#include "../engine/HuggingFaceClient.h"
#include <commctrl.h>
#include <windowsx.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <wincodec.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace Gety {

static std::wstring s_dialogSnapshotPath;

void Dialogs::SetDialogSnapshotTarget(const std::wstring& path) {
    s_dialogSnapshotPath = path;
}

static bool CaptureHwndToPng(HWND hwnd, const std::wstring& outPath) {
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    Sleep(80);
    PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);

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
        hr = pStream->InitializeFromFilename(outPath.c_str(), GENERIC_WRITE);
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
    return SUCCEEDED(hr);
}

static inline int DlgScale(HWND hwnd, int val) {
    int dpi = Theme::GetWindowDpi(hwnd);
    return MulDiv(val, dpi, 96);
}

static void ApplyDialogDarkMode(HWND hwnd) {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_OLD */, &darkMode, sizeof(darkMode));

    // Aero drop shadow on borderless popup
    MARGINS margins = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

static std::wstring BrowseForFolder(HWND hParent, const std::wstring& defaultPath) {
    wchar_t path[MAX_PATH] = { 0 };
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hParent;
    bi.lpszTitle = LStr(StrId::DlgNewDir);
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDListW(pidl, path)) {
            CoTaskMemFree(pidl);
            return path;
        }
        CoTaskMemFree(pidl);
    }
    return defaultPath;
}

// -------------------------------------------------------------
// Shared Custom Frameless Header & Card Container
// -------------------------------------------------------------
struct CustomDialogHeader {
    std::wstring title;
    bool closeHovered = false;

    void Draw(HWND hwnd, HDC hdc, int w, int h) {
        // 1. Full Window Background (#13151b)
        RECT rc = { 0, 0, w, h };
        HBRUSH hBg = CreateSolidBrush(Theme::BgMain);
        FillRect(hdc, &rc, hBg);
        DeleteObject(hBg);

        // 2. Outer Window Border (#2a2f3d)
        HPEN hBorderPen = CreatePen(PS_SOLID, 1, Theme::BorderNormal);
        HGDIOBJ oldPen = SelectObject(hdc, hBorderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, 0, 0, w, h);

        int headerH = DlgScale(hwnd, 42);

        // 3. Top Header Bar (#1a1d26)
        RECT headerR = { 1, 1, w - 1, headerH };
        HBRUSH hHeaderBg = CreateSolidBrush(Theme::BgSurface);
        FillRect(hdc, &headerR, hHeaderBg);
        DeleteObject(hHeaderBg);

        // Separator line
        MoveToEx(hdc, 1, headerH, NULL);
        LineTo(hdc, w - 1, headerH);

        // 4. Logo & Title
        SetBkMode(hdc, TRANSPARENT);
        HFONT hFontBrand = Theme::CreateAppFont(hwnd, 10, FW_BOLD);
        HGDIOBJ oldFont = SelectObject(hdc, hFontBrand);
        SetTextColor(hdc, Theme::AccentCyan);
        RECT brandR = { DlgScale(hwnd, 16), 0, DlgScale(hwnd, 96), headerH };
        DrawTextW(hdc, L"⚡ GETY", -1, &brandR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        HFONT hFontTitle = Theme::CreateAppFont(hwnd, 10, FW_SEMIBOLD);
        SelectObject(hdc, hFontTitle);
        SetTextColor(hdc, Theme::TextPrimary);
        RECT titleR = { DlgScale(hwnd, 96), 0, w - DlgScale(hwnd, 48), headerH };
        DrawTextW(hdc, title.c_str(), -1, &titleR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 5. Close Button
        int closeBtnW = DlgScale(hwnd, 28);
        int closeBtnH = DlgScale(hwnd, 26);
        int closeX = w - DlgScale(hwnd, 38);
        int closeY = (headerH - closeBtnH) / 2;
        RECT closeR = { closeX, closeY, closeX + closeBtnW, closeY + closeBtnH };
        if (closeHovered) {
            HBRUSH hCloseBg = CreateSolidBrush(Theme::AccentRed);
            HGDIOBJ oldClosePen = SelectObject(hdc, GetStockObject(NULL_PEN));
            SelectObject(hdc, hCloseBg);
            RoundRect(hdc, closeR.left, closeR.top, closeR.right, closeR.bottom, 6, 6);
            DeleteObject(hCloseBg);
            SetTextColor(hdc, RGB(255, 255, 255));
        } else {
            HBRUSH hCloseBg = CreateSolidBrush(Theme::BgCard);
            HPEN hCloseBorder = CreatePen(PS_SOLID, 1, Theme::BorderNormal);
            SelectObject(hdc, hCloseBorder);
            SelectObject(hdc, hCloseBg);
            RoundRect(hdc, closeR.left, closeR.top, closeR.right, closeR.bottom, 6, 6);
            DeleteObject(hCloseBg);
            DeleteObject(hCloseBorder);
            SetTextColor(hdc, Theme::TextMuted);
        }
        DrawTextW(hdc, L"✕", -1, &closeR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 6. Content Card Background (#161921 with #2a2f3d border)
        int cardPad = DlgScale(hwnd, 14);
        int cardBottom = h - DlgScale(hwnd, 58);
        RECT cardR = { cardPad, headerH + DlgScale(hwnd, 8), w - cardPad, cardBottom };
        HBRUSH hCardBg = CreateSolidBrush(Theme::BgCard);
        HPEN hCardPen = CreatePen(PS_SOLID, 1, Theme::BorderNormal);
        SelectObject(hdc, hCardPen);
        SelectObject(hdc, hCardBg);
        RoundRect(hdc, cardR.left, cardR.top, cardR.right, cardR.bottom, 10, 10);
        DeleteObject(hCardBg);
        DeleteObject(hCardPen);

        // Cleanup
        SelectObject(hdc, oldFont);
        DeleteObject(hFontBrand);
        DeleteObject(hFontTitle);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hBorderPen);
    }

    bool HitTestClose(HWND hwnd, int x, int y, int w) const {
        int headerH = DlgScale(hwnd, 42);
        int closeBtnW = DlgScale(hwnd, 28);
        int closeBtnH = DlgScale(hwnd, 26);
        int closeX = w - DlgScale(hwnd, 38);
        int closeY = (headerH - closeBtnH) / 2;
        return (x >= closeX && x <= closeX + closeBtnW && y >= closeY && y <= closeY + closeBtnH);
    }
};

// -------------------------------------------------------------
// 1. New Download Dialog (Yeni İndirme)
// -------------------------------------------------------------
struct NewDownloadDialogData {
    std::wstring url;
    std::wstring filename;
    std::wstring saveDir;
    std::wstring category;
    int splitParts = 10;
    bool startImmediately = true;
    bool accepted = false;
    bool userEditedFilename = false;

    HWND hwndLblUrl = NULL;
    HWND hwndUrl = NULL;
    HWND hwndBtnPaste = NULL;
    HWND hwndLblFilename = NULL;
    HWND hwndFilename = NULL;
    HWND hwndLblDir = NULL;
    HWND hwndDir = NULL;
    HWND hwndBtnBrowse = NULL;
    HWND hwndLblCat = NULL;
    HWND hwndCat = NULL;
    HWND hwndLblParts = NULL;
    HWND hwndParts = NULL;
    HWND hwndStart = NULL;
    HWND hwndLblStart = NULL;
    HWND hwndBtnCancel = NULL;
    HWND hwndBtnOk = NULL;

    HFONT hFont = NULL;
    HFONT hFontBold = NULL;

    CustomDialogHeader header;
};

static void LayoutNewDownloadControls(HWND hwnd, NewDownloadDialogData* pData) {
    if (!pData) return;
    RECT clR;
    GetClientRect(hwnd, &clR);
    int padX = DlgScale(hwnd, 30);
    int fieldW = clR.right - padX * 2;
    int inputH = DlgScale(hwnd, 28);
    int labelH = DlgScale(hwnd, 18);

    // 1. URL + Paste Button
    int y0 = DlgScale(hwnd, 58);
    int pasteW = DlgScale(hwnd, 92);
    int urlW = fieldW - pasteW - DlgScale(hwnd, 8);
    MoveWindow(pData->hwndLblUrl, padX, y0, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndUrl, padX, y0 + labelH + 2, urlW, inputH, TRUE);
    MoveWindow(pData->hwndBtnPaste, padX + urlW + DlgScale(hwnd, 8), y0 + labelH + 2, pasteW, inputH, TRUE);

    // 2. Dosya Adı (Filename)
    int y1 = y0 + inputH + labelH + DlgScale(hwnd, 10);
    MoveWindow(pData->hwndLblFilename, padX, y1, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndFilename, padX, y1 + labelH + 2, fieldW, inputH, TRUE);

    // 3. Save Directory + Browse
    int y2 = y1 + inputH + labelH + DlgScale(hwnd, 10);
    int browseW = DlgScale(hwnd, 92);
    int dirW = fieldW - browseW - DlgScale(hwnd, 8);
    MoveWindow(pData->hwndLblDir, padX, y2, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndDir, padX, y2 + labelH + 2, dirW, inputH, TRUE);
    MoveWindow(pData->hwndBtnBrowse, padX + dirW + DlgScale(hwnd, 8), y2 + labelH + 2, browseW, inputH, TRUE);

    // 4. Category & Split Parts (2 Columns)
    int y3 = y2 + inputH + labelH + DlgScale(hwnd, 10);
    int colGap = DlgScale(hwnd, 16);
    int colW = (fieldW - colGap) / 2;
    int col2X = padX + colW + colGap;
    MoveWindow(pData->hwndLblCat, padX, y3, colW, labelH, TRUE);
    MoveWindow(pData->hwndCat, padX, y3 + labelH + 2, colW, DlgScale(hwnd, 200), TRUE);
    MoveWindow(pData->hwndLblParts, col2X, y3, colW, labelH, TRUE);
    MoveWindow(pData->hwndParts, col2X, y3 + labelH + 2, colW, DlgScale(hwnd, 200), TRUE);

    // 5. Start Immediately Checkbox + Label
    int y4 = y3 + inputH + labelH + DlgScale(hwnd, 14);
    int chkBoxW = DlgScale(hwnd, 18);
    int chkBoxH = DlgScale(hwnd, 18);
    MoveWindow(pData->hwndStart, padX, y4 + 1, chkBoxW, chkBoxH, TRUE);
    MoveWindow(pData->hwndLblStart, padX + chkBoxW + DlgScale(hwnd, 8), y4, fieldW - (chkBoxW + DlgScale(hwnd, 8)), labelH + 2, TRUE);

    // 6. Bottom Action Buttons
    int btnH = DlgScale(hwnd, 34);
    int yBtns = clR.bottom - DlgScale(hwnd, 46);
    int cancelW = DlgScale(hwnd, 105);
    int okW = DlgScale(hwnd, 145);
    int okX = clR.right - padX - okW;
    int cancelX = okX - cancelW - DlgScale(hwnd, 12);
    MoveWindow(pData->hwndBtnCancel, cancelX, yBtns, cancelW, btnH, TRUE);
    MoveWindow(pData->hwndBtnOk, okX, yBtns, okW, btnH, TRUE);
}

static LRESULT CALLBACK NewDownloadDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NewDownloadDialogData* pData = (NewDownloadDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            pData = (NewDownloadDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            pData->header.title = LStr(StrId::DlgNewTitle);
            ApplyDialogDarkMode(hwnd);

            pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

            // 1. URL
            pData->hwndLblUrl = CreateWindowW(L"STATIC", LStr(StrId::DlgNewUrl), WS_CHILD | WS_VISIBLE,
                                              0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblUrl, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndUrl = CreateWindowExW(0, L"EDIT", pData->url.c_str(),
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                             0, 0, 0, 0, hwnd, (HMENU)101, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndUrl, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndUrl, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnPaste = CreateWindowW(L"BUTTON", LStr(StrId::DlgNewPaste), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                0, 0, 0, 0, hwnd, (HMENU)109, GetModuleHandle(NULL), NULL);

            // If initialUrl had a filename, auto-populate pData->filename
            if (pData->filename.empty() && !pData->url.empty()) {
                pData->filename = WinHttpUtils::ExtractFilenameFromUrl(pData->url);
            }

            // 2. Dosya Adı (Filename) - EDITABLE!
            pData->hwndLblFilename = CreateWindowW(L"STATIC", LStr(StrId::DlgNewFilename), WS_CHILD | WS_VISIBLE,
                                                   0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblFilename, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndFilename = CreateWindowExW(0, L"EDIT", pData->filename.c_str(),
                                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                                  0, 0, 0, 0, hwnd, (HMENU)107, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndFilename, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndFilename, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            // 3. Save Directory + Browse
            pData->hwndLblDir = CreateWindowW(L"STATIC", LStr(StrId::DlgNewDir), WS_CHILD | WS_VISIBLE,
                                              0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblDir, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndDir = CreateWindowExW(0, L"EDIT", pData->saveDir.c_str(),
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                             0, 0, 0, 0, hwnd, (HMENU)102, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndDir, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndDir, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnBrowse = CreateWindowW(L"BUTTON", LStr(StrId::DlgNewBrowse), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)103, GetModuleHandle(NULL), NULL);

            // 4. Category & Split Parts (2 Columns)
            pData->hwndLblCat = CreateWindowW(L"STATIC", LStr(StrId::DlgNewCat), WS_CHILD | WS_VISIBLE,
                                              0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblCat, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndCat = CreateWindowExW(0, WC_COMBOBOXW, L"",
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                             0, 0, 0, 0, hwnd, (HMENU)104, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndCat, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndCat, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            const StrId catIds[] = { StrId::CatGeneral, StrId::CatSoftware, StrId::CatGames, StrId::CatMusic, StrId::CatVideo, StrId::CatDocuments, StrId::CatArchives };
            for (const auto cid : catIds) {
                SendMessageW(pData->hwndCat, CB_ADDSTRING, 0, (LPARAM)LStr(cid));
            }
            StrId initialCat = WinHttpUtils::DetectCategoryFromFilename(pData->filename.empty() ? pData->url : pData->filename);
            int catIdx = 0;
            for (int i = 0; i < (int)_countof(catIds); ++i) {
                if (catIds[i] == initialCat) {
                    catIdx = i;
                    break;
                }
            }
            SendMessageW(pData->hwndCat, CB_SETCURSEL, catIdx, 0);

            pData->hwndLblParts = CreateWindowW(L"STATIC", LStr(StrId::DlgNewParts), WS_CHILD | WS_VISIBLE,
                                                0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblParts, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndParts = CreateWindowExW(0, WC_COMBOBOXW, L"",
                                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                               0, 0, 0, 0, hwnd, (HMENU)105, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndParts, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndParts, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            std::vector<int> partOptions = { 1, 2, 4, 6, 8, 10, 12, 16, 20, 24, 32, 64 };
            bool foundPart = false;
            for (int p : partOptions) {
                if (p == pData->splitParts) { foundPart = true; break; }
            }
            if (!foundPart && pData->splitParts > 0) {
                partOptions.push_back(pData->splitParts);
                std::sort(partOptions.begin(), partOptions.end());
            }

            int defaultPartIdx = 0;
            for (size_t i = 0; i < partOptions.size(); ++i) {
                wchar_t optBuf[64];
                swprintf_s(optBuf, L"%d (%s)", partOptions[i], LStr(StrId::ColParts));
                int idx = (int)SendMessageW(pData->hwndParts, CB_ADDSTRING, 0, (LPARAM)optBuf);
                SendMessageW(pData->hwndParts, CB_SETITEMDATA, idx, (LPARAM)partOptions[i]);
                if (partOptions[i] == pData->splitParts) {
                    defaultPartIdx = (int)i;
                }
            }
            SendMessageW(pData->hwndParts, CB_SETCURSEL, defaultPartIdx, 0);

            // 5. Start Immediately Checkbox + Crisp Static Label
            pData->hwndStart = CreateWindowW(L"BUTTON", L"",
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                             0, 0, 0, 0, hwnd, (HMENU)106, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndStart, BM_SETCHECK, pData->startImmediately ? BST_CHECKED : BST_UNCHECKED, 0);

            pData->hwndLblStart = CreateWindowW(L"STATIC", LStr(StrId::DlgNewStartImm),
                                                WS_CHILD | WS_VISIBLE | SS_NOTIFY,
                                                0, 0, 0, 0, hwnd, (HMENU)108, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblStart, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            // 6. Bottom Action Buttons
            pData->hwndBtnCancel = CreateWindowW(L"BUTTON", LStr(StrId::DlgCancel), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            pData->hwndBtnOk = CreateWindowW(L"BUTTON", LStr(StrId::DlgOk), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                             0, 0, 0, 0, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

            LayoutNewDownloadControls(hwnd, pData);

            SetFocus(pData->hwndUrl);
            SendMessageW(pData->hwndUrl, EM_SETSEL, 0, -1);

            if (!s_dialogSnapshotPath.empty()) {
                SetTimer(hwnd, 9999, 150, NULL);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (pData) {
                if (pData->hFont) DeleteObject(pData->hFont);
                if (pData->hFontBold) DeleteObject(pData->hFontBold);
                pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
                pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

                SendMessageW(pData->hwndLblUrl, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndUrl, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblFilename, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndFilename, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblDir, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndDir, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblCat, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndCat, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblParts, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndParts, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblStart, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                LayoutNewDownloadControls(hwnd, pData);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pData) {
                if (pData->hFont) { DeleteObject(pData->hFont); pData->hFont = NULL; }
                if (pData->hFontBold) { DeleteObject(pData->hFontBold); pData->hFontBold = NULL; }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 9999) {
                KillTimer(hwnd, 9999);
                CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                s_dialogSnapshotPath.clear();
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y <= DlgScale(hwnd, 42)) {
                if (pData && pData->header.HitTestClose(hwnd, pt.x, pt.y, rc.right)) {
                    return HTCLIENT;
                }
                return HTCAPTION; // Header draggable
            }
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                bool wasHov = pData->header.closeHovered;
                pData->header.closeHovered = pData->header.HitTestClose(hwnd, x, y, rc.right);
                if (wasHov != pData->header.closeHovered) {
                    RECT r = { rc.right - DlgScale(hwnd, 42), 0, rc.right, DlgScale(hwnd, 42) };
                    InvalidateRect(hwnd, &r, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    pData->accepted = false;
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == IDOK) {
                Theme::DrawModernButton(dis, true, L"⬇  " + std::wstring(LStr(StrId::DlgOk)));
                return TRUE;
            } else if (dis->CtlID == IDCANCEL) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgCancel));
                return TRUE;
            } else if (dis->CtlID == 103) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgNewBrowse));
                return TRUE;
            } else if (dis->CtlID == 109) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgNewPaste));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgCard);
            static HBRUSH hbrCard = CreateSolidBrush(Theme::BgCard);
            return (LRESULT)hbrCard;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrEdit = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrEdit;
        }

        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrList = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrList;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmCode = HIWORD(wParam);

            if (wmId == 101 && wmCode == EN_CHANGE) {
                // Auto-extract filename on URL change
                if (pData && !pData->userEditedFilename) {
                    wchar_t urlBuf[2048] = { 0 };
                    GetWindowTextW(pData->hwndUrl, urlBuf, _countof(urlBuf));
                    std::wstring extracted = WinHttpUtils::ExtractFilenameFromUrl(urlBuf);
                    if (!extracted.empty() && extracted != L"download.dat") {
                        SetWindowTextW(pData->hwndFilename, extracted.c_str());
                    }
                    StrId autoCat = WinHttpUtils::DetectCategoryFromFilename(extracted.empty() ? urlBuf : extracted);
                    const StrId catIds[] = { StrId::CatGeneral, StrId::CatSoftware, StrId::CatGames, StrId::CatMusic, StrId::CatVideo, StrId::CatDocuments, StrId::CatArchives };
                    for (int i = 0; i < (int)_countof(catIds); ++i) {
                        if (catIds[i] == autoCat) {
                            SendMessageW(pData->hwndCat, CB_SETCURSEL, i, 0);
                            break;
                        }
                    }
                }
            } else if (wmId == 107 && wmCode == EN_CHANGE) {
                // User manually edited filename
                if (pData && GetFocus() == pData->hwndFilename) {
                    pData->userEditedFilename = true;
                    wchar_t fnBuf[1024] = { 0 };
                    GetWindowTextW(pData->hwndFilename, fnBuf, _countof(fnBuf));
                    StrId autoCat = WinHttpUtils::DetectCategoryFromFilename(fnBuf);
                    const StrId catIds[] = { StrId::CatGeneral, StrId::CatSoftware, StrId::CatGames, StrId::CatMusic, StrId::CatVideo, StrId::CatDocuments, StrId::CatArchives };
                    for (int i = 0; i < (int)_countof(catIds); ++i) {
                        if (catIds[i] == autoCat) {
                            SendMessageW(pData->hwndCat, CB_SETCURSEL, i, 0);
                            break;
                        }
                    }
                }
            } else if (wmId == 108) {
                if (pData && pData->hwndStart) {
                    LRESULT chk = SendMessageW(pData->hwndStart, BM_GETCHECK, 0, 0);
                    SendMessageW(pData->hwndStart, BM_SETCHECK, (chk == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED, 0);
                }
            } else if (wmId == 103) {
                std::wstring dir = BrowseForFolder(hwnd, pData->saveDir);
                SetWindowTextW(pData->hwndDir, dir.c_str());
            } else if (wmId == 109) {
                std::wstring clip = ClipboardWatcher::GetClipboardText(hwnd);
                std::wstring url = WinHttpUtils::ExtractUrlFromText(clip);
                if (url.empty()) {
                    std::wstring trimmed = WinHttpUtils::TrimUrl(clip);
                    if (!trimmed.empty() && trimmed.find_first_of(L"\r\n") == std::wstring::npos) {
                        url = trimmed;
                    }
                }
                if (!url.empty()) {
                    SetWindowTextW(pData->hwndUrl, url.c_str());
                    SendMessageW(pData->hwndUrl, EM_SETSEL, 0, -1);
                    SetFocus(pData->hwndUrl);
                    if (pData && !pData->userEditedFilename) {
                        std::wstring extracted = WinHttpUtils::ExtractFilenameFromUrl(url);
                        if (!extracted.empty() && extracted != L"download.dat") {
                            SetWindowTextW(pData->hwndFilename, extracted.c_str());
                        }
                        StrId autoCat = WinHttpUtils::DetectCategoryFromFilename(extracted.empty() ? url : extracted);
                        const StrId catIds[] = { StrId::CatGeneral, StrId::CatSoftware, StrId::CatGames, StrId::CatMusic, StrId::CatVideo, StrId::CatDocuments, StrId::CatArchives };
                        for (int i = 0; i < (int)_countof(catIds); ++i) {
                            if (catIds[i] == autoCat) {
                                SendMessageW(pData->hwndCat, CB_SETCURSEL, i, 0);
                                break;
                            }
                        }
                    }
                }
            } else if (wmId == IDOK) {
                wchar_t buf[2048] = { 0 };
                GetWindowTextW(pData->hwndUrl, buf, _countof(buf));
                pData->url = WinHttpUtils::TrimUrl(buf);

                if (HuggingFaceClient::IsLikelyHuggingFaceModel(pData->url)) {
                    std::wstring modelToOpen = pData->url;
                    DestroyWindow(hwnd);
                    Dialogs::ShowHuggingFaceModelDialog(GetParent(hwnd), modelToOpen);
                    return 0;
                } else if (OllamaClient::IsLikelyOllamaModel(pData->url)) {
                    if (pData->url.find(L"://") == std::wstring::npos || pData->url.find(L"ollama.com") != std::wstring::npos || pData->url.rfind(L"ollama:", 0) == 0) {
                        std::wstring modelToOpen = pData->url;
                        DestroyWindow(hwnd);
                        Dialogs::ShowOllamaModelDialog(GetParent(hwnd), modelToOpen);
                        return 0;
                    }
                }

                GetWindowTextW(pData->hwndFilename, buf, _countof(buf));
                pData->filename = buf;

                GetWindowTextW(pData->hwndDir, buf, _countof(buf));
                pData->saveDir = buf;

                int catSel = (int)SendMessageW(pData->hwndCat, CB_GETCURSEL, 0, 0);
                if (catSel != CB_ERR) {
                    SendMessageW(pData->hwndCat, CB_GETLBTEXT, catSel, (LPARAM)buf);
                    pData->category = buf;
                }

                int partSel = (int)SendMessageW(pData->hwndParts, CB_GETCURSEL, 0, 0);
                if (partSel != CB_ERR) {
                    LRESULT dataVal = SendMessageW(pData->hwndParts, CB_GETITEMDATA, partSel, 0);
                    if (dataVal > 0) {
                        pData->splitParts = (int)dataVal;
                    }
                }

                pData->startImmediately = (SendMessageW(pData->hwndStart, BM_GETCHECK, 0, 0) == BST_CHECKED);
                pData->accepted = true;
                DestroyWindow(hwnd);
            } else if (wmId == IDCANCEL) {
                pData->accepted = false;
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            if (pData) pData->accepted = false;
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Dialogs::ShowNewDownloadDialog(HWND hParent,
                                   const std::wstring& initialUrl,
                                   std::wstring& outUrl,
                                   std::wstring& outFilename,
                                   std::wstring& outSaveDir,
                                   std::wstring& outCategory,
                                   int& outSplitParts,
                                   bool& outStartImmediately)
{
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = NewDownloadDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyNewDownloadDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    NewDownloadDialogData data;
    data.url = initialUrl;
    if (data.url.empty()) {
        std::wstring clip = ClipboardWatcher::GetClipboardText(hParent);
        std::wstring extracted = WinHttpUtils::ExtractUrlFromText(clip);
        if (!extracted.empty()) {
            data.url = extracted;
        } else {
            std::wstring trimmed = WinHttpUtils::TrimUrl(clip);
            if (!trimmed.empty() && trimmed.find_first_of(L"\r\n") == std::wstring::npos && trimmed.length() < 2048) {
                data.url = trimmed;
            }
        }
    }
    if (!data.url.empty()) {
        data.filename = WinHttpUtils::ExtractFilenameFromUrl(data.url);
    }
    data.saveDir = Config::Instance().defaultSavePath;
    data.category = LStr(StrId::CatGeneral);
    data.splitParts = Config::Instance().defaultSplitParts;
    data.startImmediately = true;

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 650);
    int dlgH = DlgScale(hParent, 410);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyNewDownloadDlg",
        LStr(StrId::DlgNewTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hDlg) {
        HWND hUrl = GetDlgItem(hDlg, 101);
        if (hUrl) {
            SetFocus(hUrl);
            SendMessageW(hUrl, EM_SETSEL, 0, -1);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    if (data.accepted && !data.url.empty()) {
        outUrl = data.url;
        outFilename = data.filename;
        outSaveDir = data.saveDir;
        outCategory = data.category;
        outSplitParts = data.splitParts;
        outStartImmediately = data.startImmediately;
        return true;
    }

    return false;
}

// -------------------------------------------------------------
// 2. Batch Download Dialog (Toplu İndirme)
// -------------------------------------------------------------
struct BatchDialogData {
    HWND hwndLblPattern = NULL;
    HWND hwndPattern = NULL;
    HWND hwndLblFrom = NULL;
    HWND hwndFrom = NULL;
    HWND hwndLblTo = NULL;
    HWND hwndTo = NULL;
    HWND hwndLblDigits = NULL;
    HWND hwndDigits = NULL;
    HWND hwndBtnPreview = NULL;
    HWND hwndLblList = NULL;
    HWND hwndList = NULL;
    HWND hwndBtnCancel = NULL;
    HWND hwndBtnOk = NULL;

    HFONT hFont = NULL;
    HFONT hFontBold = NULL;

    std::vector<std::wstring> generatedUrls;
    CustomDialogHeader header;
};

static void LayoutBatchControls(HWND hwnd, BatchDialogData* pData) {
    if (!pData) return;
    RECT clR;
    GetClientRect(hwnd, &clR);
    int padX = DlgScale(hwnd, 30);
    int fieldW = clR.right - padX * 2;
    int inputH = DlgScale(hwnd, 28);
    int labelH = DlgScale(hwnd, 18);

    // Pattern
    int y0 = DlgScale(hwnd, 58);
    MoveWindow(pData->hwndLblPattern, padX, y0, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndPattern, padX, y0 + labelH + 2, fieldW, inputH, TRUE);

    // From / To / Digits row
    int y1 = y0 + inputH + labelH + DlgScale(hwnd, 10);
    int subW = DlgScale(hwnd, 65);
    int numEditW = DlgScale(hwnd, 60);

    MoveWindow(pData->hwndLblFrom, padX, y1 + 4, subW, labelH, TRUE);
    MoveWindow(pData->hwndFrom, padX + subW, y1, numEditW, inputH, TRUE);

    int xTo = padX + subW + numEditW + DlgScale(hwnd, 16);
    MoveWindow(pData->hwndLblTo, xTo, y1 + 4, DlgScale(hwnd, 40), labelH, TRUE);
    MoveWindow(pData->hwndTo, xTo + DlgScale(hwnd, 40), y1, numEditW, inputH, TRUE);

    int xDig = xTo + DlgScale(hwnd, 40) + numEditW + DlgScale(hwnd, 16);
    int digLblW = DlgScale(hwnd, 95);
    MoveWindow(pData->hwndLblDigits, xDig, y1 + 4, digLblW, labelH, TRUE);
    MoveWindow(pData->hwndDigits, xDig + digLblW, y1, DlgScale(hwnd, 45), inputH, TRUE);

    int prevW = DlgScale(hwnd, 95);
    MoveWindow(pData->hwndBtnPreview, padX + fieldW - prevW, y1, prevW, inputH, TRUE);

    // Generated List
    int y2 = y1 + inputH + DlgScale(hwnd, 10);
    MoveWindow(pData->hwndLblList, padX, y2, fieldW, labelH, TRUE);

    int listH = clR.bottom - DlgScale(hwnd, 64) - (y2 + labelH + 2);
    MoveWindow(pData->hwndList, padX, y2 + labelH + 2, fieldW, listH, TRUE);

    // Bottom Buttons
    int btnH = DlgScale(hwnd, 34);
    int yBtns = clR.bottom - DlgScale(hwnd, 46);
    int cancelW = DlgScale(hwnd, 105);
    int okW = DlgScale(hwnd, 185);
    int okX = clR.right - padX - okW;
    int cancelX = okX - cancelW - DlgScale(hwnd, 12);

    MoveWindow(pData->hwndBtnCancel, cancelX, yBtns, cancelW, btnH, TRUE);
    MoveWindow(pData->hwndBtnOk, okX, yBtns, okW, btnH, TRUE);
}

static LRESULT CALLBACK BatchDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BatchDialogData* pData = (BatchDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            pData = (BatchDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            pData->header.title = LStr(StrId::DlgBatchTitle);
            ApplyDialogDarkMode(hwnd);

            pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

            // Pattern
            pData->hwndLblPattern = CreateWindowW(L"STATIC", LStr(StrId::DlgBatchPattern), WS_CHILD | WS_VISIBLE,
                                                  0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblPattern, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndPattern = CreateWindowExW(0, L"EDIT", L"http://example.com/file_(*).zip",
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                                 0, 0, 0, 0, hwnd, (HMENU)201, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndPattern, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndPattern, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            // From / To / Digits row
            pData->hwndLblFrom = CreateWindowW(L"STATIC", LStr(StrId::DlgBatchFrom), WS_CHILD | WS_VISIBLE,
                                               0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblFrom, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndFrom = CreateWindowExW(0, L"EDIT", L"1",
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                                              0, 0, 0, 0, hwnd, (HMENU)202, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndFrom, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndFrom, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblTo = CreateWindowW(L"STATIC", LStr(StrId::DlgBatchTo), WS_CHILD | WS_VISIBLE,
                                             0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblTo, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndTo = CreateWindowExW(0, L"EDIT", L"10",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                                            0, 0, 0, 0, hwnd, (HMENU)203, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndTo, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndTo, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblDigits = CreateWindowW(L"STATIC", LStr(StrId::DlgBatchDigits), WS_CHILD | WS_VISIBLE,
                                                 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblDigits, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndDigits = CreateWindowExW(0, L"EDIT", L"2",
                                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                                                0, 0, 0, 0, hwnd, (HMENU)204, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndDigits, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndDigits, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnPreview = CreateWindowW(L"BUTTON", LStr(StrId::DlgBatchPreview), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                  0, 0, 0, 0, hwnd, (HMENU)205, GetModuleHandle(NULL), NULL);

            // Generated List
            pData->hwndLblList = CreateWindowW(L"STATIC", LStr(StrId::DlgBatchList), WS_CHILD | WS_VISIBLE,
                                               0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblList, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndList = CreateWindowExW(0, L"LISTBOX", L"",
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                              0, 0, 0, 0, hwnd, (HMENU)206, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndList, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndList, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            // Bottom Buttons
            pData->hwndBtnCancel = CreateWindowW(L"BUTTON", LStr(StrId::DlgClose), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            pData->hwndBtnOk = CreateWindowW(L"BUTTON", LStr(StrId::DlgBatchAddAll), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                             0, 0, 0, 0, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

            LayoutBatchControls(hwnd, pData);

            PostMessageW(hwnd, WM_COMMAND, 205, 0);

            if (!s_dialogSnapshotPath.empty()) {
                SetTimer(hwnd, 9999, 150, NULL);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (pData) {
                if (pData->hFont) DeleteObject(pData->hFont);
                if (pData->hFontBold) DeleteObject(pData->hFontBold);
                pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
                pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

                SendMessageW(pData->hwndLblPattern, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndPattern, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblFrom, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndFrom, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblTo, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndTo, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblDigits, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndDigits, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblList, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndList, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                LayoutBatchControls(hwnd, pData);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pData) {
                if (pData->hFont) { DeleteObject(pData->hFont); pData->hFont = NULL; }
                if (pData->hFontBold) { DeleteObject(pData->hFontBold); pData->hFontBold = NULL; }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 9999) {
                KillTimer(hwnd, 9999);
                CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                s_dialogSnapshotPath.clear();
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y <= DlgScale(hwnd, 42)) {
                if (pData && pData->header.HitTestClose(hwnd, pt.x, pt.y, rc.right)) {
                    return HTCLIENT;
                }
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                bool wasHov = pData->header.closeHovered;
                pData->header.closeHovered = pData->header.HitTestClose(hwnd, x, y, rc.right);
                if (wasHov != pData->header.closeHovered) {
                    RECT r = { rc.right - DlgScale(hwnd, 42), 0, rc.right, DlgScale(hwnd, 42) };
                    InvalidateRect(hwnd, &r, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == IDOK) {
                Theme::DrawModernButton(dis, true, L"⬇  " + std::wstring(LStr(StrId::DlgBatchAddAll)));
                return TRUE;
            } else if (dis->CtlID == IDCANCEL) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgClose));
                return TRUE;
            } else if (dis->CtlID == 205) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgBatchPreview));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgCard);
            static HBRUSH hbrCard = CreateSolidBrush(Theme::BgCard);
            return (LRESULT)hbrCard;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrEdit = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrEdit;
        }

        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrList = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrList;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == 205) { // Preview
                wchar_t patBuf[1024];
                GetWindowTextW(pData->hwndPattern, patBuf, _countof(patBuf));
                int fromVal = GetDlgItemInt(hwnd, 202, NULL, FALSE);
                int toVal = GetDlgItemInt(hwnd, 203, NULL, FALSE);
                int digits = GetDlgItemInt(hwnd, 204, NULL, FALSE);

                pData->generatedUrls = BatchExpander::ExpandNumeric(patBuf, fromVal, toVal, digits);
                SendMessageW(pData->hwndList, LB_RESETCONTENT, 0, 0);
                for (const auto& u : pData->generatedUrls) {
                    SendMessageW(pData->hwndList, LB_ADDSTRING, 0, (LPARAM)u.c_str());
                }
            } else if (wmId == IDOK) {
                if (pData->generatedUrls.empty()) {
                    wchar_t patBuf[1024];
                    GetWindowTextW(pData->hwndPattern, patBuf, _countof(patBuf));
                    int fromVal = GetDlgItemInt(hwnd, 202, NULL, FALSE);
                    int toVal = GetDlgItemInt(hwnd, 203, NULL, FALSE);
                    int digits = GetDlgItemInt(hwnd, 204, NULL, FALSE);
                    pData->generatedUrls = BatchExpander::ExpandNumeric(patBuf, fromVal, toVal, digits);
                }

                std::wstring saveDir = Config::Instance().defaultSavePath;
                int splitParts = Config::Instance().defaultSplitParts;
                for (const auto& u : pData->generatedUrls) {
                    DownloadManager::Instance().AddTask(u, saveDir, LStr(StrId::CatGeneral), splitParts, true);
                }
                DestroyWindow(hwnd);
            } else if (wmId == IDCANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Dialogs::ShowBatchDownloadDialog(HWND hParent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = BatchDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyBatchDownloadDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    BatchDialogData data;

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 660);
    int dlgH = DlgScale(hParent, 500);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyBatchDownloadDlg",
        LStr(StrId::DlgBatchTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }
    return true;
}

// -------------------------------------------------------------
// 2.5 Video Link Dialog (Link Ekle / Video İndirme)
// -------------------------------------------------------------
struct VideoLinkDialogData {
    HWND hwndLblUrl = NULL;
    HWND hwndUrl = NULL;
    HWND hwndBtnAnalyze = NULL;
    HWND hwndStatus = NULL;

    HWND hwndLblResults = NULL;
    HWND hwndList = NULL;
    HWND hwndComboQuality = NULL;
    HWND hwndBtnSelectAll = NULL;
    HWND hwndBtnDeselectAll = NULL;

    HWND hwndLblSave = NULL;
    HWND hwndSave = NULL;
    HWND hwndBtnBrowse = NULL;

    HWND hwndBtnCancel = NULL;
    HWND hwndBtnOk = NULL;

    HFONT hFont = NULL;
    HFONT hFontBold = NULL;

    CustomDialogHeader header;
    std::wstring initialUrl;
    std::wstring saveDir;
    std::atomic<bool> isAnalyzing = false;
    MediaExtractionResult result;
    std::thread workerThread;
};

static void LayoutVideoLinkControls(HWND hwnd, VideoLinkDialogData* pData) {
    if (!pData) return;
    RECT clR;
    GetClientRect(hwnd, &clR);
    int padX = DlgScale(hwnd, 30);
    int fieldW = clR.right - padX * 2;
    int inputH = DlgScale(hwnd, 28);
    int labelH = DlgScale(hwnd, 18);

    // URL row
    int y0 = DlgScale(hwnd, 58);
    MoveWindow(pData->hwndLblUrl, padX, y0, fieldW, labelH, TRUE);

    int btnAnalyzeW = DlgScale(hwnd, 110);
    int editUrlW = fieldW - btnAnalyzeW - DlgScale(hwnd, 10);
    MoveWindow(pData->hwndUrl, padX, y0 + labelH + 2, editUrlW, inputH, TRUE);
    MoveWindow(pData->hwndBtnAnalyze, padX + editUrlW + DlgScale(hwnd, 10), y0 + labelH + 2, btnAnalyzeW, inputH, TRUE);

    // Status label
    int y1 = y0 + labelH + 2 + inputH + DlgScale(hwnd, 8);
    MoveWindow(pData->hwndStatus, padX, y1, fieldW, labelH + 4, TRUE);

    // Results area
    int y2 = y1 + labelH + DlgScale(hwnd, 8);
    int comboW = DlgScale(hwnd, 180);
    int selBtnW = DlgScale(hwnd, 85);

    MoveWindow(pData->hwndLblResults, padX, y2, (std::max)(100, fieldW - comboW - selBtnW * 2 - DlgScale(hwnd, 20)), labelH, TRUE);
    MoveWindow(pData->hwndBtnSelectAll, padX + fieldW - comboW - selBtnW * 2 - DlgScale(hwnd, 10), y2 - 2, selBtnW, DlgScale(hwnd, 24), TRUE);
    MoveWindow(pData->hwndBtnDeselectAll, padX + fieldW - comboW - selBtnW - DlgScale(hwnd, 5), y2 - 2, selBtnW, DlgScale(hwnd, 24), TRUE);
    MoveWindow(pData->hwndComboQuality, padX + fieldW - comboW, y2 - 4, comboW, DlgScale(hwnd, 28), TRUE);

    int yList = y2 + labelH + 4;
    int listH = clR.bottom - DlgScale(hwnd, 120) - yList;
    MoveWindow(pData->hwndList, padX, yList, fieldW, listH, TRUE);

    // Save Dir row
    int ySave = clR.bottom - DlgScale(hwnd, 105);
    MoveWindow(pData->hwndLblSave, padX, ySave, fieldW, labelH, TRUE);

    int btnBrowseW = DlgScale(hwnd, 90);
    int editSaveW = fieldW - btnBrowseW - DlgScale(hwnd, 10);
    MoveWindow(pData->hwndSave, padX, ySave + labelH + 2, editSaveW, inputH, TRUE);
    MoveWindow(pData->hwndBtnBrowse, padX + editSaveW + DlgScale(hwnd, 10), ySave + labelH + 2, btnBrowseW, inputH, TRUE);

    // Bottom Buttons
    int btnH = DlgScale(hwnd, 34);
    int yBtns = clR.bottom - DlgScale(hwnd, 46);
    int cancelW = DlgScale(hwnd, 105);
    int okW = DlgScale(hwnd, 200);
    int okX = clR.right - padX - okW;
    int cancelX = okX - cancelW - DlgScale(hwnd, 12);

    MoveWindow(pData->hwndBtnCancel, cancelX, yBtns, cancelW, btnH, TRUE);
    MoveWindow(pData->hwndBtnOk, okX, yBtns, okW, btnH, TRUE);
}

static void StartVideoLinkAnalysis(HWND hwnd, VideoLinkDialogData* pData) {
    if (!pData || pData->isAnalyzing.load()) return;

    wchar_t urlBuf[2048];
    GetWindowTextW(pData->hwndUrl, urlBuf, _countof(urlBuf));
    std::wstring url = WinHttpUtils::TrimUrl(urlBuf);
    if (url.empty()) return;

    pData->isAnalyzing = true;
    EnableWindow(pData->hwndBtnAnalyze, FALSE);
    EnableWindow(pData->hwndBtnOk, FALSE);
    SetWindowTextW(pData->hwndStatus, (std::wstring(LStr(StrId::DlgVideoAnalyzing)) + L"...").c_str());

    if (pData->workerThread.joinable()) {
        pData->workerThread.join();
    }

    pData->workerThread = std::thread([hwnd, url]() {
        MediaExtractionResult res = MediaExtractor::ExtractInfo(url);
        PostMessageW(hwnd, WM_APP + 50, res.success ? 1 : 0, (LPARAM)new MediaExtractionResult(res));
    });
}

static void RefreshPlaylistListbox(VideoLinkDialogData* pData) {
    if (!pData) return;
    SendMessageW(pData->hwndList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < pData->result.items.size(); ++i) {
        const auto& item = pData->result.items[i];
        std::wstring check = item.isSelected ? L"[✓] " : L"[  ] ";
        std::wstring line = check + std::to_wstring(i + 1) + L". " + item.title +
                            L" (" + MediaExtractor::FormatDuration(item.durationSec) + L")";
        SendMessageW(pData->hwndList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
}

static LRESULT CALLBACK VideoLinkDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    VideoLinkDialogData* pData = (VideoLinkDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            pData = (VideoLinkDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            pData->header.title = L"🎬 " + std::wstring(LStr(StrId::DlgVideoTitle));
            ApplyDialogDarkMode(hwnd);

            pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

            // URL row
            pData->hwndLblUrl = CreateWindowW(L"STATIC", LStr(StrId::DlgVideoUrlPrompt), WS_CHILD | WS_VISIBLE,
                                              0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblUrl, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndUrl = CreateWindowExW(0, L"EDIT", pData->initialUrl.c_str(),
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                             0, 0, 0, 0, hwnd, (HMENU)201, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndUrl, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndUrl, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnAnalyze = CreateWindowW(L"BUTTON", LStr(StrId::DlgVideoAnalyze),
                                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                  0, 0, 0, 0, hwnd, (HMENU)202, GetModuleHandle(NULL), NULL);

            // Status label
            pData->hwndStatus = CreateWindowW(L"STATIC", LStr(StrId::DlgVideoUrlPrompt),
                                              WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_NOPREFIX,
                                              0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndStatus, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            // Results area
            pData->hwndLblResults = CreateWindowW(L"STATIC", LStr(StrId::DlgVideoQuality), WS_CHILD | WS_VISIBLE,
                                                  0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblResults, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndBtnSelectAll = CreateWindowW(L"BUTTON", LStr(StrId::DlgVideoSelectAll),
                                                    WS_CHILD | BS_OWNERDRAW,
                                                    0, 0, 0, 0, hwnd, (HMENU)205, GetModuleHandle(NULL), NULL);

            pData->hwndBtnDeselectAll = CreateWindowW(L"BUTTON", LStr(StrId::DlgVideoDeselectAll),
                                                      WS_CHILD | BS_OWNERDRAW,
                                                      0, 0, 0, 0, hwnd, (HMENU)206, GetModuleHandle(NULL), NULL);

            pData->hwndComboQuality = CreateWindowW(L"COMBOBOX", L"",
                                                    WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                                    0, 0, 0, 0, hwnd, (HMENU)207, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndComboQuality, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndComboQuality, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
            SendMessageW(pData->hwndComboQuality, CB_ADDSTRING, 0, (LPARAM)L"1080p Full HD (MP4)");
            SendMessageW(pData->hwndComboQuality, CB_ADDSTRING, 0, (LPARAM)L"720p HD (MP4)");
            SendMessageW(pData->hwndComboQuality, CB_ADDSTRING, 0, (LPARAM)L"En Yüksek Kalite (MP3)");
            SendMessageW(pData->hwndComboQuality, CB_SETCURSEL, 0, 0);

            pData->hwndList = CreateWindowExW(0, L"LISTBOX", L"",
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                              0, 0, 0, 0, hwnd, (HMENU)208, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndList, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndList, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            // Save row
            pData->hwndLblSave = CreateWindowW(L"STATIC", LStr(StrId::DlgVideoSaveDir), WS_CHILD | WS_VISIBLE,
                                               0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblSave, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndSave = CreateWindowExW(0, L"EDIT", pData->saveDir.c_str(),
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                              0, 0, 0, 0, hwnd, (HMENU)209, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndSave, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndSave, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnBrowse = CreateWindowW(L"BUTTON", LStr(StrId::DlgVideoBrowse),
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)210, GetModuleHandle(NULL), NULL);

            // Bottom Buttons
            pData->hwndBtnCancel = CreateWindowW(L"BUTTON", LStr(StrId::DlgClose),
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            pData->hwndBtnOk = CreateWindowW(L"BUTTON", LStr(StrId::DlgVideoDownload),
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                             0, 0, 0, 0, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);
            EnableWindow(pData->hwndBtnOk, FALSE);

            LayoutVideoLinkControls(hwnd, pData);

            if (!pData->initialUrl.empty()) {
                StartVideoLinkAnalysis(hwnd, pData);
            }

            if (!s_dialogSnapshotPath.empty()) {
                if (pData->initialUrl.empty()) {
                    SetTimer(hwnd, 9999, 150, NULL);
                }
            }
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (pData) {
                if (pData->hFont) DeleteObject(pData->hFont);
                if (pData->hFontBold) DeleteObject(pData->hFontBold);
                pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
                pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

                SendMessageW(pData->hwndLblUrl, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndUrl, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndStatus, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblResults, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndComboQuality, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndList, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblSave, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndSave, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                LayoutVideoLinkControls(hwnd, pData);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_APP + 50: {
            if (!pData) return 0;
            pData->isAnalyzing = false;
            EnableWindow(pData->hwndBtnAnalyze, TRUE);

            MediaExtractionResult* pRes = (MediaExtractionResult*)lParam;
            if (pRes) {
                pData->result = *pRes;
                delete pRes;
            }

            SendMessageW(pData->hwndList, LB_RESETCONTENT, 0, 0);

            if (!pData->result.success) {
                std::wstring err = pData->result.errorMsg.empty() ? L"Medya bilgisi alınamadı veya bağlantı desteklenmiyor." : pData->result.errorMsg;
                SetWindowTextW(pData->hwndStatus, (L"❌ " + err).c_str());
                EnableWindow(pData->hwndBtnOk, FALSE);
                return 0;
            }

            EnableWindow(pData->hwndBtnOk, TRUE);

            if (pData->result.isPlaylist) {
                std::wstring statusStr = L"▶ " + pData->result.title + L" (" + std::to_wstring(pData->result.items.size()) + L" video)";
                SetWindowTextW(pData->hwndStatus, statusStr.c_str());
                SetWindowTextW(pData->hwndLblResults, LStr(StrId::DlgVideoPlaylistVideos));

                ShowWindow(pData->hwndComboQuality, SW_SHOW);
                ShowWindow(pData->hwndBtnSelectAll, SW_SHOW);
                ShowWindow(pData->hwndBtnDeselectAll, SW_SHOW);

                RefreshPlaylistListbox(pData);
            } else if (!pData->result.items.empty()) {
                const auto& item = pData->result.items[0];
                std::wstring statusStr = L"▶ " + item.title + L" (" + MediaExtractor::FormatDuration(item.durationSec) + L")";
                if (!item.uploader.empty()) {
                    statusStr += L" • " + item.uploader;
                }
                SetWindowTextW(pData->hwndStatus, statusStr.c_str());
                SetWindowTextW(pData->hwndLblResults, LStr(StrId::DlgVideoQuality));

                ShowWindow(pData->hwndComboQuality, SW_HIDE);
                ShowWindow(pData->hwndBtnSelectAll, SW_HIDE);
                ShowWindow(pData->hwndBtnDeselectAll, SW_HIDE);

                for (const auto& fmt : item.formats) {
                    std::wstring line;
                    if (fmt.isAudioOnly) {
                        line = L"🎵 " + fmt.resolution;
                    } else {
                        line = L"🎬 " + fmt.resolution;
                    }
                    if (!fmt.note.empty()) line += L" [" + fmt.note + L"]";
                    if (fmt.filesize > 0) line += L" (~" + MediaExtractor::FormatBytes(fmt.filesize) + L")";
                    SendMessageW(pData->hwndList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
                }
                SendMessageW(pData->hwndList, LB_SETCURSEL, 0, 0);
            }

            LayoutVideoLinkControls(hwnd, pData);
            InvalidateRect(hwnd, NULL, TRUE);

            if (!s_dialogSnapshotPath.empty()) {
                SetTimer(hwnd, 9999, 400, NULL);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pData) {
                if (pData->workerThread.joinable()) {
                    pData->workerThread.detach();
                }
                if (pData->hFont) { DeleteObject(pData->hFont); pData->hFont = NULL; }
                if (pData->hFontBold) { DeleteObject(pData->hFontBold); pData->hFontBold = NULL; }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 9999) {
                KillTimer(hwnd, 9999);
                CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                s_dialogSnapshotPath.clear();
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y < DlgScale(hwnd, 46) && pt.x < rc.right - DlgScale(hwnd, 50)) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (pData) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == IDOK) {
                Theme::DrawModernButton(dis, true, L"⬇  " + std::wstring(LStr(StrId::DlgVideoDownload)));
                return TRUE;
            } else if (dis->CtlID == IDCANCEL) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgClose));
                return TRUE;
            } else if (dis->CtlID == 202) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgVideoAnalyze));
                return TRUE;
            } else if (dis->CtlID == 205) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgVideoSelectAll));
                return TRUE;
            } else if (dis->CtlID == 206) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgVideoDeselectAll));
                return TRUE;
            } else if (dis->CtlID == 210) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgVideoBrowse));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            if (pData && hCtl == pData->hwndStatus) {
                SetTextColor(hdc, Theme::AccentCyan);
            } else {
                SetTextColor(hdc, Theme::TextPrimary);
            }
            SetBkColor(hdc, Theme::BgCard);
            static HBRUSH hbrCard = CreateSolidBrush(Theme::BgCard);
            return (LRESULT)hbrCard;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrEdit = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrEdit;
        }

        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrList = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrList;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == 202) { // Analyze
                StartVideoLinkAnalysis(hwnd, pData);
            } else if (wmId == 205) { // Select All
                if (pData && pData->result.isPlaylist) {
                    for (auto& item : pData->result.items) item.isSelected = true;
                    RefreshPlaylistListbox(pData);
                }
            } else if (wmId == 206) { // Deselect All
                if (pData && pData->result.isPlaylist) {
                    for (auto& item : pData->result.items) item.isSelected = false;
                    RefreshPlaylistListbox(pData);
                }
            } else if (wmId == 208 && wmEvent == LBN_DBLCLK) { // Double click on list
                if (pData) {
                    int curSel = (int)SendMessageW(pData->hwndList, LB_GETCURSEL, 0, 0);
                    if (pData->result.isPlaylist) {
                        if (curSel >= 0 && curSel < (int)pData->result.items.size()) {
                            pData->result.items[curSel].isSelected = !pData->result.items[curSel].isSelected;
                            RefreshPlaylistListbox(pData);
                            SendMessageW(pData->hwndList, LB_SETCURSEL, curSel, 0);
                        }
                    } else {
                        // Single video format selection -> start download immediately!
                        PostMessageW(hwnd, WM_COMMAND, IDOK, 0);
                    }
                }
            } else if (wmId == 210) { // Browse save folder
                BROWSEINFOW bi = { 0 };
                bi.hwndOwner = hwnd;
                bi.lpszTitle = L"İndirilecek videolar için kayıt klasörünü seçin:";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    wchar_t path[MAX_PATH];
                    if (SHGetPathFromIDListW(pidl, path)) {
                        SetWindowTextW(pData->hwndSave, path);
                    }
                    CoTaskMemFree(pidl);
                }
            } else if (wmId == IDOK) {
                if (!pData || !pData->result.success) return 0;

                wchar_t sdirBuf[MAX_PATH];
                GetWindowTextW(pData->hwndSave, sdirBuf, _countof(sdirBuf));
                std::wstring saveDir = sdirBuf;
                if (saveDir.empty()) saveDir = Config::Instance().defaultSavePath;

                if (pData->result.isPlaylist) {
                    int selQ = (int)SendMessageW(pData->hwndComboQuality, CB_GETCURSEL, 0, 0);
                    std::wstring chosenFormat = L"bestvideo[height<=1080]+bestaudio/best[height<=1080]/best";
                    std::wstring category = LStr(StrId::CatVideo);
                    if (selQ == 1) {
                        chosenFormat = L"bestvideo[height<=720]+bestaudio/best[height<=720]/best";
                    } else if (selQ == 2) {
                        chosenFormat = L"mp3";
                        category = LStr(StrId::CatMusic);
                    }

                    for (const auto& it : pData->result.items) {
                        if (it.isSelected) {
                            DownloadManager::Instance().AddMediaTask(it.url, saveDir, category, it.title, chosenFormat, true);
                        }
                    }
                } else if (!pData->result.items.empty()) {
                    const auto& item = pData->result.items[0];
                    int selFmt = (int)SendMessageW(pData->hwndList, LB_GETCURSEL, 0, 0);
                    std::wstring formatId = L"bestvideo+bestaudio/best";
                    std::wstring category = LStr(StrId::CatVideo);
                    if (selFmt >= 0 && selFmt < (int)item.formats.size()) {
                        formatId = item.formats[selFmt].formatId;
                        if (item.formats[selFmt].isAudioOnly) {
                            category = LStr(StrId::CatMusic);
                        }
                    }
                    DownloadManager::Instance().AddMediaTask(item.url, saveDir, category, item.title, formatId, true);
                }

                DestroyWindow(hwnd);
            } else if (wmId == IDCANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Dialogs::ShowVideoLinkDialog(HWND hParent, const std::wstring& initialUrl) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = VideoLinkDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyVideoLinkDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    VideoLinkDialogData data;
    data.saveDir = Config::Instance().defaultSavePath;

    if (!initialUrl.empty()) {
        data.initialUrl = initialUrl;
    } else {
        // Check clipboard for prefill
        std::wstring clipUrl = ClipboardWatcher::GetClipboardText(hParent);
        std::wstring extracted = WinHttpUtils::ExtractUrlFromText(clipUrl);
        if (!extracted.empty()) {
            data.initialUrl = extracted;
        } else {
            clipUrl = WinHttpUtils::TrimUrl(clipUrl);
            if (clipUrl.rfind(L"http://", 0) == 0 || clipUrl.rfind(L"https://", 0) == 0) {
                data.initialUrl = clipUrl;
            }
        }
    }

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 700);
    int dlgH = DlgScale(hParent, 520);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyVideoLinkDlg",
        LStr(StrId::DlgVideoTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }
    return true;
}

// -------------------------------------------------------------
// 2.5 Ollama Model Dialog (Yapay Zeka / Ollama Modeli İndir)
// -------------------------------------------------------------
#define WM_APP_OLLAMA_DONE   (WM_APP + 50)
#define WM_APP_OLLAMA_STATUS (WM_APP + 51)

// -------------------------------------------------------------
// 2.5 Unified AI Model Downloader Dialog (Ollama & Hugging Face)
// -------------------------------------------------------------
#define WM_APP_OLLAMA_DONE   (WM_APP + 50)
#define WM_APP_OLLAMA_STATUS (WM_APP + 51)
#define WM_APP_HF_DONE       (WM_APP + 52)
#define WM_APP_HF_STATUS     (WM_APP + 53)

#define ID_TAB_OLLAMA        320
#define ID_TAB_HF            321

#define ID_HF_REPO           340
#define ID_HF_ANALYZE        341
#define ID_HF_TOKEN          342
#define ID_HF_COMBO_FILE     343
#define ID_HF_FILENAME       344

struct ModelDialogData {
    int activeTab = 0; // 0 = Ollama, 1 = Hugging Face

    // Top Segmented Tabs
    HWND hwndTabOllama = NULL;
    HWND hwndTabHf = NULL;

    // --- Ollama Tab Controls ---
    HWND hwndLblModel = NULL;
    HWND hwndModel = NULL;
    HWND hwndBtnAnalyze = NULL;
    HWND hwndStatus = NULL;

    HWND hwndLblFormat = NULL;
    HWND hwndRadioGguf = NULL;
    HWND hwndLblRadioGguf = NULL;
    HWND hwndRadioOriginal = NULL;
    HWND hwndLblRadioOriginal = NULL;

    HWND hwndLblFilename = NULL;
    HWND hwndFilename = NULL;

    HWND hwndLblDetails = NULL;
    HWND hwndDetails = NULL;

    // --- Hugging Face Tab Controls ---
    HWND hwndLblHfRepo = NULL;
    HWND hwndHfRepo = NULL;
    HWND hwndBtnHfAnalyze = NULL;
    HWND hwndLblHfToken = NULL;
    HWND hwndHfToken = NULL;
    HWND hwndHfStatus = NULL;

    HWND hwndLblHfQuant = NULL;
    HWND hwndComboHfFiles = NULL;

    HWND hwndLblHfFilename = NULL;
    HWND hwndHfFilename = NULL;

    HWND hwndLblHfDetails = NULL;
    HWND hwndHfDetails = NULL;

    // --- Shared Bottom Controls ---
    HWND hwndLblSave = NULL;
    HWND hwndSave = NULL;
    HWND hwndBtnBrowse = NULL;

    HWND hwndLblParts = NULL;
    HWND hwndComboParts = NULL;

    HWND hwndBtnCancel = NULL;
    HWND hwndBtnOk = NULL;

    HFONT hFont = NULL;
    HFONT hFontBold = NULL;
    HFONT hFontMono = NULL;

    CustomDialogHeader header;
    std::wstring initialModel;
    std::wstring saveDir;
    int splitParts = 10;

    // Ollama state
    std::atomic<bool> isAnalyzing = false;
    OllamaFetchResult fetchResult;
    std::thread workerThread;
    OllamaDownloadFormat selectedFormat = OllamaDownloadFormat::Gguf;

    // Hugging Face state
    std::atomic<bool> isHfAnalyzing = false;
    HuggingFaceFetchResult hfFetchResult;
    std::thread hfWorkerThread;
};

static void SwitchModelDialogTab(HWND hwnd, ModelDialogData* pData, int newTab) {
    if (!pData) return;
    pData->activeTab = newTab;

    // Show/Hide Ollama controls
    int showOllama = (newTab == 0) ? SW_SHOW : SW_HIDE;
    if (pData->hwndLblModel) ShowWindow(pData->hwndLblModel, showOllama);
    if (pData->hwndModel) ShowWindow(pData->hwndModel, showOllama);
    if (pData->hwndBtnAnalyze) ShowWindow(pData->hwndBtnAnalyze, showOllama);
    if (pData->hwndStatus) ShowWindow(pData->hwndStatus, showOllama);
    if (pData->hwndLblFormat) ShowWindow(pData->hwndLblFormat, showOllama);
    if (pData->hwndRadioGguf) ShowWindow(pData->hwndRadioGguf, showOllama);
    if (pData->hwndLblRadioGguf) ShowWindow(pData->hwndLblRadioGguf, showOllama);
    if (pData->hwndRadioOriginal) ShowWindow(pData->hwndRadioOriginal, showOllama);
    if (pData->hwndLblRadioOriginal) ShowWindow(pData->hwndLblRadioOriginal, showOllama);
    if (pData->hwndLblFilename) ShowWindow(pData->hwndLblFilename, showOllama);
    if (pData->hwndFilename) ShowWindow(pData->hwndFilename, showOllama);
    if (pData->hwndLblDetails) ShowWindow(pData->hwndLblDetails, showOllama);
    if (pData->hwndDetails) ShowWindow(pData->hwndDetails, showOllama);

    // Show/Hide Hugging Face controls
    int showHf = (newTab == 1) ? SW_SHOW : SW_HIDE;
    if (pData->hwndLblHfRepo) ShowWindow(pData->hwndLblHfRepo, showHf);
    if (pData->hwndHfRepo) ShowWindow(pData->hwndHfRepo, showHf);
    if (pData->hwndBtnHfAnalyze) ShowWindow(pData->hwndBtnHfAnalyze, showHf);
    if (pData->hwndLblHfToken) ShowWindow(pData->hwndLblHfToken, showHf);
    if (pData->hwndHfToken) ShowWindow(pData->hwndHfToken, showHf);
    if (pData->hwndHfStatus) ShowWindow(pData->hwndHfStatus, showHf);
    if (pData->hwndLblHfQuant) ShowWindow(pData->hwndLblHfQuant, showHf);
    if (pData->hwndComboHfFiles) ShowWindow(pData->hwndComboHfFiles, showHf);
    if (pData->hwndLblHfFilename) ShowWindow(pData->hwndLblHfFilename, showHf);
    if (pData->hwndHfFilename) ShowWindow(pData->hwndHfFilename, showHf);
    if (pData->hwndLblHfDetails) ShowWindow(pData->hwndLblHfDetails, showHf);
    if (pData->hwndHfDetails) ShowWindow(pData->hwndHfDetails, showHf);

    // Update Header & Action Button
    if (newTab == 0) {
        pData->header.title = LStr(StrId::DlgOllamaTitle);
        EnableWindow(pData->hwndBtnOk, pData->fetchResult.success);
    } else {
        pData->header.title = LStr(StrId::DlgHfTitle);
        EnableWindow(pData->hwndBtnOk, pData->hfFetchResult.success && !pData->hfFetchResult.info.files.empty());
    }

    InvalidateRect(pData->hwndTabOllama, NULL, TRUE);
    InvalidateRect(pData->hwndTabHf, NULL, TRUE);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void LayoutModelControls(HWND hwnd, ModelDialogData* pData) {
    if (!pData) return;
    RECT clR;
    GetClientRect(hwnd, &clR);
    int padX = DlgScale(hwnd, 30);
    int fieldW = clR.right - padX * 2;
    int inputH = DlgScale(hwnd, 28);
    int labelH = DlgScale(hwnd, 18);

    // 1. Top Segmented Tabs Bar
    int yTab = DlgScale(hwnd, 48);
    int tabH = DlgScale(hwnd, 32);
    int tabW = DlgScale(hwnd, 220);
    MoveWindow(pData->hwndTabOllama, padX, yTab, tabW, tabH, TRUE);
    MoveWindow(pData->hwndTabHf, padX + tabW + DlgScale(hwnd, 12), yTab, tabW, tabH, TRUE);

    // Bottom Action Buttons (Shared)
    int btnH = DlgScale(hwnd, 34);
    int yBtns = clR.bottom - DlgScale(hwnd, 46);
    int cancelW = DlgScale(hwnd, 105);
    int okW = DlgScale(hwnd, 220);
    int okX = clR.right - padX - okW;
    int cancelX = okX - cancelW - DlgScale(hwnd, 12);
    MoveWindow(pData->hwndBtnCancel, cancelX, yBtns, cancelW, btnH, TRUE);
    MoveWindow(pData->hwndBtnOk, okX, yBtns, okW, btnH, TRUE);

    // Save Dir & Split parts row (Shared)
    int ySave = clR.bottom - DlgScale(hwnd, 105);
    MoveWindow(pData->hwndLblSave, padX, ySave, fieldW - DlgScale(hwnd, 130), labelH, TRUE);
    MoveWindow(pData->hwndLblParts, padX + fieldW - DlgScale(hwnd, 110), ySave, DlgScale(hwnd, 110), labelH, TRUE);

    int btnBrowseW = DlgScale(hwnd, 85);
    int partsW = DlgScale(hwnd, 110);
    int editSaveW = fieldW - btnBrowseW - partsW - DlgScale(hwnd, 20);
    MoveWindow(pData->hwndSave, padX, ySave + labelH + 2, editSaveW, inputH, TRUE);
    MoveWindow(pData->hwndBtnBrowse, padX + editSaveW + DlgScale(hwnd, 10), ySave + labelH + 2, btnBrowseW, inputH, TRUE);
    MoveWindow(pData->hwndComboParts, padX + fieldW - partsW, ySave + labelH + 2, partsW, DlgScale(hwnd, 120), TRUE);

    // --- TAB 0 (Ollama) Layout ---
    int yOllama0 = yTab + tabH + DlgScale(hwnd, 10);
    int btnAnalyzeW = DlgScale(hwnd, 125);
    int editModelW = fieldW - btnAnalyzeW - DlgScale(hwnd, 10);
    MoveWindow(pData->hwndLblModel, padX, yOllama0, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndModel, padX, yOllama0 + labelH + 2, editModelW, inputH, TRUE);
    MoveWindow(pData->hwndBtnAnalyze, padX + editModelW + DlgScale(hwnd, 10), yOllama0 + labelH + 2, btnAnalyzeW, inputH, TRUE);

    int yOllama1 = yOllama0 + labelH + 2 + inputH + DlgScale(hwnd, 4);
    MoveWindow(pData->hwndStatus, padX, yOllama1, fieldW, labelH + 4, TRUE);

    int yOllama2 = yOllama1 + labelH + DlgScale(hwnd, 4);
    MoveWindow(pData->hwndLblFormat, padX, yOllama2, fieldW, labelH, TRUE);

    int radioH = DlgScale(hwnd, 20);
    int radioBoxW = DlgScale(hwnd, 20);
    int radioTextW = fieldW - radioBoxW - DlgScale(hwnd, 8);
    MoveWindow(pData->hwndRadioGguf, padX + DlgScale(hwnd, 6), yOllama2 + labelH + 2, radioBoxW, radioH, TRUE);
    MoveWindow(pData->hwndLblRadioGguf, padX + DlgScale(hwnd, 30), yOllama2 + labelH + 2, radioTextW, radioH, TRUE);
    MoveWindow(pData->hwndRadioOriginal, padX + DlgScale(hwnd, 6), yOllama2 + labelH + 2 + radioH + 2, radioBoxW, radioH, TRUE);
    MoveWindow(pData->hwndLblRadioOriginal, padX + DlgScale(hwnd, 30), yOllama2 + labelH + 2 + radioH + 2, radioTextW, radioH, TRUE);

    int yOllama3 = yOllama2 + labelH + 2 + radioH * 2 + DlgScale(hwnd, 6);
    MoveWindow(pData->hwndLblFilename, padX, yOllama3, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndFilename, padX, yOllama3 + labelH + 2, fieldW, inputH, TRUE);

    int yOllama4 = yOllama3 + labelH + 2 + inputH + DlgScale(hwnd, 6);
    MoveWindow(pData->hwndLblDetails, padX, yOllama4, fieldW, labelH, TRUE);
    int detailsOllamaH = ySave - (yOllama4 + labelH + 4) - DlgScale(hwnd, 8);
    if (detailsOllamaH < DlgScale(hwnd, 60)) detailsOllamaH = DlgScale(hwnd, 60);
    MoveWindow(pData->hwndDetails, padX, yOllama4 + labelH + 2, fieldW, detailsOllamaH, TRUE);

    // --- TAB 1 (Hugging Face) Layout ---
    int yHf0 = yTab + tabH + DlgScale(hwnd, 10);
    int btnHfAnalyzeW = DlgScale(hwnd, 125);
    int editHfRepoW = fieldW - btnHfAnalyzeW - DlgScale(hwnd, 10);
    MoveWindow(pData->hwndLblHfRepo, padX, yHf0, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndHfRepo, padX, yHf0 + labelH + 2, editHfRepoW, inputH, TRUE);
    MoveWindow(pData->hwndBtnHfAnalyze, padX + editHfRepoW + DlgScale(hwnd, 10), yHf0 + labelH + 2, btnHfAnalyzeW, inputH, TRUE);

    int yHfToken = yHf0 + labelH + 2 + inputH + DlgScale(hwnd, 4);
    MoveWindow(pData->hwndLblHfToken, padX, yHfToken, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndHfToken, padX, yHfToken + labelH + 2, fieldW, inputH, TRUE);

    int yHf1 = yHfToken + labelH + 2 + inputH + DlgScale(hwnd, 4);
    MoveWindow(pData->hwndHfStatus, padX, yHf1, fieldW, labelH + 4, TRUE);

    int yHf2 = yHf1 + labelH + DlgScale(hwnd, 4);
    MoveWindow(pData->hwndLblHfQuant, padX, yHf2, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndComboHfFiles, padX, yHf2 + labelH + 2, fieldW, DlgScale(hwnd, 200), TRUE);

    int yHf3 = yHf2 + labelH + 2 + inputH + DlgScale(hwnd, 6);
    MoveWindow(pData->hwndLblHfFilename, padX, yHf3, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndHfFilename, padX, yHf3 + labelH + 2, fieldW, inputH, TRUE);

    int yHf4 = yHf3 + labelH + 2 + inputH + DlgScale(hwnd, 6);
    MoveWindow(pData->hwndLblHfDetails, padX, yHf4, fieldW, labelH, TRUE);
    int detailsHfH = ySave - (yHf4 + labelH + 4) - DlgScale(hwnd, 8);
    if (detailsHfH < DlgScale(hwnd, 60)) detailsHfH = DlgScale(hwnd, 60);
    MoveWindow(pData->hwndHfDetails, padX, yHf4 + labelH + 2, fieldW, detailsHfH, TRUE);
}

static void StartOllamaAnalysis(HWND hwnd, ModelDialogData* pData) {
    if (!pData || pData->isAnalyzing.load()) return;

    wchar_t modelBuf[2048];
    GetWindowTextW(pData->hwndModel, modelBuf, _countof(modelBuf));
    std::wstring rawInput = WinHttpUtils::TrimUrl(modelBuf);
    if (rawInput.empty()) return;

    OllamaModelSpec spec = OllamaClient::ParseModelSpec(rawInput);
    if (!spec.isValid) {
        SetWindowTextW(pData->hwndStatus, L"❌ Geçersiz model adı! Örnek format: nomic-embed-text-v2-moe:latest veya llama3.2:1b");
        return;
    }

    pData->isAnalyzing = true;
    EnableWindow(pData->hwndBtnAnalyze, FALSE);
    EnableWindow(pData->hwndBtnOk, FALSE);
    SetWindowTextW(pData->hwndStatus, (std::wstring(LStr(StrId::DlgOllamaAnalyzing)) + L" (" + spec.displayName + L")").c_str());
    SetWindowTextW(pData->hwndDetails, L"Model manifesti sorgulanıyor...");

    if (pData->workerThread.joinable()) {
        pData->workerThread.join();
    }

    pData->workerThread = std::thread([hwnd, spec]() {
        OllamaFetchResult res = OllamaClient::FetchModelInfo(spec, [hwnd](const std::wstring& msg) {
            wchar_t* copyMsg = _wcsdup(msg.c_str());
            PostMessageW(hwnd, WM_APP_OLLAMA_STATUS, 0, (LPARAM)copyMsg);
        });

        auto* pRes = new OllamaFetchResult(std::move(res));
        PostMessageW(hwnd, WM_APP_OLLAMA_DONE, 0, (LPARAM)pRes);
    });
}

static void StartHuggingFaceAnalysis(HWND hwnd, ModelDialogData* pData) {
    if (!pData || pData->isHfAnalyzing.load()) return;

    wchar_t repoBuf[2048];
    GetWindowTextW(pData->hwndHfRepo, repoBuf, _countof(repoBuf));
    std::wstring rawInput = WinHttpUtils::TrimUrl(repoBuf);
    if (rawInput.empty()) return;

    HuggingFaceModelSpec spec = HuggingFaceClient::ParseModelSpec(rawInput);
    if (!spec.isValid) {
        SetWindowTextW(pData->hwndHfStatus, L"❌ Geçersiz Hugging Face model kimliği! Örnek: TheBloke/Mistral-7B-Instruct-v0.2-GGUF");
        return;
    }

    wchar_t tokBuf[1024];
    GetWindowTextW(pData->hwndHfToken, tokBuf, _countof(tokBuf));
    std::wstring token = WinHttpUtils::TrimUrl(tokBuf);

    pData->isHfAnalyzing = true;
    EnableWindow(pData->hwndBtnHfAnalyze, FALSE);
    EnableWindow(pData->hwndBtnOk, FALSE);
    SetWindowTextW(pData->hwndHfStatus, (std::wstring(LStr(StrId::DlgHfAnalyzing)) + L" (" + spec.displayName + L")").c_str());
    SetWindowTextW(pData->hwndHfDetails, L"Hugging Face dosya ağacı sorgulanıyor...");

    if (pData->hfWorkerThread.joinable()) {
        pData->hfWorkerThread.join();
    }

    pData->hfWorkerThread = std::thread([hwnd, spec, token]() {
        HuggingFaceFetchResult res = HuggingFaceClient::FetchModelTree(spec, token, [hwnd](const std::wstring& msg) {
            wchar_t* copyMsg = _wcsdup(msg.c_str());
            PostMessageW(hwnd, WM_APP_HF_STATUS, 0, (LPARAM)copyMsg);
        });

        auto* pRes = new HuggingFaceFetchResult(std::move(res));
        PostMessageW(hwnd, WM_APP_HF_DONE, 0, (LPARAM)pRes);
    });
}

static LRESULT CALLBACK ModelDownloaderDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ModelDialogData* pData = (ModelDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            pData = (ModelDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);

            ApplyDialogDarkMode(hwnd);
            pData->header.title = (pData->activeTab == 0) ? LStr(StrId::DlgOllamaTitle) : LStr(StrId::DlgHfTitle);

            pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_BOLD);
            pData->hFontMono = Theme::CreateAppFont(hwnd, 9, FW_NORMAL, L"Consolas");

            // Segmented Tab Buttons
            pData->hwndTabOllama = CreateWindowExW(0, L"BUTTON", LStr(StrId::DlgModelTabOllama),
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)ID_TAB_OLLAMA, GetModuleHandle(NULL), NULL);

            pData->hwndTabHf = CreateWindowExW(0, L"BUTTON", LStr(StrId::DlgModelTabHf),
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)ID_TAB_HF, GetModuleHandle(NULL), NULL);

            // ==================== OLLAMA CONTROLS ====================
            std::wstring initialOllama = (pData->activeTab == 0) ? pData->initialModel : L"";
            pData->hwndLblModel = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgOllamaUrlPrompt),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblModel, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndModel = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initialOllama.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)301, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndModel, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndModel, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnAnalyze = CreateWindowExW(0, L"BUTTON", LStr(StrId::DlgOllamaAnalyze),
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)302, GetModuleHandle(NULL), NULL);

            pData->hwndStatus = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndStatus, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            bool isTr = (I18n::Instance().GetCurrentLanguage() == LangId::Turkish);
            pData->hwndLblFormat = CreateWindowExW(0, L"STATIC", isTr ? L"İndirme Formatı Seçin:" : L"Select Download Format:",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblFormat, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndRadioGguf = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)303, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndRadioGguf, BM_SETCHECK, BST_CHECKED, 0);

            pData->hwndLblRadioGguf = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgOllamaFormatGguf),
                WS_CHILD | WS_VISIBLE | SS_NOTIFY | SS_NOPREFIX, 0, 0, 0, 0, hwnd, (HMENU)333, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblRadioGguf, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndRadioOriginal = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)304, GetModuleHandle(NULL), NULL);

            pData->hwndLblRadioOriginal = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgOllamaFormatOriginal),
                WS_CHILD | WS_VISIBLE | SS_NOTIFY | SS_NOPREFIX, 0, 0, 0, 0, hwnd, (HMENU)334, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblRadioOriginal, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblFilename = CreateWindowExW(0, L"STATIC", (std::wstring(LStr(StrId::DlgOllamaFilename)) + L" (GGUF .gguf):").c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblFilename, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndFilename = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)305, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndFilename, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndFilename, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblDetails = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgOllamaModelInfo),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblDetails, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndDetails = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                isTr ? L"Model adını girip 'Modeli İncele' butonuna basın." : L"Enter model name and click 'Inspect Model'.",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_TABSTOP,
                0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndDetails, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndDetails, WM_SETFONT, (WPARAM)pData->hFontMono, TRUE);

            // ==================== HUGGING FACE CONTROLS ====================
            std::wstring initialHf = (pData->activeTab == 1) ? pData->initialModel : L"";
            pData->hwndLblHfRepo = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgHfRepoPrompt),
                WS_CHILD | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblHfRepo, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndHfRepo = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initialHf.c_str(),
                WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)ID_HF_REPO, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndHfRepo, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndHfRepo, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnHfAnalyze = CreateWindowExW(0, L"BUTTON", LStr(StrId::DlgHfAnalyze),
                WS_CHILD | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)ID_HF_ANALYZE, GetModuleHandle(NULL), NULL);

            pData->hwndLblHfToken = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgHfTokenPrompt),
                WS_CHILD | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblHfToken, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndHfToken = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Config::Instance().huggingFaceToken.c_str(),
                WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)ID_HF_TOKEN, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndHfToken, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndHfToken, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndHfStatus = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndHfStatus, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndLblHfQuant = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgHfQuantPrompt),
                WS_CHILD | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblHfQuant, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndComboHfFiles = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)ID_HF_COMBO_FILE, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndComboHfFiles, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndComboHfFiles, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblHfFilename = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgHfFilename),
                WS_CHILD | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblHfFilename, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndHfFilename = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)ID_HF_FILENAME, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndHfFilename, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndHfFilename, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblHfDetails = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgHfModelInfo),
                WS_CHILD | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblHfDetails, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndHfDetails = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                isTr ? L"Model deposunu girip 'Modeli İncele' butonuna basın." : L"Enter repository (e.g. TheBloke/Mistral-7B-Instruct-v0.2-GGUF) and click 'Inspect Model'.",
                WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_TABSTOP,
                0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndHfDetails, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndHfDetails, WM_SETFONT, (WPARAM)pData->hFontMono, TRUE);

            // ==================== SHARED CONTROLS ====================
            pData->hwndLblSave = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgOllamaSaveDir),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblSave, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndSave = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pData->saveDir.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndSave, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndSave, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnBrowse = CreateWindowExW(0, L"BUTTON", LStr(StrId::DlgNewBrowse),
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)310, GetModuleHandle(NULL), NULL);

            pData->hwndLblParts = CreateWindowExW(0, L"STATIC", LStr(StrId::DlgOllamaParts),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblParts, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndComboParts = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)311, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndComboParts, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndComboParts, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            const int partOptions[] = { 1, 2, 4, 8, 10, 16, 24, 30 };
            int selIdx = 4; // default 10
            for (size_t i = 0; i < _countof(partOptions); ++i) {
                std::wstring text = std::to_wstring(partOptions[i]) + (isTr ? L" Parça" : L" Parts");
                SendMessageW(pData->hwndComboParts, CB_ADDSTRING, 0, (LPARAM)text.c_str());
                if (partOptions[i] == pData->splitParts) selIdx = (int)i;
            }
            SendMessageW(pData->hwndComboParts, CB_SETCURSEL, selIdx, 0);

            pData->hwndBtnCancel = CreateWindowExW(0, L"BUTTON", LStr(StrId::DlgClose),
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            pData->hwndBtnOk = CreateWindowExW(0, L"BUTTON", (L"⬇  " + std::wstring(LStr(StrId::DlgOllamaDownload))).c_str(),
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP | WS_DISABLED, 0, 0, 0, 0, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

            LayoutModelControls(hwnd, pData);
            SwitchModelDialogTab(hwnd, pData, pData->activeTab);

            // Trigger initial analysis if model is provided
            if (pData->activeTab == 0 && !pData->initialModel.empty()) {
                StartOllamaAnalysis(hwnd, pData);
            } else if (pData->activeTab == 1 && !pData->initialModel.empty()) {
                StartHuggingFaceAnalysis(hwnd, pData);
            }
            return 0;
        }

        case WM_SIZE: {
            LayoutModelControls(hwnd, pData);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_APP_OLLAMA_STATUS: {
            wchar_t* pMsg = (wchar_t*)lParam;
            if (pMsg) {
                if (pData && pData->hwndStatus) {
                    SetWindowTextW(pData->hwndStatus, pMsg);
                }
                free(pMsg);
            }
            return 0;
        }

        case WM_APP_OLLAMA_DONE: {
            OllamaFetchResult* pRes = (OllamaFetchResult*)lParam;
            if (pRes && pData) {
                pData->fetchResult = *pRes;
                delete pRes;
                pData->isAnalyzing = false;
                EnableWindow(pData->hwndBtnAnalyze, TRUE);

                if (pData->fetchResult.success) {
                    if (pData->activeTab == 0) EnableWindow(pData->hwndBtnOk, TRUE);
                    bool isTr = (I18n::Instance().GetCurrentLanguage() == LangId::Turkish);
                    std::wstring statusText = L"✔ " + pData->fetchResult.info.spec.displayName +
                        L"  (" + OllamaClient::FormatBytes(pData->fetchResult.info.totalSize) +
                        L", " + std::to_wstring(pData->fetchResult.info.layers.size()) + (isTr ? L" Katman)" : L" Layers)");
                    SetWindowTextW(pData->hwndStatus, statusText.c_str());

                    std::wstring details;
                    details += (isTr ? L"Model: " : L"Model: ") + pData->fetchResult.info.spec.fullName + L"\r\n";
                    details += (isTr ? L"Toplam Boyut: " : L"Total Size: ") + OllamaClient::FormatBytes(pData->fetchResult.info.totalSize) + L"\r\n";
                    details += (isTr ? L"Katman Sayısı: " : L"Layers Count: ") + std::to_wstring(pData->fetchResult.info.layers.size()) + L"\r\n";
                    if (pData->fetchResult.info.ggufLayerIndex >= 0) {
                        details += (isTr ? L"GGUF Katman Boyutu: " : L"GGUF Layer Size: ") + OllamaClient::FormatBytes(pData->fetchResult.info.ggufSize) + L"\r\n";
                    }
                    details += isTr ? L"\r\n--- KATMAN LİSTESİ ---\r\n" : L"\r\n--- LAYERS LIST ---\r\n";
                    for (size_t i = 0; i < pData->fetchResult.info.layers.size(); ++i) {
                        const auto& l = pData->fetchResult.info.layers[i];
                        details += std::to_wstring(i + 1) + L". " + (l.isModel ? L"[MODEL GGUF] " : (isTr ? L"[KATMAN] " : L"[LAYER] ")) +
                            l.targetFilename + L" (" + OllamaClient::FormatBytes(l.size) + L")\r\n";
                        details += L"    SHA256: " + OllamaClient::Utf8ToWide(l.digest) + L"\r\n";
                    }
                    SetWindowTextW(pData->hwndDetails, details.c_str());

                    if (pData->selectedFormat == OllamaDownloadFormat::Gguf) {
                        SetWindowTextW(pData->hwndFilename, pData->fetchResult.info.recommendedGgufFilename.c_str());
                    } else {
                        SetWindowTextW(pData->hwndFilename, pData->fetchResult.info.recommendedPackageDir.c_str());
                    }
                } else {
                    if (pData->activeTab == 0) EnableWindow(pData->hwndBtnOk, FALSE);
                    SetWindowTextW(pData->hwndStatus, (L"❌ " + pData->fetchResult.errorMsg).c_str());
                    SetWindowTextW(pData->hwndDetails, pData->fetchResult.errorMsg.c_str());
                }

                if (!s_dialogSnapshotPath.empty()) {
                    InvalidateRect(pData->hwndDetails, NULL, TRUE);
                    UpdateWindow(pData->hwndDetails);
                    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                    Sleep(100);
                    CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
            }
            return 0;
        }

        case WM_APP_HF_STATUS: {
            wchar_t* pMsg = (wchar_t*)lParam;
            if (pMsg) {
                if (pData && pData->hwndHfStatus) {
                    SetWindowTextW(pData->hwndHfStatus, pMsg);
                }
                free(pMsg);
            }
            return 0;
        }

        case WM_APP_HF_DONE: {
            HuggingFaceFetchResult* pRes = (HuggingFaceFetchResult*)lParam;
            if (pRes && pData) {
                pData->hfFetchResult = *pRes;
                delete pRes;
                pData->isHfAnalyzing = false;
                EnableWindow(pData->hwndBtnHfAnalyze, TRUE);

                if (pData->hfFetchResult.success && !pData->hfFetchResult.info.files.empty()) {
                    if (pData->activeTab == 1) EnableWindow(pData->hwndBtnOk, TRUE);
                    bool isTr = (I18n::Instance().GetCurrentLanguage() == LangId::Turkish);

                    SendMessageW(pData->hwndComboHfFiles, CB_RESETCONTENT, 0, 0);
                    for (size_t i = 0; i < pData->hfFetchResult.info.files.size(); ++i) {
                        const auto& f = pData->hfFetchResult.info.files[i];
                        std::wstring itemText = f.filename + L"  (" + HuggingFaceClient::FormatBytes(f.size) + L")";
                        if (f.isGguf) {
                            if (!f.quantType.empty()) itemText += L"  [" + f.quantType + L"]";
                            if (f.qualityStars > 0) itemText += L"  " + HuggingFaceClient::FormatStars(f.qualityStars);
                            if (f.isRecommended) itemText += isTr ? L"  ⭐ (Önerilen)" : L"  ⭐ (Recommended)";
                        } else if (f.isSafetensors) {
                            itemText += L"  [Safetensors]";
                        }
                        SendMessageW(pData->hwndComboHfFiles, CB_ADDSTRING, 0, (LPARAM)itemText.c_str());
                    }

                    int sel = pData->hfFetchResult.info.recommendedGgufIndex;
                    if (sel < 0 || sel >= (int)pData->hfFetchResult.info.files.size()) sel = 0;
                    SendMessageW(pData->hwndComboHfFiles, CB_SETCURSEL, sel, 0);
                    SetWindowTextW(pData->hwndHfFilename, pData->hfFetchResult.info.files[sel].filename.c_str());

                    std::wstring statusText = L"✔ " + pData->hfFetchResult.info.spec.displayName +
                        L"  (" + std::to_wstring(pData->hfFetchResult.info.files.size()) + (isTr ? L" Dosya, " : L" Files, ") +
                        HuggingFaceClient::FormatBytes(pData->hfFetchResult.info.totalSize) + L")";
                    SetWindowTextW(pData->hwndHfStatus, statusText.c_str());

                    std::wstring details;
                    details += (isTr ? L"Depo: " : L"Repository: ") + pData->hfFetchResult.info.spec.repoId + L"\r\n";
                    details += (isTr ? L"Dal / Revizyon: " : L"Branch / Revision: ") + pData->hfFetchResult.info.spec.revision + L"\r\n";
                    details += (isTr ? L"Toplam Boyut: " : L"Total Size: ") + HuggingFaceClient::FormatBytes(pData->hfFetchResult.info.totalSize) + L"\r\n";
                    details += (isTr ? L"Toplam Dosya: " : L"Total Files: ") + std::to_wstring(pData->hfFetchResult.info.files.size()) +
                        L" (GGUF: " + std::to_wstring(pData->hfFetchResult.info.totalGgufCount) +
                        L", Safetensors: " + std::to_wstring(pData->hfFetchResult.info.totalSafetensorsCount) + L")\r\n";

                    if (pData->hfFetchResult.info.hasGguf) {
                        details += isTr ? L"\r\n--- GGUF NİCELLEŞTİRME & KALİTE TABLOSU ---\r\n" : L"\r\n--- GGUF QUANTIZATION & QUALITY TABLE ---\r\n";
                        for (const auto& f : pData->hfFetchResult.info.files) {
                            if (f.isGguf) {
                                details += L"• " + f.filename + L"\r\n";
                                details += (isTr ? L"   Boyut: " : L"   Size: ") + HuggingFaceClient::FormatBytes(f.size) +
                                    (isTr ? L" | Kalite: " : L" | Quality: ") + HuggingFaceClient::FormatStars(f.qualityStars);
                                if (!f.description.empty()) details += L" (" + f.description + L")";
                                if (f.isRecommended) details += isTr ? L" [ÖNERİLEN]" : L" [RECOMMENDED]";
                                details += L"\r\n";
                            }
                        }
                    }

                    details += isTr ? L"\r\n--- TÜM DEPO DOSYALARI ---\r\n" : L"\r\n--- ALL REPOSITORY FILES ---\r\n";
                    for (size_t i = 0; i < pData->hfFetchResult.info.files.size(); ++i) {
                        const auto& f = pData->hfFetchResult.info.files[i];
                        details += std::to_wstring(i + 1) + L". " + f.filename + L" (" + HuggingFaceClient::FormatBytes(f.size) + L")\r\n";
                        if (!f.sha256.empty()) {
                            details += L"    SHA256: " + HuggingFaceClient::Utf8ToWide(f.sha256) + L"\r\n";
                        }
                    }
                    SetWindowTextW(pData->hwndHfDetails, details.c_str());
                } else {
                    if (pData->activeTab == 1) EnableWindow(pData->hwndBtnOk, FALSE);
                    SetWindowTextW(pData->hwndHfStatus, (L"❌ " + pData->hfFetchResult.errorMsg).c_str());
                    SetWindowTextW(pData->hwndHfDetails, pData->hfFetchResult.errorMsg.c_str());
                }

                if (!s_dialogSnapshotPath.empty()) {
                    InvalidateRect(pData->hwndHfDetails, NULL, TRUE);
                    UpdateWindow(pData->hwndHfDetails);
                    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                    Sleep(100);
                    CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pData) {
                if (pData->workerThread.joinable()) pData->workerThread.detach();
                if (pData->hfWorkerThread.joinable()) pData->hfWorkerThread.detach();
                if (pData->hFont) DeleteObject(pData->hFont);
                if (pData->hFontBold) DeleteObject(pData->hFontBold);
                if (pData->hFontMono) DeleteObject(pData->hFontMono);
            }
            return 0;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y < DlgScale(hwnd, 46) && pt.x < rc.right - DlgScale(hwnd, 50)) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (pData) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == ID_TAB_OLLAMA) {
                bool isSel = (pData && pData->activeTab == 0);
                Theme::DrawSegmentedTab(dis, isSel, LStr(StrId::DlgModelTabOllama));
                return TRUE;
            } else if (dis->CtlID == ID_TAB_HF) {
                bool isSel = (pData && pData->activeTab == 1);
                Theme::DrawSegmentedTab(dis, isSel, LStr(StrId::DlgModelTabHf));
                return TRUE;
            } else if (dis->CtlID == IDOK) {
                Theme::DrawModernButton(dis, true, L"⬇  " + std::wstring(LStr(StrId::DlgOllamaDownload)));
                return TRUE;
            } else if (dis->CtlID == IDCANCEL) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgClose));
                return TRUE;
            } else if (dis->CtlID == 302) { // Ollama Analyze
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgOllamaAnalyze));
                return TRUE;
            } else if (dis->CtlID == ID_HF_ANALYZE) { // HF Analyze
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgHfAnalyze));
                return TRUE;
            } else if (dis->CtlID == 310) { // Browse
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgNewBrowse));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            if (pData && hCtl == pData->hwndStatus) {
                if (pData->fetchResult.success) {
                    SetTextColor(hdc, Theme::AccentGreen);
                } else if (!pData->fetchResult.errorMsg.empty()) {
                    SetTextColor(hdc, Theme::AccentRed);
                } else {
                    SetTextColor(hdc, Theme::AccentCyan);
                }
            } else if (pData && hCtl == pData->hwndHfStatus) {
                if (pData->hfFetchResult.success) {
                    SetTextColor(hdc, Theme::AccentGreen);
                } else if (!pData->hfFetchResult.errorMsg.empty()) {
                    SetTextColor(hdc, Theme::AccentRed);
                } else {
                    SetTextColor(hdc, Theme::AccentCyan);
                }
            } else if (pData && (hCtl == pData->hwndDetails || hCtl == pData->hwndHfDetails)) {
                SetTextColor(hdc, Theme::TextPrimary);
                SetBkColor(hdc, Theme::BgInput);
                SetBkMode(hdc, OPAQUE);
                static HBRUSH hbrInput = CreateSolidBrush(Theme::BgInput);
                return (LRESULT)hbrInput;
            } else {
                SetTextColor(hdc, Theme::TextPrimary);
            }
            SetBkColor(hdc, Theme::BgCard);
            static HBRUSH hbrCard = CreateSolidBrush(Theme::BgCard);
            return (LRESULT)hbrCard;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrEdit = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrEdit;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);

            if (wmId == ID_TAB_OLLAMA) {
                SwitchModelDialogTab(hwnd, pData, 0);
            } else if (wmId == ID_TAB_HF) {
                SwitchModelDialogTab(hwnd, pData, 1);
            } else if (wmId == 301 && HIWORD(wParam) == EN_CHANGE) {
                if (pData && !pData->isAnalyzing.load() && pData->activeTab == 0) {
                    EnableWindow(pData->hwndBtnOk, FALSE);
                }
            } else if (wmId == ID_HF_REPO && HIWORD(wParam) == EN_CHANGE) {
                if (pData && !pData->isHfAnalyzing.load() && pData->activeTab == 1) {
                    EnableWindow(pData->hwndBtnOk, FALSE);
                }
            } else if (wmId == 302) { // Ollama Analyze
                StartOllamaAnalysis(hwnd, pData);
            } else if (wmId == ID_HF_ANALYZE) { // HF Analyze
                StartHuggingFaceAnalysis(hwnd, pData);
            } else if (wmId == ID_HF_COMBO_FILE && HIWORD(wParam) == CBN_SELCHANGE) {
                if (pData && pData->hfFetchResult.success) {
                    int curSel = (int)SendMessageW(pData->hwndComboHfFiles, CB_GETCURSEL, 0, 0);
                    if (curSel >= 0 && curSel < (int)pData->hfFetchResult.info.files.size()) {
                        SetWindowTextW(pData->hwndHfFilename, pData->hfFetchResult.info.files[curSel].filename.c_str());
                    }
                }
            } else if (wmId == 303 || wmId == 333) { // GGUF format selected
                if (pData) {
                    pData->selectedFormat = OllamaDownloadFormat::Gguf;
                    SendMessageW(pData->hwndRadioGguf, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessageW(pData->hwndRadioOriginal, BM_SETCHECK, BST_UNCHECKED, 0);
                    SetWindowTextW(pData->hwndLblFilename, (std::wstring(LStr(StrId::DlgOllamaFilename)) + L" (GGUF .gguf):").c_str());
                    if (pData->fetchResult.success) {
                        SetWindowTextW(pData->hwndFilename, pData->fetchResult.info.recommendedGgufFilename.c_str());
                    }
                }
            } else if (wmId == 304 || wmId == 334) { // Original Package selected
                if (pData) {
                    pData->selectedFormat = OllamaDownloadFormat::OriginalPackage;
                    SendMessageW(pData->hwndRadioGguf, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendMessageW(pData->hwndRadioOriginal, BM_SETCHECK, BST_CHECKED, 0);
                    SetWindowTextW(pData->hwndLblFilename, (std::wstring(LStr(StrId::DlgOllamaFilename)) + L" (Paket Klasörü):").c_str());
                    if (pData->fetchResult.success) {
                        SetWindowTextW(pData->hwndFilename, pData->fetchResult.info.recommendedPackageDir.c_str());
                    }
                }
            } else if (wmId == 310) { // Browse Save Folder
                BROWSEINFOW bi = { 0 };
                bi.hwndOwner = hwnd;
                bi.lpszTitle = L"Model için kayıt klasörünü seçin:";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    wchar_t path[MAX_PATH];
                    if (SHGetPathFromIDListW(pidl, path)) {
                        SetWindowTextW(pData->hwndSave, path);
                    }
                    CoTaskMemFree(pidl);
                }
            } else if (wmId == IDOK) {
                if (!pData) return 0;

                wchar_t sdirBuf[MAX_PATH];
                GetWindowTextW(pData->hwndSave, sdirBuf, _countof(sdirBuf));
                std::wstring saveDir = sdirBuf;
                if (saveDir.empty()) saveDir = Config::Instance().defaultSavePath;

                int parts = 10;
                int curSelParts = (int)SendMessageW(pData->hwndComboParts, CB_GETCURSEL, 0, 0);
                const int partOptions[] = { 1, 2, 4, 8, 10, 16, 24, 30 };
                if (curSelParts >= 0 && curSelParts < (int)_countof(partOptions)) {
                    parts = partOptions[curSelParts];
                }

                if (pData->activeTab == 0) {
                    // Ollama Download
                    if (!pData->fetchResult.success) return 0;

                    wchar_t fnBuf[MAX_PATH];
                    GetWindowTextW(pData->hwndFilename, fnBuf, _countof(fnBuf));
                    std::wstring targetName = WinHttpUtils::CleanFilename(fnBuf);

                    if (pData->selectedFormat == OllamaDownloadFormat::Gguf) {
                        if (pData->fetchResult.info.ggufLayerIndex >= 0 &&
                            pData->fetchResult.info.ggufLayerIndex < (int)pData->fetchResult.info.layers.size()) {
                            const auto& layer = pData->fetchResult.info.layers[pData->fetchResult.info.ggufLayerIndex];
                            DownloadManager::Instance().AddTask(
                                layer.downloadUrl,
                                saveDir,
                                LStr(StrId::CatAI),
                                parts,
                                true,
                                targetName
                            );
                        }
                    } else {
                        std::wstring packageDir = saveDir;
                        if (!packageDir.empty() && packageDir.back() != L'\\') packageDir += L'\\';
                        packageDir += targetName;
                        CreateDirectoryW(packageDir.c_str(), NULL);

                        std::wstring modelGgufName;
                        for (const auto& layer : pData->fetchResult.info.layers) {
                            DownloadManager::Instance().AddTask(
                                layer.downloadUrl,
                                packageDir,
                                LStr(StrId::CatAI),
                                parts,
                                true,
                                layer.targetFilename
                            );
                            if (layer.isModel) {
                                modelGgufName = layer.targetFilename;
                            }
                        }

                        if (!modelGgufName.empty()) {
                            std::wstring modelfilePath = packageDir + L"\\Modelfile";
                            HANDLE hModelfile = CreateFileW(modelfilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                            if (hModelfile != INVALID_HANDLE_VALUE) {
                                std::string mf = "# Modelfile created by Gety for " + OllamaClient::WideToUtf8(pData->fetchResult.info.spec.displayName) + "\n";
                                mf += "FROM ./" + OllamaClient::WideToUtf8(modelGgufName) + "\n";
                                DWORD written = 0;
                                WriteFile(hModelfile, mf.c_str(), (DWORD)mf.length(), &written, NULL);
                                CloseHandle(hModelfile);
                            }
                        }
                    }
                } else {
                    // Hugging Face Download
                    if (!pData->hfFetchResult.success || pData->hfFetchResult.info.files.empty()) return 0;

                    int curSel = (int)SendMessageW(pData->hwndComboHfFiles, CB_GETCURSEL, 0, 0);
                    if (curSel < 0 || curSel >= (int)pData->hfFetchResult.info.files.size()) return 0;
                    const auto& selFile = pData->hfFetchResult.info.files[curSel];

                    wchar_t fnBuf[MAX_PATH];
                    GetWindowTextW(pData->hwndHfFilename, fnBuf, _countof(fnBuf));
                    std::wstring targetName = WinHttpUtils::CleanFilename(fnBuf);
                    if (targetName.empty()) targetName = selFile.filename;

                    wchar_t tokBuf[1024] = { 0 };
                    GetWindowTextW(pData->hwndHfToken, tokBuf, _countof(tokBuf));
                    std::wstring token = WinHttpUtils::TrimUrl(tokBuf);
                    if (token != Config::Instance().huggingFaceToken) {
                        Config::Instance().huggingFaceToken = token;
                        Config::Instance().Save();
                    }

                    // Resolve download URL (following redirect or pre-authenticating CDN URL)
                    std::wstring directUrl = selFile.downloadUrl;
                    std::wstring resErr;
                    HuggingFaceClient::ResolveDownloadUrl(selFile.downloadUrl, token, directUrl, resErr);

                    DownloadManager::Instance().AddTask(
                        directUrl,
                        saveDir,
                        LStr(StrId::CatAI),
                        parts,
                        true,
                        targetName
                    );
                }

                DestroyWindow(hwnd);
            } else if (wmId == IDCANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Dialogs::ShowModelDownloaderDialog(HWND hParent, int initialTab, const std::wstring& initialModel) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ModelDownloaderDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyModelDownloaderDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    ModelDialogData data;
    data.activeTab = (initialTab == 1) ? 1 : 0;
    data.saveDir = Config::Instance().defaultSavePath;
    data.splitParts = Config::Instance().defaultSplitParts;

    if (!initialModel.empty()) {
        data.initialModel = initialModel;
        // Auto-switch tab if model looks like Hugging Face
        if (HuggingFaceClient::IsLikelyHuggingFaceModel(initialModel)) {
            data.activeTab = 1;
        }
    } else {
        std::wstring clip = ClipboardWatcher::GetClipboardText(hParent);
        if (HuggingFaceClient::IsLikelyHuggingFaceModel(clip)) {
            data.initialModel = WinHttpUtils::TrimUrl(clip);
            data.activeTab = 1;
        } else if (OllamaClient::IsLikelyOllamaModel(clip)) {
            data.initialModel = WinHttpUtils::TrimUrl(clip);
            data.activeTab = 0;
        }
    }

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 940);
    int dlgH = DlgScale(hParent, 680);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyModelDownloaderDlg",
        (data.activeTab == 0) ? LStr(StrId::DlgOllamaTitle) : LStr(StrId::DlgHfTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hDlg) {
        HWND hTargetEdit = (data.activeTab == 0) ? GetDlgItem(hDlg, 301) : GetDlgItem(hDlg, ID_HF_REPO);
        if (hTargetEdit) {
            SetFocus(hTargetEdit);
            SendMessageW(hTargetEdit, EM_SETSEL, 0, -1);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }
    return true;
}

bool Dialogs::ShowOllamaModelDialog(HWND hParent, const std::wstring& initialModel) {
    return ShowModelDownloaderDialog(hParent, 0, initialModel);
}

bool Dialogs::ShowHuggingFaceModelDialog(HWND hParent, const std::wstring& initialModel) {
    return ShowModelDownloaderDialog(hParent, 1, initialModel);
}

// -------------------------------------------------------------
// 3. Options Dialog (Ayarlar)
// -------------------------------------------------------------
struct OptionsDialogData {
    HWND hwndLblSaveDir = NULL;
    HWND hwndEditSaveDir = NULL;
    HWND hwndBtnBrowse = NULL;

    HWND hwndLblMaxTasks = NULL;
    HWND hwndEditMaxTasks = NULL;
    HWND hwndLblDefParts = NULL;
    HWND hwndEditDefParts = NULL;
    HWND hwndLblMaxRetries = NULL;
    HWND hwndEditMaxRetries = NULL;

    HWND hwndLblSpeed = NULL;
    HWND hwndEditSpeed = NULL;
    HWND hwndLblLang = NULL;
    HWND hwndComboLang = NULL;

    HWND hwndChks[6] = { NULL };
    HWND hwndLblChks[6] = { NULL };

    HWND hwndBtnCancel = NULL;
    HWND hwndBtnSave = NULL;

    HFONT hFont = NULL;
    HFONT hFontBold = NULL;

    CustomDialogHeader header;
};

static void LayoutOptionsControls(HWND hwnd, OptionsDialogData* pData) {
    if (!pData) return;
    RECT clR;
    GetClientRect(hwnd, &clR);
    int padX = DlgScale(hwnd, 30);
    int fieldW = clR.right - padX * 2;
    int inputH = DlgScale(hwnd, 28);
    int labelH = DlgScale(hwnd, 18);

    // 1. Save Folder + Browse
    int y0 = DlgScale(hwnd, 58);
    MoveWindow(pData->hwndLblSaveDir, padX, y0, fieldW, labelH, TRUE);
    int browseW = DlgScale(hwnd, 92);
    int dirW = fieldW - browseW - DlgScale(hwnd, 8);
    MoveWindow(pData->hwndEditSaveDir, padX, y0 + labelH + 2, dirW, inputH, TRUE);
    MoveWindow(pData->hwndBtnBrowse, padX + dirW + DlgScale(hwnd, 8), y0 + labelH + 2, browseW, inputH, TRUE);

    // 2. 3 Columns: Max Active, Default Split, Max Retries
    int y1 = y0 + inputH + labelH + DlgScale(hwnd, 10);
    int colGap = DlgScale(hwnd, 12);
    int col3W = (fieldW - colGap * 2) / 3;
    int col3_1X = padX;
    int col3_2X = padX + col3W + colGap;
    int col3_3X = padX + (col3W + colGap) * 2;

    MoveWindow(pData->hwndLblMaxTasks, col3_1X, y1, col3W, labelH, TRUE);
    MoveWindow(pData->hwndEditMaxTasks, col3_1X, y1 + labelH + 2, col3W, inputH, TRUE);

    MoveWindow(pData->hwndLblDefParts, col3_2X, y1, col3W, labelH, TRUE);
    MoveWindow(pData->hwndEditDefParts, col3_2X, y1 + labelH + 2, col3W, inputH, TRUE);

    MoveWindow(pData->hwndLblMaxRetries, col3_3X, y1, col3W, labelH, TRUE);
    MoveWindow(pData->hwndEditMaxRetries, col3_3X, y1 + labelH + 2, col3W, inputH, TRUE);

    // 3. 2 Columns: Speed Limit & Language
    int y2 = y1 + inputH + labelH + DlgScale(hwnd, 10);
    int col2W = (fieldW - colGap) / 2;
    int col2X = padX + col2W + colGap;

    MoveWindow(pData->hwndLblSpeed, padX, y2, col2W, labelH, TRUE);
    MoveWindow(pData->hwndEditSpeed, padX, y2 + labelH + 2, col2W, inputH, TRUE);

    MoveWindow(pData->hwndLblLang, col2X, y2, col2W, labelH, TRUE);
    MoveWindow(pData->hwndComboLang, col2X, y2 + labelH + 2, col2W, DlgScale(hwnd, 200), TRUE);

    // 4. Checkboxes
    int chkY = y2 + inputH + labelH + DlgScale(hwnd, 12);
    int chkStep = DlgScale(hwnd, 24);
    int chkBoxW = DlgScale(hwnd, 18);
    int chkBoxH = DlgScale(hwnd, 18);
    int lblX = padX + chkBoxW + DlgScale(hwnd, 8);
    int lblW = fieldW - (chkBoxW + DlgScale(hwnd, 8));

    for (int i = 0; i < 6; ++i) {
        int curY = chkY + chkStep * i;
        MoveWindow(pData->hwndChks[i], padX, curY + 1, chkBoxW, chkBoxH, TRUE);
        MoveWindow(pData->hwndLblChks[i], lblX, curY, lblW, labelH + 2, TRUE);
    }

    // Bottom Buttons
    int btnH = DlgScale(hwnd, 34);
    int yBtns = clR.bottom - DlgScale(hwnd, 46);
    int cancelW = DlgScale(hwnd, 105);
    int saveW = DlgScale(hwnd, 125);
    int saveX = clR.right - padX - saveW;
    int cancelX = saveX - cancelW - DlgScale(hwnd, 12);

    MoveWindow(pData->hwndBtnCancel, cancelX, yBtns, cancelW, btnH, TRUE);
    MoveWindow(pData->hwndBtnSave, saveX, yBtns, saveW, btnH, TRUE);
}

static LRESULT CALLBACK OptionsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OptionsDialogData* pData = (OptionsDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            pData = (OptionsDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            pData->header.title = LStr(StrId::DlgOptTitle);
            ApplyDialogDarkMode(hwnd);

            pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

            // 1. Default Save Folder + Browse
            pData->hwndLblSaveDir = CreateWindowW(L"STATIC", LStr(StrId::DlgOptSaveDir), WS_CHILD | WS_VISIBLE,
                                                  0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblSaveDir, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndEditSaveDir = CreateWindowExW(0, L"EDIT", Config::Instance().defaultSavePath.c_str(),
                                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                                     0, 0, 0, 0, hwnd, (HMENU)311, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndEditSaveDir, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndEditSaveDir, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnBrowse = CreateWindowW(L"BUTTON", LStr(StrId::DlgNewBrowse), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)312, GetModuleHandle(NULL), NULL);

            // 2. Max Active Tasks, Default Split Parts & Max Retries (Three Columns)
            pData->hwndLblMaxTasks = CreateWindowW(L"STATIC", LStr(StrId::DlgOptMaxTasks), WS_CHILD | WS_VISIBLE,
                                                   0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblMaxTasks, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndEditMaxTasks = CreateWindowExW(0, L"EDIT", std::to_wstring(Config::Instance().maxActiveDownloads).c_str(),
                                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                                                      0, 0, 0, 0, hwnd, (HMENU)301, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndEditMaxTasks, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndEditMaxTasks, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblDefParts = CreateWindowW(L"STATIC", LStr(StrId::DlgOptDefParts), WS_CHILD | WS_VISIBLE,
                                                   0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblDefParts, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndEditDefParts = CreateWindowExW(0, L"EDIT", std::to_wstring(Config::Instance().defaultSplitParts).c_str(),
                                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                                                      0, 0, 0, 0, hwnd, (HMENU)302, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndEditDefParts, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndEditDefParts, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblMaxRetries = CreateWindowW(L"STATIC", LStr(StrId::DlgOptMaxRetries), WS_CHILD | WS_VISIBLE,
                                                     0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblMaxRetries, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndEditMaxRetries = CreateWindowExW(0, L"EDIT", std::to_wstring(Config::Instance().maxRetryAttempts).c_str(),
                                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                                                         0, 0, 0, 0, hwnd, (HMENU)309, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndEditMaxRetries, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndEditMaxRetries, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            // 3. Manual Speed Limit & Language (Two Columns)
            pData->hwndLblSpeed = CreateWindowW(L"STATIC", LStr(StrId::DlgOptSpeedLimit), WS_CHILD | WS_VISIBLE,
                                                0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblSpeed, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndEditSpeed = CreateWindowExW(0, L"EDIT", std::to_wstring(Config::Instance().manualSpeedLimitKbps).c_str(),
                                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                                                   0, 0, 0, 0, hwnd, (HMENU)303, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndEditSpeed, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndEditSpeed, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndLblLang = CreateWindowW(L"STATIC", LStr(StrId::DlgOptLanguage), WS_CHILD | WS_VISIBLE,
                                               0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblLang, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndComboLang = CreateWindowExW(0, WC_COMBOBOXW, L"",
                                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                                   0, 0, 0, 0, hwnd, (HMENU)310, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndComboLang, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndComboLang, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            const auto& langs = I18n::Instance().GetLanguages();
            int curLangIdx = 0;
            for (size_t i = 0; i < langs.size(); ++i) {
                SendMessageW(pData->hwndComboLang, CB_ADDSTRING, 0, (LPARAM)langs[i].nativeName.c_str());
                if (langs[i].code == Config::Instance().language) {
                    curLangIdx = (int)i;
                }
            }
            SendMessageW(pData->hwndComboLang, CB_SETCURSEL, curLangIdx, 0);

            // 4. Checkboxes with crisp bright static labels
            struct OptChkItem {
                int id;
                int lblId;
                StrId strId;
                bool checked;
            };
            OptChkItem chkItems[] = {
                { 304, 324, StrId::DlgOptClip, Config::Instance().monitorClipboard },
                { 305, 325, StrId::DlgOptSound, Config::Instance().playSoundOnComplete },
                { 306, 326, StrId::DlgOptDrop, Config::Instance().showDropZone },
                { 308, 328, StrId::DlgOptMinToTray, Config::Instance().minimizeToTrayOnClose },
                { 307, 327, StrId::DlgOptShutdown, Config::Instance().autoShutdownOnComplete },
                { 313, 333, StrId::DlgOptAutoStart, Config::Instance().startWithWindows },
            };

            for (int i = 0; i < 6; ++i) {
                pData->hwndChks[i] = CreateWindowW(L"BUTTON", L"",
                                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                                    0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)chkItems[i].id, GetModuleHandle(NULL), NULL);
                SendMessageW(pData->hwndChks[i], BM_SETCHECK, chkItems[i].checked ? BST_CHECKED : BST_UNCHECKED, 0);

                pData->hwndLblChks[i] = CreateWindowW(L"STATIC", LStr(chkItems[i].strId),
                                                      WS_CHILD | WS_VISIBLE | SS_NOTIFY,
                                                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)chkItems[i].lblId, GetModuleHandle(NULL), NULL);
                SendMessageW(pData->hwndLblChks[i], WM_SETFONT, (WPARAM)pData->hFont, TRUE);
            }

            // Bottom Buttons
            pData->hwndBtnCancel = CreateWindowW(L"BUTTON", LStr(StrId::DlgCancel), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            pData->hwndBtnSave = CreateWindowW(L"BUTTON", LStr(StrId::DlgSave), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                               0, 0, 0, 0, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

            LayoutOptionsControls(hwnd, pData);

            if (!s_dialogSnapshotPath.empty()) {
                SetTimer(hwnd, 9999, 150, NULL);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (pData) {
                if (pData->hFont) DeleteObject(pData->hFont);
                if (pData->hFontBold) DeleteObject(pData->hFontBold);
                pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
                pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

                SendMessageW(pData->hwndLblSaveDir, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndEditSaveDir, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                SendMessageW(pData->hwndLblMaxTasks, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndEditMaxTasks, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblDefParts, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndEditDefParts, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblMaxRetries, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndEditMaxRetries, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                SendMessageW(pData->hwndLblSpeed, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndEditSpeed, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblLang, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndComboLang, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                for (int i = 0; i < 6; ++i) {
                    SendMessageW(pData->hwndLblChks[i], WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                }

                LayoutOptionsControls(hwnd, pData);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pData) {
                if (pData->hFont) { DeleteObject(pData->hFont); pData->hFont = NULL; }
                if (pData->hFontBold) { DeleteObject(pData->hFontBold); pData->hFontBold = NULL; }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 9999) {
                KillTimer(hwnd, 9999);
                CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                s_dialogSnapshotPath.clear();
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y <= DlgScale(hwnd, 42)) {
                if (pData && pData->header.HitTestClose(hwnd, pt.x, pt.y, rc.right)) {
                    return HTCLIENT;
                }
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                bool wasHov = pData->header.closeHovered;
                pData->header.closeHovered = pData->header.HitTestClose(hwnd, x, y, rc.right);
                if (wasHov != pData->header.closeHovered) {
                    RECT r = { rc.right - DlgScale(hwnd, 42), 0, rc.right, DlgScale(hwnd, 42) };
                    InvalidateRect(hwnd, &r, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == IDOK) {
                Theme::DrawModernButton(dis, true, LStr(StrId::DlgSave));
                return TRUE;
            } else if (dis->CtlID == IDCANCEL) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgCancel));
                return TRUE;
            } else if (dis->CtlID == 312) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgNewBrowse));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgCard);
            static HBRUSH hbrCard = CreateSolidBrush(Theme::BgCard);
            return (LRESULT)hbrCard;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrEdit = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrEdit;
        }

        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrList = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrList;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if ((wmId >= 324 && wmId <= 328) || wmId == 333) {
                int chkId = (wmId == 333) ? 313 : (wmId - 20);
                BOOL checked = (IsDlgButtonChecked(hwnd, chkId) == BST_CHECKED);
                CheckDlgButton(hwnd, chkId, checked ? BST_UNCHECKED : BST_CHECKED);
            } else if (wmId == 312) {
                wchar_t currentDir[MAX_PATH] = { 0 };
                GetDlgItemTextW(hwnd, 311, currentDir, MAX_PATH);
                std::wstring dir = BrowseForFolder(hwnd, currentDir);
                SetDlgItemTextW(hwnd, 311, dir.c_str());
            } else if (wmId == IDOK) {
                wchar_t buf[1024];
                GetDlgItemTextW(hwnd, 311, buf, 1024);
                Config::Instance().defaultSavePath = buf;

                int maxActive = GetDlgItemInt(hwnd, 301, NULL, FALSE);
                int defParts = GetDlgItemInt(hwnd, 302, NULL, FALSE);
                int speedLimit = GetDlgItemInt(hwnd, 303, NULL, FALSE);
                int maxRetries = GetDlgItemInt(hwnd, 309, NULL, FALSE);

                if (maxActive >= 1 && maxActive <= 10) Config::Instance().maxActiveDownloads = maxActive;
                if (defParts >= 1 && defParts <= 64) Config::Instance().defaultSplitParts = defParts;
                if (speedLimit > 0) Config::Instance().manualSpeedLimitKbps = speedLimit;
                if (maxRetries >= 1 && maxRetries <= 50) Config::Instance().maxRetryAttempts = maxRetries;

                int langSel = (int)SendDlgItemMessageW(hwnd, 310, CB_GETCURSEL, 0, 0);
                const auto& langs = I18n::Instance().GetLanguages();
                if (langSel >= 0 && langSel < (int)langs.size()) {
                    Config::Instance().language = langs[langSel].code;
                    I18n::Instance().SetLanguageByCode(Config::Instance().language);
                }

                Config::Instance().monitorClipboard = (IsDlgButtonChecked(hwnd, 304) == BST_CHECKED);
                Config::Instance().playSoundOnComplete = (IsDlgButtonChecked(hwnd, 305) == BST_CHECKED);
                Config::Instance().showDropZone = (IsDlgButtonChecked(hwnd, 306) == BST_CHECKED);
                Config::Instance().minimizeToTrayOnClose = (IsDlgButtonChecked(hwnd, 308) == BST_CHECKED);
                Config::Instance().autoShutdownOnComplete = (IsDlgButtonChecked(hwnd, 307) == BST_CHECKED);
                Config::Instance().startWithWindows = (IsDlgButtonChecked(hwnd, 313) == BST_CHECKED);

                Config::Instance().Save();

                ClipboardWatcher::Instance().SetEnabled(Config::Instance().monitorClipboard);
                DownloadManager::Instance().TickQueue();

                SpeedLimiter::Instance().SetLimit(
                    Config::Instance().speedMode,
                    Config::Instance().manualSpeedLimitKbps,
                    Config::Instance().backgroundSpeedLimitKbps
                );

                DestroyWindow(hwnd);
            } else if (wmId == IDCANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Dialogs::ShowOptionsDialog(HWND hParent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = OptionsDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyOptionsDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    OptionsDialogData data;

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 680);
    int dlgH = DlgScale(hParent, 485);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyOptionsDlg",
        LStr(StrId::DlgOptTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }
    return true;
}

// -------------------------------------------------------------
// 4. About Dialog (Gety Hakkında)
// -------------------------------------------------------------
struct AboutDialogData {
    HWND hwndBtnOk = NULL;
    CustomDialogHeader header;
};

static LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AboutDialogData* pData = (AboutDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            pData = (AboutDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            pData->header.title = LStr(StrId::DlgAboutTitle);
            ApplyDialogDarkMode(hwnd);

            RECT clR;
            GetClientRect(hwnd, &clR);
            int btnW = DlgScale(hwnd, 120);
            int btnH = DlgScale(hwnd, 34);
            int xBtn = (clR.right - btnW) / 2;
            int yBtn = clR.bottom - DlgScale(hwnd, 46);

            pData->hwndBtnOk = CreateWindowW(L"BUTTON", LStr(StrId::DlgOk), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                             xBtn, yBtn, btnW, btnH, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

            if (!s_dialogSnapshotPath.empty()) {
                SetTimer(hwnd, 9999, 150, NULL);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (pData && pData->hwndBtnOk) {
                RECT clR;
                GetClientRect(hwnd, &clR);
                int btnW = DlgScale(hwnd, 120);
                int btnH = DlgScale(hwnd, 34);
                int xBtn = (clR.right - btnW) / 2;
                int yBtn = clR.bottom - DlgScale(hwnd, 46);
                MoveWindow(pData->hwndBtnOk, xBtn, yBtn, btnW, btnH, TRUE);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 9999) {
                KillTimer(hwnd, 9999);
                CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                s_dialogSnapshotPath.clear();
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y <= DlgScale(hwnd, 42)) {
                if (pData && pData->header.HitTestClose(hwnd, pt.x, pt.y, rc.right)) {
                    return HTCLIENT;
                }
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                bool wasHov = pData->header.closeHovered;
                pData->header.closeHovered = pData->header.HitTestClose(hwnd, x, y, rc.right);
                if (wasHov != pData->header.closeHovered) {
                    RECT r = { rc.right - DlgScale(hwnd, 42), 0, rc.right, DlgScale(hwnd, 42) };
                    InvalidateRect(hwnd, &r, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }

            // Big Centered Logo
            SetBkMode(hdc, TRANSPARENT);
            HFONT hFontBig = Theme::CreateAppFont(hwnd, 15, FW_BOLD);
            HGDIOBJ oldFont = SelectObject(hdc, hFontBig);
            SetTextColor(hdc, Theme::AccentCyan);
            RECT logoR = { 0, DlgScale(hwnd, 52), rc.right, DlgScale(hwnd, 82) };
            DrawTextW(hdc, L"⚡ GETY", -1, &logoR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Subtitle
            HFONT hFontSub = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            SelectObject(hdc, hFontSub);
            SetTextColor(hdc, Theme::TextMuted);
            RECT subR = { 0, DlgScale(hwnd, 82), rc.right, DlgScale(hwnd, 104) };
            DrawTextW(hdc, L"Sürüm 1.0.1 (C++20 Native Ultra-Fast Edition)", -1, &subR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Content text inside card
            HFONT hFontBody = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            SelectObject(hdc, hFontBody);
            SetTextColor(hdc, Theme::TextPrimary);
            int padX = DlgScale(hwnd, 32);
            int padY = DlgScale(hwnd, 114);
            int cardB = rc.bottom - DlgScale(hwnd, 64);
            RECT textR = { padX, padY, rc.right - padX, cardB };
            std::wstring fullDesc = LStr(StrId::DlgAboutDesc);
            size_t p = fullDesc.find(L"\n\n");
            std::wstring desc = (p != std::wstring::npos) ? fullDesc.substr(p + 2) : fullDesc;
            DrawTextW(hdc, desc.c_str(), -1, &textR, DT_LEFT | DT_WORDBREAK);

            SelectObject(hdc, oldFont);
            DeleteObject(hFontBig);
            DeleteObject(hFontSub);
            DeleteObject(hFontBody);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == IDOK) {
                Theme::DrawModernButton(dis, true, LStr(StrId::DlgOk));
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Dialogs::ShowAboutDialog(HWND hParent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = AboutDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyAboutDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    AboutDialogData data;

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 540);
    int dlgH = DlgScale(hParent, 500);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyAboutDlg",
        LStr(StrId::DlgAboutTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }
}

// -------------------------------------------------------------
// 5. Update URL Dialog (Bağlantıyı Güncelle)
// -------------------------------------------------------------
struct UpdateUrlDialogData {
    std::wstring currentUrl;
    std::wstring newUrl;
    bool accepted = false;

    HWND hwndLblOld = NULL;
    HWND hwndOldUrl = NULL;
    HWND hwndLblNew = NULL;
    HWND hwndNewUrl = NULL;
    HWND hwndBtnCancel = NULL;
    HWND hwndBtnOk = NULL;

    HFONT hFont = NULL;
    HFONT hFontBold = NULL;

    CustomDialogHeader header;
};

static void LayoutUpdateUrlControls(HWND hwnd, UpdateUrlDialogData* pData) {
    if (!pData) return;
    RECT clR;
    GetClientRect(hwnd, &clR);
    int padX = DlgScale(hwnd, 30);
    int fieldW = clR.right - padX * 2;
    int inputH = DlgScale(hwnd, 28);
    int labelH = DlgScale(hwnd, 18);

    // 1. Old (Current) URL - Read Only
    int y0 = DlgScale(hwnd, 58);
    MoveWindow(pData->hwndLblOld, padX, y0, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndOldUrl, padX, y0 + labelH + 2, fieldW, inputH, TRUE);

    // 2. New URL - Editable
    int y1 = y0 + inputH + labelH + DlgScale(hwnd, 14);
    MoveWindow(pData->hwndLblNew, padX, y1, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndNewUrl, padX, y1 + labelH + 2, fieldW, inputH, TRUE);

    // 3. Bottom Action Buttons
    int btnH = DlgScale(hwnd, 34);
    int yBtns = clR.bottom - DlgScale(hwnd, 46);
    int cancelW = DlgScale(hwnd, 105);
    int okW = DlgScale(hwnd, 160);
    int okX = clR.right - padX - okW;
    int cancelX = okX - cancelW - DlgScale(hwnd, 12);

    MoveWindow(pData->hwndBtnCancel, cancelX, yBtns, cancelW, btnH, TRUE);
    MoveWindow(pData->hwndBtnOk, okX, yBtns, okW, btnH, TRUE);
}

static LRESULT CALLBACK UpdateUrlDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    UpdateUrlDialogData* pData = (UpdateUrlDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            pData = (UpdateUrlDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            pData->header.title = LStr(StrId::DlgUpdateUrlTitle);
            ApplyDialogDarkMode(hwnd);

            pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

            // 1. Old (Current) URL - Read Only
            pData->hwndLblOld = CreateWindowW(L"STATIC", LStr(StrId::DlgUpdateUrlOld), WS_CHILD | WS_VISIBLE,
                                              0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblOld, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndOldUrl = CreateWindowExW(0, L"EDIT", pData->currentUrl.c_str(),
                                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
                                                0, 0, 0, 0, hwnd, (HMENU)501, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndOldUrl, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndOldUrl, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            // 2. New URL - Editable
            pData->hwndLblNew = CreateWindowW(L"STATIC", LStr(StrId::DlgUpdateUrlNew), WS_CHILD | WS_VISIBLE,
                                              0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblNew, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndNewUrl = CreateWindowExW(0, L"EDIT", pData->newUrl.c_str(),
                                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                                0, 0, 0, 0, hwnd, (HMENU)502, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndNewUrl, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndNewUrl, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            // 3. Bottom Action Buttons
            pData->hwndBtnCancel = CreateWindowW(L"BUTTON", LStr(StrId::DlgCancel), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            pData->hwndBtnOk = CreateWindowW(L"BUTTON", LStr(StrId::DlgUpdateUrlBtn), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                             0, 0, 0, 0, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

            LayoutUpdateUrlControls(hwnd, pData);

            SetFocus(pData->hwndNewUrl);

            if (!s_dialogSnapshotPath.empty()) {
                SetTimer(hwnd, 9999, 150, NULL);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (pData) {
                if (pData->hFont) DeleteObject(pData->hFont);
                if (pData->hFontBold) DeleteObject(pData->hFontBold);
                pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
                pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

                SendMessageW(pData->hwndLblOld, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndOldUrl, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblNew, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndNewUrl, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                LayoutUpdateUrlControls(hwnd, pData);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pData) {
                if (pData->hFont) { DeleteObject(pData->hFont); pData->hFont = NULL; }
                if (pData->hFontBold) { DeleteObject(pData->hFontBold); pData->hFontBold = NULL; }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 9999) {
                KillTimer(hwnd, 9999);
                CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                s_dialogSnapshotPath.clear();
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y <= DlgScale(hwnd, 42)) {
                if (pData && pData->header.HitTestClose(hwnd, pt.x, pt.y, rc.right)) {
                    return HTCLIENT;
                }
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                bool wasHov = pData->header.closeHovered;
                pData->header.closeHovered = pData->header.HitTestClose(hwnd, x, y, rc.right);
                if (wasHov != pData->header.closeHovered) {
                    RECT r = { rc.right - DlgScale(hwnd, 42), 0, rc.right, DlgScale(hwnd, 42) };
                    InvalidateRect(hwnd, &r, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    pData->accepted = false;
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == IDOK) {
                Theme::DrawModernButton(dis, true, L"🔗  " + std::wstring(LStr(StrId::DlgUpdateUrlBtn)));
                return TRUE;
            } else if (dis->CtlID == IDCANCEL) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgCancel));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgCard);
            static HBRUSH hbrCard = CreateSolidBrush(Theme::BgCard);
            return (LRESULT)hbrCard;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrEdit = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrEdit;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDOK) {
                wchar_t buf[4096] = { 0 };
                GetWindowTextW(pData->hwndNewUrl, buf, _countof(buf));
                std::wstring trimmed = WinHttpUtils::TrimUrl(buf);
                if (trimmed.empty()) {
                    MessageBoxW(hwnd, L"Lütfen geçerli bir indirme bağlantısı girin.", LStr(StrId::DlgUpdateUrlTitle), MB_OK | MB_ICONWARNING);
                    return 0;
                }
                pData->newUrl = trimmed;
                pData->accepted = true;
                DestroyWindow(hwnd);
            } else if (wmId == IDCANCEL) {
                pData->accepted = false;
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            if (pData) pData->accepted = false;
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Dialogs::ShowUpdateUrlDialog(HWND hParent, const std::wstring& currentUrl, std::wstring& outNewUrl) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = UpdateUrlDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyUpdateUrlDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    UpdateUrlDialogData data;
    data.currentUrl = currentUrl;
    data.newUrl = currentUrl;

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 660);
    int dlgH = DlgScale(hParent, 270);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyUpdateUrlDlg",
        LStr(StrId::DlgUpdateUrlTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    if (data.accepted) {
        outNewUrl = data.newUrl;
        return true;
    }
    return false;
}

// -------------------------------------------------------------
// 6. Rename Dialog (Yeniden Adlandır)
// -------------------------------------------------------------
struct RenameDialogData {
    std::wstring currentFilename;
    std::wstring newFilename;
    bool accepted = false;

    HWND hwndLblPrompt = NULL;
    HWND hwndFilename = NULL;
    HWND hwndBtnCancel = NULL;
    HWND hwndBtnOk = NULL;

    HFONT hFont = NULL;
    HFONT hFontBold = NULL;

    CustomDialogHeader header;
};

static void LayoutRenameControls(HWND hwnd, RenameDialogData* pData) {
    if (!pData) return;
    RECT clR;
    GetClientRect(hwnd, &clR);
    int padX = DlgScale(hwnd, 30);
    int fieldW = clR.right - padX * 2;
    int inputH = DlgScale(hwnd, 28);
    int labelH = DlgScale(hwnd, 18);

    int y0 = DlgScale(hwnd, 64);
    MoveWindow(pData->hwndLblPrompt, padX, y0, fieldW, labelH, TRUE);
    MoveWindow(pData->hwndFilename, padX, y0 + labelH + DlgScale(hwnd, 6), fieldW, inputH, TRUE);

    int btnH = DlgScale(hwnd, 34);
    int yBtns = clR.bottom - DlgScale(hwnd, 46);
    int cancelW = DlgScale(hwnd, 105);
    int okW = DlgScale(hwnd, 130);
    int okX = clR.right - padX - okW;
    int cancelX = okX - cancelW - DlgScale(hwnd, 12);

    MoveWindow(pData->hwndBtnCancel, cancelX, yBtns, cancelW, btnH, TRUE);
    MoveWindow(pData->hwndBtnOk, okX, yBtns, okW, btnH, TRUE);
}

static LRESULT CALLBACK RenameDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RenameDialogData* pData = (RenameDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            pData = (RenameDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            pData->header.title = LStr(StrId::DlgRenameTitle);
            ApplyDialogDarkMode(hwnd);

            pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

            pData->hwndLblPrompt = CreateWindowW(L"STATIC", LStr(StrId::DlgRenamePrompt), WS_CHILD | WS_VISIBLE,
                                                 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblPrompt, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);

            pData->hwndFilename = CreateWindowExW(0, L"EDIT", pData->currentFilename.c_str(),
                                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                                  0, 0, 0, 0, hwnd, (HMENU)601, GetModuleHandle(NULL), NULL);
            SetWindowTheme(pData->hwndFilename, L"DarkMode_Explorer", NULL);
            SendMessageW(pData->hwndFilename, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnCancel = CreateWindowW(L"BUTTON", LStr(StrId::DlgCancel), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            pData->hwndBtnOk = CreateWindowW(L"BUTTON", LStr(StrId::DlgSave), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                             0, 0, 0, 0, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

            LayoutRenameControls(hwnd, pData);

            SetFocus(pData->hwndFilename);
            size_t lastDot = pData->currentFilename.rfind(L'.');
            if (lastDot != std::wstring::npos && lastDot > 0) {
                SendMessageW(pData->hwndFilename, EM_SETSEL, 0, (LPARAM)lastDot);
            } else {
                SendMessageW(pData->hwndFilename, EM_SETSEL, 0, -1);
            }

            if (!s_dialogSnapshotPath.empty()) {
                SetTimer(hwnd, 9999, 150, NULL);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (pData) {
                if (pData->hFont) DeleteObject(pData->hFont);
                if (pData->hFontBold) DeleteObject(pData->hFontBold);
                pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
                pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

                SendMessageW(pData->hwndLblPrompt, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
                SendMessageW(pData->hwndFilename, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                LayoutRenameControls(hwnd, pData);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pData) {
                if (pData->hFont) { DeleteObject(pData->hFont); pData->hFont = NULL; }
                if (pData->hFontBold) { DeleteObject(pData->hFontBold); pData->hFontBold = NULL; }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 9999) {
                KillTimer(hwnd, 9999);
                CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                s_dialogSnapshotPath.clear();
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y <= DlgScale(hwnd, 42)) {
                if (pData && pData->header.HitTestClose(hwnd, pt.x, pt.y, rc.right)) {
                    return HTCLIENT;
                }
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                bool wasHov = pData->header.closeHovered;
                pData->header.closeHovered = pData->header.HitTestClose(hwnd, x, y, rc.right);
                if (wasHov != pData->header.closeHovered) {
                    RECT r = { rc.right - DlgScale(hwnd, 42), 0, rc.right, DlgScale(hwnd, 42) };
                    InvalidateRect(hwnd, &r, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    pData->accepted = false;
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == IDOK) {
                Theme::DrawModernButton(dis, true, L"✏  " + std::wstring(LStr(StrId::DlgSave)));
                return TRUE;
            } else if (dis->CtlID == IDCANCEL) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgCancel));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgCard);
            static HBRUSH hbrCard = CreateSolidBrush(Theme::BgCard);
            return (LRESULT)hbrCard;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrEdit = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrEdit;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDOK) {
                wchar_t buf[1024] = { 0 };
                GetWindowTextW(pData->hwndFilename, buf, _countof(buf));
                std::wstring val = buf;
                size_t first = val.find_first_not_of(L" \t\r\n");
                size_t last = val.find_last_not_of(L" \t\r\n");
                if (first == std::wstring::npos || last == std::wstring::npos) {
                    MessageBoxW(hwnd, L"Lütfen geçerli bir dosya adı girin.", LStr(StrId::DlgRenameTitle), MB_OK | MB_ICONWARNING);
                    return 0;
                }
                std::wstring trimmed = val.substr(first, (last - first + 1));
                if (trimmed.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) {
                    MessageBoxW(hwnd, L"Dosya adı geçersiz karakterler içeremez (\\ / : * ? \" < > |).", LStr(StrId::DlgRenameTitle), MB_OK | MB_ICONWARNING);
                    return 0;
                }
                pData->newFilename = trimmed;
                pData->accepted = true;
                DestroyWindow(hwnd);
            } else if (wmId == IDCANCEL) {
                pData->accepted = false;
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            if (pData) pData->accepted = false;
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Dialogs::ShowRenameDialog(HWND hParent, const std::wstring& currentFilename, std::wstring& outNewFilename) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = RenameDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyRenameDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    RenameDialogData data;
    data.currentFilename = currentFilename;
    data.newFilename = currentFilename;

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 540);
    int dlgH = DlgScale(hParent, 220);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyRenameDlg",
        LStr(StrId::DlgRenameTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    if (data.accepted) {
        outNewFilename = data.newFilename;
        return true;
    }
    return false;
}

// -------------------------------------------------------------
// 7. Delete Confirmation Dialog (İndirmeyi Sil / Dosya Kalsın Seçeneği)
// -------------------------------------------------------------
struct DeleteConfirmDialogData {
    int taskCount = 1;
    bool defaultDeleteFile = false;
    bool deleteFile = false;
    bool accepted = false;

    HWND hwndLblPrompt = NULL;
    HWND hwndChkDeleteFile = NULL;
    HWND hwndLblDeleteFile = NULL;
    HWND hwndBtnCancel = NULL;
    HWND hwndBtnDelete = NULL;

    HFONT hFont = NULL;
    HFONT hFontBold = NULL;

    CustomDialogHeader header;
};

static void LayoutDeleteConfirmControls(HWND hwnd, DeleteConfirmDialogData* pData) {
    if (!pData) return;
    RECT clR;
    GetClientRect(hwnd, &clR);
    int padX = DlgScale(hwnd, 30);
    int fieldW = clR.right - padX * 2;

    int y0 = DlgScale(hwnd, 62);
    int promptH = DlgScale(hwnd, 38);
    MoveWindow(pData->hwndLblPrompt, padX, y0, fieldW, promptH, TRUE);

    int chkY = y0 + promptH + DlgScale(hwnd, 8);
    int chkW = DlgScale(hwnd, 20);
    int chkH = DlgScale(hwnd, 20);
    MoveWindow(pData->hwndChkDeleteFile, padX, chkY, chkW, chkH, TRUE);
    MoveWindow(pData->hwndLblDeleteFile, padX + chkW + DlgScale(hwnd, 8), chkY + 1, fieldW - chkW - DlgScale(hwnd, 8), chkH, TRUE);

    int btnH = DlgScale(hwnd, 34);
    int yBtns = clR.bottom - DlgScale(hwnd, 46);
    int cancelW = DlgScale(hwnd, 105);
    int delW = DlgScale(hwnd, 130);
    int delX = clR.right - padX - delW;
    int cancelX = delX - cancelW - DlgScale(hwnd, 12);

    MoveWindow(pData->hwndBtnCancel, cancelX, yBtns, cancelW, btnH, TRUE);
    MoveWindow(pData->hwndBtnDelete, delX, yBtns, delW, btnH, TRUE);
}

static LRESULT CALLBACK DeleteConfirmDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DeleteConfirmDialogData* pData = (DeleteConfirmDialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            pData = (DeleteConfirmDialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            pData->header.title = LStr(StrId::DlgDeleteTitle);
            ApplyDialogDarkMode(hwnd);

            pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
            pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

            std::wstring promptText;
            if (pData->taskCount <= 1) {
                promptText = LStr(StrId::DlgDeleteConfirmPrompt);
            } else {
                wchar_t pBuf[256];
                swprintf_s(pBuf, LStr(StrId::DlgDeleteMultiplePrompt), pData->taskCount);
                promptText = pBuf;
            }

            pData->hwndLblPrompt = CreateWindowW(L"STATIC", promptText.c_str(), WS_CHILD | WS_VISIBLE,
                                                 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblPrompt, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndChkDeleteFile = CreateWindowW(L"BUTTON", L"",
                                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                                     0, 0, 0, 0, hwnd, (HMENU)701, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndChkDeleteFile, BM_SETCHECK, pData->defaultDeleteFile ? BST_CHECKED : BST_UNCHECKED, 0);

            pData->hwndLblDeleteFile = CreateWindowW(L"STATIC", LStr(StrId::DlgDeleteKeepFileCheck),
                                                     WS_CHILD | WS_VISIBLE | SS_NOTIFY,
                                                     0, 0, 0, 0, hwnd, (HMENU)702, GetModuleHandle(NULL), NULL);
            SendMessageW(pData->hwndLblDeleteFile, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

            pData->hwndBtnCancel = CreateWindowW(L"BUTTON", LStr(StrId::DlgCancel), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            pData->hwndBtnDelete = CreateWindowW(L"BUTTON", LStr(StrId::MenuTaskDelete), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

            LayoutDeleteConfirmControls(hwnd, pData);

            SetFocus(pData->hwndBtnCancel);

            if (!s_dialogSnapshotPath.empty()) {
                SetTimer(hwnd, 9999, 150, NULL);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (pData) {
                if (pData->hFont) DeleteObject(pData->hFont);
                if (pData->hFontBold) DeleteObject(pData->hFontBold);
                pData->hFont = Theme::CreateAppFont(hwnd, 9, FW_NORMAL);
                pData->hFontBold = Theme::CreateAppFont(hwnd, 9, FW_SEMIBOLD);

                SendMessageW(pData->hwndLblPrompt, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
                SendMessageW(pData->hwndLblDeleteFile, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

                LayoutDeleteConfirmControls(hwnd, pData);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pData) {
                if (pData->hFont) { DeleteObject(pData->hFont); pData->hFont = NULL; }
                if (pData->hFontBold) { DeleteObject(pData->hFontBold); pData->hFontBold = NULL; }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 9999) {
                KillTimer(hwnd, 9999);
                CaptureHwndToPng(hwnd, s_dialogSnapshotPath);
                s_dialogSnapshotPath.clear();
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pt.y <= DlgScale(hwnd, 42)) {
                if (pData && pData->header.HitTestClose(hwnd, pt.x, pt.y, rc.right)) {
                    return HTCLIENT;
                }
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                bool wasHov = pData->header.closeHovered;
                pData->header.closeHovered = pData->header.HitTestClose(hwnd, x, y, rc.right);
                if (wasHov != pData->header.closeHovered) {
                    RECT r = { rc.right - DlgScale(hwnd, 42), 0, rc.right, DlgScale(hwnd, 42) };
                    InvalidateRect(hwnd, &r, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pData) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pData->header.HitTestClose(hwnd, x, y, rc.right)) {
                    pData->accepted = false;
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (pData) {
                pData->header.Draw(hwnd, hdc, rc.right, rc.bottom);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == IDOK) {
                Theme::DrawModernDangerButton(dis, L"🗑  " + std::wstring(LStr(StrId::MenuTaskDelete)));
                return TRUE;
            } else if (dis->CtlID == IDCANCEL) {
                Theme::DrawModernButton(dis, false, LStr(StrId::DlgCancel));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgCard);
            static HBRUSH hbrCard = CreateSolidBrush(Theme::BgCard);
            return (LRESULT)hbrCard;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == 702) {
                LRESULT chk = SendMessageW(pData->hwndChkDeleteFile, BM_GETCHECK, 0, 0);
                SendMessageW(pData->hwndChkDeleteFile, BM_SETCHECK, chk == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
            } else if (wmId == IDOK) {
                pData->deleteFile = (SendMessageW(pData->hwndChkDeleteFile, BM_GETCHECK, 0, 0) == BST_CHECKED);
                pData->accepted = true;
                DestroyWindow(hwnd);
            } else if (wmId == IDCANCEL) {
                pData->accepted = false;
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE:
            if (pData) pData->accepted = false;
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Dialogs::ShowDeleteConfirmDialog(HWND hParent, int taskCount, bool& outDeleteFile, bool defaultDeleteFile) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DeleteConfirmDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Theme::BgMain);
        wc.lpszClassName = L"GetyDeleteConfirmDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    DeleteConfirmDialogData data;
    data.taskCount = taskCount;
    data.defaultDeleteFile = defaultDeleteFile;
    data.deleteFile = defaultDeleteFile;

    RECT prc = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &prc);
    }
    if (prc.right <= prc.left || prc.bottom <= prc.top) {
        prc.left = 0;
        prc.top = 0;
        prc.right = GetSystemMetrics(SM_CXSCREEN);
        prc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int dlgW = DlgScale(hParent, 540);
    int dlgH = DlgScale(hParent, 230);
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"GetyDeleteConfirmDlg",
        LStr(StrId::DlgDeleteTitle),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        &data
    );

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, FALSE);
    }

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    if (data.accepted) {
        outDeleteFile = data.deleteFile;
        return true;
    }
    return false;
}

} // namespace Gety
