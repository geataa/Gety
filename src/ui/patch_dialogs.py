import sys

with open(r'E:\0_SkySoft\Gety\src\ui\Dialogs.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

idx_start = content.find('static LRESULT CALLBACK OptionsDlgProc')
if idx_start == -1:
    print('Failed to find OptionsDlgProc')
    sys.exit(1)

new_code = """static LRESULT CALLBACK OptionsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            ApplyDialogDarkMode(hwnd);
            HFONT hFont = GetDlgFont();
            HFONT hFontBold = GetDlgFontBold();

            HWND hLbl0 = CreateWindowW(L"STATIC", LStr(StrId::DlgOptSaveDir), WS_CHILD | WS_VISIBLE,
                                       20, 15, 300, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hLbl0, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hEditSaveDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                                Config::Instance().defaultSavePath.c_str(),
                                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                                20, 35, 300, 26, hwnd, (HMENU)311, GetModuleHandle(NULL), NULL);
            SetWindowTheme(hEditSaveDir, L"DarkMode_Explorer", NULL);
            SendMessageW(hEditSaveDir, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND btnBrowse = CreateWindowW(L"BUTTON", LStr(StrId::DlgNewBrowse), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                           330, 35, 80, 26, hwnd, (HMENU)312, GetModuleHandle(NULL), NULL);
            SendMessageW(btnBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLbl1 = CreateWindowW(L"STATIC", LStr(StrId::DlgOptMaxTasks), WS_CHILD | WS_VISIBLE,
                                       20, 70, 300, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hLbl1, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hEditMax = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                            std::to_wstring(Config::Instance().maxActiveDownloads).c_str(),
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                                            330, 67, 80, 26, hwnd, (HMENU)301, GetModuleHandle(NULL), NULL);
            SetWindowTheme(hEditMax, L"DarkMode_Explorer", NULL);
            SendMessageW(hEditMax, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLbl2 = CreateWindowW(L"STATIC", LStr(StrId::DlgOptDefParts), WS_CHILD | WS_VISIBLE,
                                       20, 103, 300, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hLbl2, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hEditParts = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                              std::to_wstring(Config::Instance().defaultSplitParts).c_str(),
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                                              330, 100, 80, 26, hwnd, (HMENU)302, GetModuleHandle(NULL), NULL);
            SetWindowTheme(hEditParts, L"DarkMode_Explorer", NULL);
            SendMessageW(hEditParts, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLbl3 = CreateWindowW(L"STATIC", LStr(StrId::DlgOptSpeedLimit), WS_CHILD | WS_VISIBLE,
                                       20, 136, 300, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hLbl3, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hEditSpeed = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                              std::to_wstring(Config::Instance().manualSpeedLimitKbps).c_str(),
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                                              330, 133, 80, 26, hwnd, (HMENU)303, GetModuleHandle(NULL), NULL);
            SetWindowTheme(hEditSpeed, L"DarkMode_Explorer", NULL);
            SendMessageW(hEditSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblLang = CreateWindowW(L"STATIC", LStr(StrId::DlgOptLanguage), WS_CHILD | WS_VISIBLE,
                                         20, 169, 200, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hLblLang, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hComboLang = CreateWindowExW(0, WC_COMBOBOXW, L"",
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                              230, 166, 180, 200, hwnd, (HMENU)310, GetModuleHandle(NULL), NULL);
            SetWindowTheme(hComboLang, L"DarkMode_Explorer", NULL);
            SendMessageW(hComboLang, WM_SETFONT, (WPARAM)hFont, TRUE);

            const auto& langs = I18n::Instance().GetLanguages();
            int curLangIdx = 0;
            for (size_t i = 0; i < langs.size(); ++i) {
                SendMessageW(hComboLang, CB_ADDSTRING, 0, (LPARAM)langs[i].nativeName.c_str());
                if (langs[i].code == Config::Instance().language) {
                    curLangIdx = (int)i;
                }
            }
            SendMessageW(hComboLang, CB_SETCURSEL, curLangIdx, 0);

            HWND chkClip = CreateWindowW(L"BUTTON", LStr(StrId::DlgOptClip),
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                         20, 205, 400, 24, hwnd, (HMENU)304, GetModuleHandle(NULL), NULL);
            SendMessageW(chkClip, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(chkClip, BM_SETCHECK, Config::Instance().monitorClipboard ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND chkSound = CreateWindowW(L"BUTTON", LStr(StrId::DlgOptSound),
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                          20, 233, 400, 24, hwnd, (HMENU)305, GetModuleHandle(NULL), NULL);
            SendMessageW(chkSound, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(chkSound, BM_SETCHECK, Config::Instance().playSoundOnComplete ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND chkDrop = CreateWindowW(L"BUTTON", LStr(StrId::DlgOptDrop),
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                         20, 261, 400, 24, hwnd, (HMENU)306, GetModuleHandle(NULL), NULL);
            SendMessageW(chkDrop, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(chkDrop, BM_SETCHECK, Config::Instance().showDropZone ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND chkMinToTray = CreateWindowW(L"BUTTON", LStr(StrId::DlgOptMinToTray),
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                              20, 289, 400, 24, hwnd, (HMENU)308, GetModuleHandle(NULL), NULL);
            SendMessageW(chkMinToTray, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(chkMinToTray, BM_SETCHECK, Config::Instance().minimizeToTrayOnClose ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND chkShut = CreateWindowW(L"BUTTON", LStr(StrId::DlgOptShutdown),
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                         20, 317, 400, 24, hwnd, (HMENU)307, GetModuleHandle(NULL), NULL);
            SendMessageW(chkShut, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(chkShut, BM_SETCHECK, Config::Instance().autoShutdownOnComplete ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND btnSave = CreateWindowW(L"BUTTON", LStr(StrId::DlgSave), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                         210, 360, 100, 30, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);
            SendMessageW(btnSave, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND btnCancel = CreateWindowW(L"BUTTON", LStr(StrId::DlgCancel), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                           320, 360, 90, 30, hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);
            SendMessageW(btnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

            return 0;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgMain);
            static HBRUSH hbrStatic = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrStatic;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgInput);
            static HBRUSH hbrEdit = CreateSolidBrush(Theme::BgInput);
            return (LRESULT)hbrEdit;
        }

        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgMain);
            static HBRUSH hbrBtn = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrBtn;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == 312) {
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

                if (maxActive >= 1 && maxActive <= 10) Config::Instance().maxActiveDownloads = maxActive;
                if (defParts >= 1 && defParts <= 30) Config::Instance().defaultSplitParts = defParts;
                if (speedLimit > 0) Config::Instance().manualSpeedLimitKbps = speedLimit;

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

                Config::Instance().Save();
                
                // Set speed limiter
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

    RECT prc;
    GetWindowRect(hParent, &prc);
    int dlgW = 450;
    int dlgH = 445;
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"GetyOptionsDlg",
        LStr(StrId::DlgOptTitle),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    EnableWindow(hParent, FALSE);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
    return true;
}

static LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            ApplyDialogDarkMode(hwnd);
            HFONT hFont = GetDlgFont();
            HFONT hFontBold = GetDlgFontBold();

            HWND hLblTitle = CreateWindowW(L"STATIC", L"⚡ GETY", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                           10, 20, 380, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hLblTitle, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hLblDesc = CreateWindowW(L"STATIC", LStr(StrId::DlgAboutDesc), WS_CHILD | WS_VISIBLE | SS_CENTER,
                                          10, 60, 380, 200, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hLblDesc, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND btnOk = CreateWindowW(L"BUTTON", LStr(StrId::DlgOk), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                       160, 270, 100, 30, hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);
            SendMessageW(btnOk, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            return 0;
        }

        case WM_CTLCOLORDLG: {
            static HBRUSH hbrDlg = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrDlg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hStatic = (HWND)lParam;
            wchar_t buf[64] = {0};
            GetWindowTextW(hStatic, buf, 64);
            if (wcscmp(buf, L"⚡ GETY") == 0) {
                SetTextColor(hdc, Theme::Cyan);
            } else {
                SetTextColor(hdc, Theme::TextPrimary);
            }
            SetBkColor(hdc, Theme::BgMain);
            static HBRUSH hbrStatic = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrStatic;
        }

        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::TextPrimary);
            SetBkColor(hdc, Theme::BgMain);
            static HBRUSH hbrBtn = CreateSolidBrush(Theme::BgMain);
            return (LRESULT)hbrBtn;
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

    RECT prc;
    GetWindowRect(hParent, &prc);
    int dlgW = 420;
    int dlgH = 350;
    int x = prc.left + ((prc.right - prc.left) - dlgW) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"GetyAboutDlg",
        LStr(StrId::DlgAboutTitle),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH,
        hParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    EnableWindow(hParent, FALSE);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
}
"""

content = content[:idx_start] + new_code + '\n} // namespace Gety\n'

with open(r'E:\0_SkySoft\Gety\src\ui\Dialogs.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('Dialogs.cpp patched')
