#include "MainWindow.h"
#include "Dialogs.h"
#include "../core/Config.h"
#include "../core/I18n.h"
#include "../core/ClipboardWatcher.h"
#include "../core/SpeedLimiter.h"
#include "../engine/DownloadManager.h"
#include "../engine/WinHttpUtils.h"
#include "../engine/OllamaClient.h"
#include "../engine/HuggingFaceClient.h"
#include "../../resources/resource.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <algorithm>

namespace Gety {

static const wchar_t* MAIN_WINDOW_CLASS = L"GetyMainWindow";

MainWindow& MainWindow::Instance() {
    static MainWindow instance;
    return instance;
}

MainWindow::MainWindow() {
}

MainWindow::~MainWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_WINDOW_CLASS;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; // Direct2D paints everything
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    int winX = Config::Instance().windowX;
    int winY = Config::Instance().windowY;
    int winW = Config::Instance().windowW;
    int winH = Config::Instance().windowH;
    m_splitterX = Config::Instance().leftPaneWidth;
    m_splitterY = Config::Instance().bottomPaneHeight;

    if (winW < 600) winW = 1000;
    if (winH < 400) winH = 680;

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        MAIN_WINDOW_CLASS,
        L"Gety",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
        winX, winY, winW, winH,
        NULL,
        NULL,
        hInstance,
        this
    );

    if (!m_hwnd) return false;

    // Windows 10/11 DWM Immersive Dark Mode
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(m_hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(m_hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_OLD */, &darkMode, sizeof(darkMode));

    // Extend frame into client area for native aero drop shadow
    MARGINS margins = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    // Initialize Direct2D renderer
    if (!m_renderer.Initialize(m_hwnd)) {
        return false;
    }

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    // Initialize tray icon
    m_trayIcon.Create(m_hwnd, WM_APP_TRAYMSG, LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON)), L"Gety");

    // Initialize Drop Zone floating window
    m_dropZone.Create(m_hwnd, Config::Instance().dropZoneX, Config::Instance().dropZoneY, Config::Instance().dropZoneOpacity);
    if (Config::Instance().showDropZone) {
        m_dropZone.Show(true);
    }
    UpdateDropZoneStats();

    // Initialize Clipboard Watcher
    ClipboardWatcher::Instance().SetEnabled(Config::Instance().monitorClipboard);
    ClipboardWatcher::Instance().Init(m_hwnd);

    // Initialize Download Manager engine
    DownloadManager::Instance().Init([this]() {
        if (m_hwnd) {
            PostMessageW(m_hwnd, WM_APP_TASK_UPDATE, 0, 0);
        }
    });

    SetSpeedMode(Config::Instance().speedMode);

    return true;
}

void MainWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        // -------------------------------------------------------------
        // Frameless DWM Window Handling
        // -------------------------------------------------------------
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) {
                // Return 0 so client area fills entire window frame (no native caption/borders)
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int border = 8;
            bool isMaximized = IsZoomed(hwnd);

            // Resize borders
            if (!isMaximized) {
                if (pt.y < border && pt.x < border) return HTTOPLEFT;
                if (pt.y < border && pt.x >= rc.right - border) return HTTOPRIGHT;
                if (pt.y >= rc.bottom - border && pt.x < border) return HTBOTTOMLEFT;
                if (pt.y >= rc.bottom - border && pt.x >= rc.right - border) return HTBOTTOMRIGHT;
                if (pt.y < border) return HTTOP;
                if (pt.y >= rc.bottom - border) return HTBOTTOM;
                if (pt.x < border) return HTLEFT;
                if (pt.x >= rc.right - border) return HTRIGHT;
            }

            // Buttons (minimize, maximize, close, action pill) MUST ALWAYS return HTCLIENT
            // so they receive WM_MOUSEMOVE, hover states, and WM_LBUTTONDOWN clicks!
            int dummyId = 0;
            if (m_renderer.IsOverTopRightButtons(pt.x, pt.y, dummyId) ||
                m_renderer.IsOverActionBar(pt.x, pt.y, dummyId)) {
                return HTCLIENT;
            }

            // Blank space in the top header is draggable
            if (m_renderer.IsInDraggableHeader(pt.x, pt.y)) {
                return HTCAPTION;
            }

            return HTCLIENT;
        }

        case WM_NCMOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            if (pt.y <= static_cast<int>(50 * m_renderer.GetDpiScale())) {
                if (!m_renderer.IsMouseNearTop()) {
                    m_renderer.SetMouseNearTop(true);
                    Invalidate();
                }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_NCLBUTTONDBLCLK: {
            if (wParam == HTCAPTION) {
                ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            Render();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent flicker

        case WM_DPICHANGED: {
            RECT* prc = (RECT*)lParam;
            if (prc) {
                SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            UINT newDpi = LOWORD(wParam);
            m_renderer.OnDpiChanged(newDpi);
            Invalidate();
            return 0;
        }

        case WM_SIZE: {
            UINT w = LOWORD(lParam);
            UINT h = HIWORD(lParam);
            if (w > 0 && h > 0) {
                m_renderer.OnResize(w, h);
                Invalidate();
            }
            return 0;
        }

        case WM_MOVE: {
            if (!IsIconic(hwnd) && !IsZoomed(hwnd)) {
                RECT r;
                GetWindowRect(hwnd, &r);
                Config::Instance().windowX = r.left;
                Config::Instance().windowY = r.top;
                Config::Instance().windowW = r.right - r.left;
                Config::Instance().windowH = r.bottom - r.top;
            }
            return 0;
        }

        // -------------------------------------------------------------
        // Mouse Interactions
        // -------------------------------------------------------------
        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);

                if (m_draggingSplitterV || m_isHoveringSplitterV) {
                    SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                    return TRUE;
                }
                if (m_draggingSplitterH || m_isHoveringSplitterH) {
                    SetCursor(LoadCursor(NULL, IDC_SIZENS));
                    return TRUE;
                }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            TRACKMOUSEEVENT tme = { sizeof(tme) };
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            if (m_draggingSplitterV) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int maxSplitX = static_cast<int>(rc.right) - 260;
                m_splitterX = (std::max)(140, (std::min)(maxSplitX, x));
                Invalidate();
                return 0;
            }

            if (m_draggingSplitterH) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int maxSplitY = static_cast<int>(rc.bottom) - 130;
                m_splitterY = (std::max)(140, (std::min)(maxSplitY, y));
                Invalidate();
                return 0;
            }

            m_isHoveringSplitterV = m_renderer.IsOverSplitterV(x, y);
            m_isHoveringSplitterH = m_renderer.IsOverSplitterH(x, y);

            m_renderer.OnMouseMove(x, y);
            Invalidate();
            return 0;
        }

        case WM_MOUSELEAVE: {
            m_renderer.SetMouseNearTop(false);
            m_renderer.ResetHoverStates();
            Invalidate();
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Splitter drag start
            if (m_renderer.IsOverSplitterV(x, y)) {
                m_draggingSplitterV = true;
                SetCapture(hwnd);
                return 0;
            }
            if (m_renderer.IsOverSplitterH(x, y)) {
                m_draggingSplitterH = true;
                SetCapture(hwnd);
                return 0;
            }

            // Top-Right Window Buttons
            int trBtnId = 0;
            if (m_renderer.IsOverTopRightButtons(x, y, trBtnId)) {
                if (trBtnId == Direct2DRenderer::BTN_CLOSE) {
                    // Close minimizes to tray per user requirement!
                    ShowWindow(m_hwnd, SW_HIDE);
                    if (!m_shownBalloonOnce) {
                        m_trayIcon.ShowBalloon(LStr(StrId::TrayBalloonTitle), LStr(StrId::TrayBalloonText));
                        m_shownBalloonOnce = true;
                    }
                    return 0;
                } else if (trBtnId == Direct2DRenderer::BTN_MAXIMIZE) {
                    ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                    return 0;
                } else if (trBtnId == Direct2DRenderer::BTN_MINIMIZE) {
                    ShowWindow(m_hwnd, SW_MINIMIZE);
                    return 0;
                }
            }

            // Action Pill Buttons
            int actId = 0;
            if (m_renderer.IsOverActionBar(x, y, actId)) {
                POINT spt = { x, y };
                ClientToScreen(hwnd, &spt);

                switch (actId) {
                    case Direct2DRenderer::BTN_NEW:
                        HandleCommand(ID_FILE_NEW);
                        break;
                    case Direct2DRenderer::BTN_BATCH:
                        HandleCommand(ID_FILE_BATCH_NEW);
                        break;
                    case Direct2DRenderer::BTN_VIDEO_LINK:
                        HandleCommand(ID_FILE_VIDEO_LINK);
                        break;
                    case Direct2DRenderer::BTN_OLLAMA_MODEL:
                        HandleCommand(ID_FILE_OLLAMA_MODEL);
                        break;
                    case Direct2DRenderer::BTN_HF_MODEL:
                        HandleCommand(ID_FILE_HF_MODEL);
                        break;
                    case Direct2DRenderer::BTN_START:
                        HandleCommand(ID_DOWNLOAD_START);
                        break;
                    case Direct2DRenderer::BTN_PAUSE:
                        HandleCommand(ID_DOWNLOAD_PAUSE);
                        break;
                    case Direct2DRenderer::BTN_DELETE:
                        HandleCommand(ID_DOWNLOAD_DELETE);
                        break;
                    case Direct2DRenderer::BTN_SPEED_MODE:
                        ShowSpeedModeMenu(spt.x, spt.y);
                        break;
                    case Direct2DRenderer::BTN_LANGUAGE:
                        ShowLanguageMenu(spt.x, spt.y);
                        break;
                    case Direct2DRenderer::BTN_SETTINGS:
                        HandleCommand(ID_TOOLS_OPTIONS);
                        break;
                    case Direct2DRenderer::BTN_ABOUT:
                        HandleCommand(ID_HELP_ABOUT);
                        break;
                }
                return 0;
            }

            // Category Selection
            CategoryFilterType clickedCat;
            if (m_renderer.IsOverCategories(x, y, clickedCat)) {
                m_currentCategory = clickedCat;
                m_selectedTaskIds.clear();
                m_selectedTaskId.clear();
                Invalidate();
                return 0;
            }

            // Task List Selection
            int rowIdx = -1;
            std::wstring taskId;
            if (m_renderer.IsOverTaskList(x, y, rowIdx, taskId)) {
                bool isCtrl = (wParam & MK_CONTROL) != 0;
                bool isShift = (wParam & MK_SHIFT) != 0;
                const auto& vis = m_renderer.GetVisibleTaskIds();

                if (isCtrl) {
                    if (m_selectedTaskIds.count(taskId)) {
                        m_selectedTaskIds.erase(taskId);
                        if (m_selectedTaskId == taskId) {
                            m_selectedTaskId = m_selectedTaskIds.empty() ? L"" : *m_selectedTaskIds.begin();
                        }
                    } else {
                        m_selectedTaskIds.insert(taskId);
                        m_selectedTaskId = taskId;
                    }
                } else if (isShift && !m_selectedTaskId.empty() && !vis.empty()) {
                    int prevIdx = -1;
                    int curIdx = -1;
                    for (int i = 0; i < (int)vis.size(); ++i) {
                        if (vis[i] == m_selectedTaskId) prevIdx = i;
                        if (vis[i] == taskId) curIdx = i;
                    }
                    if (prevIdx != -1 && curIdx != -1) {
                        int start = (std::min)(prevIdx, curIdx);
                        int end = (std::max)(prevIdx, curIdx);
                        m_selectedTaskIds.clear();
                        for (int i = start; i <= end; ++i) {
                            m_selectedTaskIds.insert(vis[i]);
                        }
                    } else {
                        m_selectedTaskIds.clear();
                        m_selectedTaskIds.insert(taskId);
                    }
                    m_selectedTaskId = taskId;
                } else {
                    m_selectedTaskIds.clear();
                    m_selectedTaskIds.insert(taskId);
                    m_selectedTaskId = taskId;
                }
                Invalidate();
                return 0;
            }

            // Detail Tabs
            DetailTab clickedTab;
            if (m_renderer.IsOverDetailTabs(x, y, clickedTab)) {
                m_activeTab = clickedTab;
                Invalidate();
                return 0;
            }

            m_renderer.OnLButtonDown(x, y);
            return 0;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (m_draggingSplitterV || m_draggingSplitterH) {
                m_draggingSplitterV = false;
                m_draggingSplitterH = false;
                ReleaseCapture();

                Config::Instance().leftPaneWidth = m_splitterX;
                Config::Instance().bottomPaneHeight = m_splitterY;
                Config::Instance().Save();
            }

            m_renderer.OnLButtonUp(x, y);
            Invalidate();
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            m_renderer.OnMouseWheel(pt.x, pt.y, delta);
            Invalidate();
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Double click task row -> start/pause or open file
            int rowIdx = -1;
            std::wstring taskId;
            if (m_renderer.IsOverTaskList(x, y, rowIdx, taskId)) {
                DownloadTaskInfo info;
                if (DownloadManager::Instance().GetSnapshot(taskId, info)) {
                    if (info.state == DownloadState::Completed) {
                        ShellExecuteW(NULL, L"open", info.fullPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    } else if (info.state == DownloadState::Downloading) {
                        DownloadManager::Instance().PauseTask(taskId);
                    } else {
                        DownloadManager::Instance().StartTask(taskId);
                    }
                }
                return 0;
            }

            // Double click header -> toggle maximize
            if (y < 48) {
                ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                return 0;
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            int rowIdx = -1;
            std::wstring taskId;
            if (m_renderer.IsOverTaskList(x, y, rowIdx, taskId)) {
                if (m_selectedTaskIds.find(taskId) == m_selectedTaskIds.end()) {
                    m_selectedTaskIds.clear();
                    m_selectedTaskIds.insert(taskId);
                    m_selectedTaskId = taskId;
                    Invalidate();
                }

                POINT spt = { x, y };
                ClientToScreen(hwnd, &spt);
                ShowTaskContextMenu(spt.x, spt.y, taskId);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (wParam == VK_DELETE) {
                HandleCommand(ID_DOWNLOAD_DELETE);
                return 0;
            } else if (wParam == 'A' && isCtrl) {
                const auto& vis = m_renderer.GetVisibleTaskIds();
                m_selectedTaskIds.clear();
                for (const auto& id : vis) {
                    m_selectedTaskIds.insert(id);
                }
                if (!m_selectedTaskIds.empty()) {
                    m_selectedTaskId = vis.front();
                }
                Invalidate();
                return 0;
            } else if ((wParam == 'N' && isCtrl) || wParam == VK_INSERT) {
                HandleCommand(ID_FILE_NEW);
                return 0;
            }
            break;
        }



        case WM_COMMAND: {
            int id = LOWORD(wParam);
            HandleCommand(id);
            return 0;
        }

        case WM_APP_CLIPBOARD_URL: {
            wchar_t* pUrl = (wchar_t*)lParam;
            if (pUrl) {
                std::wstring url = pUrl;
                free(pUrl);
                std::wstring filename, saveDir, category;
                int splitParts = Config::Instance().defaultSplitParts;
                bool startImmediately = true;
                if (Dialogs::ShowNewDownloadDialog(m_hwnd, url, url, filename, saveDir, category, splitParts, startImmediately)) {
                    m_selectedTaskId = DownloadManager::Instance().AddTask(url, saveDir, category, splitParts, startImmediately, filename);
                    Invalidate();
                }
            }
            return 0;
        }

        case WM_APP_DROPZONE_DROP: {
            wchar_t* pUrl = (wchar_t*)lParam;
            if (pUrl) {
                std::wstring url = pUrl;
                free(pUrl);
                std::wstring filename, saveDir, category;
                int splitParts = Config::Instance().defaultSplitParts;
                bool startImmediately = true;
                if (Dialogs::ShowNewDownloadDialog(m_hwnd, url, url, filename, saveDir, category, splitParts, startImmediately)) {
                    m_selectedTaskId = DownloadManager::Instance().AddTask(url, saveDir, category, splitParts, startImmediately, filename);
                    Invalidate();
                }
            }
            return 0;
        }

        case WM_APP_TASK_UPDATE: {
            UpdateDropZoneStats();
            Invalidate();
            CheckAutoShutdown();
            return 0;
        }

        case WM_APP_TRAYMSG: {
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(m_hwnd, SW_RESTORE);
                SetForegroundWindow(m_hwnd);
            } else if (lParam == WM_RBUTTONUP) {
                ShowTrayContextMenu();
            }
            return 0;
        }

        case WM_CLOSE: {
            if (Config::Instance().minimizeToTrayOnClose && !m_forceExit) {
                ShowWindow(m_hwnd, SW_HIDE);
                if (!m_shownBalloonOnce) {
                    m_trayIcon.ShowBalloon(LStr(StrId::TrayBalloonTitle), LStr(StrId::TrayBalloonText));
                    m_shownBalloonOnce = true;
                }
                return 0;
            }
            m_forceExit = true;
            DestroyWindow(m_hwnd);
            return 0;
        }

        case WM_DESTROY: {
            m_trayIcon.Remove();
            DownloadManager::Instance().Shutdown();
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void MainWindow::Render() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    m_renderer.BeginDraw();

    auto snapshots = DownloadManager::Instance().GetAllSnapshots();
    double totalSpeed = DownloadManager::Instance().GetTotalSpeedBps();
    int activeCount = DownloadManager::Instance().GetActiveCount();

    uint64_t totalDownloaded = 0;
    uint64_t totalBytes = 0;
    for (const auto& t : snapshots) {
        totalDownloaded += t.downloadedBytes;
        totalBytes += t.totalBytes;
    }

    m_renderer.RenderAll(
        snapshots,
        m_selectedTaskIds,
        m_selectedTaskId,
        m_currentCategory,
        m_activeTab,
        m_splitterX,
        m_splitterY,
        m_isHoveringSplitterV,
        m_isHoveringSplitterH,
        totalSpeed,
        activeCount,
        totalDownloaded,
        totalBytes
    );

    if (m_dropZone.IsVisible()) {
        double prog = (totalBytes > 0) ? (double)totalDownloaded / (double)totalBytes : 0.0;
        m_dropZone.UpdateStats(totalSpeed, activeCount, prog);
    }

    m_renderer.EndDraw();
}

void MainWindow::Invalidate() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::HandleCommand(int id) {
    switch (id) {
        case ID_FILE_NEW: {
            std::wstring url, filename, saveDir, category;
            int splitParts = Config::Instance().defaultSplitParts;
            bool startImmediately = true;

            std::wstring clipText = ClipboardWatcher::GetClipboardText(m_hwnd);
            std::wstring initialUrl = WinHttpUtils::ExtractUrlFromText(clipText);
            if (initialUrl.empty()) {
                std::wstring trimmed = WinHttpUtils::TrimUrl(clipText);
                if (!trimmed.empty() && trimmed.find_first_of(L"\r\n") == std::wstring::npos && trimmed.length() < 2048) {
                    initialUrl = trimmed;
                }
            }

            if (Dialogs::ShowNewDownloadDialog(m_hwnd, initialUrl, url, filename, saveDir, category, splitParts, startImmediately)) {
                std::wstring taskId = DownloadManager::Instance().AddTask(url, saveDir, category, splitParts, startImmediately, filename);
                m_selectedTaskId = taskId;
                Invalidate();
            }
            break;
        }

        case ID_FILE_BATCH_NEW: {
            Dialogs::ShowBatchDownloadDialog(m_hwnd);
            Invalidate();
            break;
        }

        case ID_FILE_VIDEO_LINK: {
            std::wstring clipText = ClipboardWatcher::GetClipboardText(m_hwnd);
            std::wstring initialUrl = WinHttpUtils::ExtractUrlFromText(clipText);
            Dialogs::ShowVideoLinkDialog(m_hwnd, initialUrl);
            Invalidate();
            break;
        }

        case ID_FILE_OLLAMA_MODEL: {
            std::wstring clipText = ClipboardWatcher::GetClipboardText(m_hwnd);
            std::wstring initialModel = clipText;
            if (!OllamaClient::IsLikelyOllamaModel(initialModel)) {
                initialModel.clear();
            }
            Dialogs::ShowOllamaModelDialog(m_hwnd, initialModel);
            Invalidate();
            break;
        }

        case ID_FILE_HF_MODEL: {
            std::wstring clipText = ClipboardWatcher::GetClipboardText(m_hwnd);
            std::wstring initialModel = clipText;
            if (!HuggingFaceClient::IsLikelyHuggingFaceModel(initialModel)) {
                initialModel.clear();
            }
            Dialogs::ShowHuggingFaceModelDialog(m_hwnd, initialModel);
            Invalidate();
            break;
        }

        case ID_FILE_EXIT: {
            m_forceExit = true;
            SendMessageW(m_hwnd, WM_CLOSE, 0, 0);
            break;
        }

        case ID_DOWNLOAD_START: {
            if (m_selectedTaskIds.empty() && !m_selectedTaskId.empty()) {
                m_selectedTaskIds.insert(m_selectedTaskId);
            }
            if (!m_selectedTaskIds.empty()) {
                for (const auto& tid : m_selectedTaskIds) {
                    DownloadManager::Instance().StartTask(tid);
                }
            } else {
                DownloadManager::Instance().ResumeAll();
            }
            Invalidate();
            break;
        }

        case ID_DOWNLOAD_PAUSE: {
            if (m_selectedTaskIds.empty() && !m_selectedTaskId.empty()) {
                m_selectedTaskIds.insert(m_selectedTaskId);
            }
            if (!m_selectedTaskIds.empty()) {
                for (const auto& tid : m_selectedTaskIds) {
                    DownloadManager::Instance().PauseTask(tid);
                }
            } else {
                DownloadManager::Instance().PauseAll();
            }
            Invalidate();
            break;
        }

        case ID_DOWNLOAD_DELETE: {
            if (m_selectedTaskIds.empty() && !m_selectedTaskId.empty()) {
                m_selectedTaskIds.insert(m_selectedTaskId);
            }
            if (!m_selectedTaskIds.empty()) {
                std::wstring prompt;
                if (m_selectedTaskIds.size() == 1) {
                    prompt = LStr(StrId::DlgDeletePrompt);
                } else {
                    prompt = std::to_wstring(m_selectedTaskIds.size()) + L" adet seçili görevi listeden silmek istediğinize emin misiniz?";
                }
                int res = MessageBoxW(m_hwnd, prompt.c_str(), LStr(StrId::DlgDeleteTitle), MB_YESNO | MB_ICONQUESTION);
                if (res == IDYES) {
                    for (const auto& tid : m_selectedTaskIds) {
                        DownloadManager::Instance().DeleteTask(tid, false);
                    }
                    m_selectedTaskIds.clear();
                    m_selectedTaskId.clear();
                    Invalidate();
                }
            }
            break;
        }

        case ID_TOOLS_SPEED_UNLIM:
            SetSpeedMode(SpeedMode::Unlimited);
            break;

        case ID_TOOLS_SPEED_MANUAL:
            SetSpeedMode(SpeedMode::ManualLimit);
            break;

        case ID_TOOLS_SPEED_BACK:
            SetSpeedMode(SpeedMode::Background);
            break;

        case ID_VIEW_DROPZONE: {
            Config::Instance().showDropZone = !Config::Instance().showDropZone;
            Config::Instance().Save();
            if (Config::Instance().showDropZone) {
                if (!m_dropZone.GetHwnd()) {
                    m_dropZone.Create(m_hwnd, Config::Instance().dropZoneX, Config::Instance().dropZoneY, Config::Instance().dropZoneOpacity);
                }
                m_dropZone.Show(true);
                UpdateDropZoneStats();
            } else {
                m_dropZone.Show(false);
            }
            Invalidate();
            break;
        }

        case ID_TOOLS_OPTIONS:
            if (Dialogs::ShowOptionsDialog(m_hwnd)) {
                ClipboardWatcher::Instance().SetEnabled(Config::Instance().monitorClipboard);
                if (Config::Instance().showDropZone) {
                    if (!m_dropZone.GetHwnd()) {
                        m_dropZone.Create(m_hwnd, Config::Instance().dropZoneX, Config::Instance().dropZoneY, Config::Instance().dropZoneOpacity);
                    }
                    m_dropZone.Show(true);
                    UpdateDropZoneStats();
                } else {
                    m_dropZone.Show(false);
                }
                Invalidate();
            }
            break;

        case ID_HELP_ABOUT:
            Dialogs::ShowAboutDialog(m_hwnd);
            break;

        default: {
            // Dynamic language command (ID_LANG_BASE + index)
            if (id >= ID_LANG_BASE && id < ID_LANG_BASE + (int)I18n::Instance().GetLanguages().size()) {
                int langIdx = id - ID_LANG_BASE;
                const auto& langs = I18n::Instance().GetLanguages();
                if (langIdx >= 0 && langIdx < (int)langs.size()) {
                    I18n::Instance().SetLanguage(langs[langIdx].id);
                    Config::Instance().language = langs[langIdx].code;
                    Config::Instance().Save();
                    Invalidate();
                }
            }
            break;
        }
    }
}

void MainWindow::SetSpeedMode(SpeedMode mode) {
    Config::Instance().speedMode = mode;
    Config::Instance().Save();

    SpeedLimiter::Instance().SetLimit(
        mode,
        Config::Instance().manualSpeedLimitKbps,
        Config::Instance().backgroundSpeedLimitKbps
    );
    Invalidate();
}

void MainWindow::ShowSpeedModeMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (Config::Instance().speedMode == SpeedMode::Unlimited ? MF_CHECKED : 0), ID_TOOLS_SPEED_UNLIM, LStr(StrId::MenuToolsSpeedUnlim));
    AppendMenuW(hMenu, MF_STRING | (Config::Instance().speedMode == SpeedMode::ManualLimit ? MF_CHECKED : 0), ID_TOOLS_SPEED_MANUAL, LStr(StrId::MenuToolsSpeedManual));
    AppendMenuW(hMenu, MF_STRING | (Config::Instance().speedMode == SpeedMode::Background ? MF_CHECKED : 0), ID_TOOLS_SPEED_BACK, LStr(StrId::MenuToolsSpeedBack));

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, screenX, screenY, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);
}

void MainWindow::ShowLanguageMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    const auto& langs = I18n::Instance().GetLanguages();
    LangId curLang = I18n::Instance().GetCurrentLanguage();

    for (size_t i = 0; i < langs.size(); ++i) {
        UINT flags = MF_STRING;
        if (langs[i].id == curLang) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, ID_LANG_BASE + (UINT)i, langs[i].nativeName.c_str());
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, screenX, screenY, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);
}

void MainWindow::ShowTrayContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESTORE, LStr(StrId::CtxOpenGety));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_FILE_NEW, LStr(StrId::MenuFileNew));
    AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_START, LStr(StrId::MenuTaskStart));
    AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_PAUSE, LStr(StrId::MenuTaskPause));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (Config::Instance().showDropZone ? MF_CHECKED : 0), ID_VIEW_DROPZONE, LStr(StrId::MenuViewDropZone));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_FILE_EXIT, LStr(StrId::MenuFileExit));

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);

    if (cmd == ID_TRAY_RESTORE) {
        ShowWindow(m_hwnd, SW_RESTORE);
        SetForegroundWindow(m_hwnd);
    } else if (cmd > 0) {
        HandleCommand(cmd);
    }
}

void MainWindow::ShowTaskContextMenu(int screenX, int screenY, const std::wstring& taskId) {
    DownloadTaskInfo info;
    if (!DownloadManager::Instance().GetSnapshot(taskId, info)) return;

    HMENU hMenu = CreatePopupMenu();
    if (info.state == DownloadState::Downloading) {
        AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_PAUSE, LStr(StrId::CtxPause));
    } else {
        AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_START, LStr(StrId::CtxStart));
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    if (info.state == DownloadState::Completed) {
        AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_OPENFILE, LStr(StrId::CtxOpenFile));
        AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_OPENFOLDER, LStr(StrId::CtxOpenFolder));
    }
    AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_PROPERTIES, LStr(StrId::CtxCopyUrl));
    if (info.state != DownloadState::Completed) {
        AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_UPDATE_URL, LStr(StrId::CtxUpdateUrl));
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_DELETE, LStr(StrId::CtxDeleteTask));

    int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, screenX, screenY, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);

    if (cmd == ID_DOWNLOAD_PAUSE) {
        if (m_selectedTaskIds.size() > 1 && m_selectedTaskIds.count(taskId)) {
            for (const auto& tid : m_selectedTaskIds) DownloadManager::Instance().PauseTask(tid);
        } else {
            DownloadManager::Instance().PauseTask(taskId);
        }
    } else if (cmd == ID_DOWNLOAD_START) {
        if (m_selectedTaskIds.size() > 1 && m_selectedTaskIds.count(taskId)) {
            for (const auto& tid : m_selectedTaskIds) DownloadManager::Instance().StartTask(tid);
        } else {
            DownloadManager::Instance().StartTask(taskId);
        }
    } else if (cmd == ID_DOWNLOAD_OPENFILE) {
        ShellExecuteW(NULL, L"open", info.fullPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    } else if (cmd == ID_DOWNLOAD_OPENFOLDER) {
        std::wstring dir = info.fullPath;
        size_t idx = dir.find_last_of(L"\\/");
        if (idx != std::wstring::npos) dir = dir.substr(0, idx);
        ShellExecuteW(NULL, L"open", dir.c_str(), NULL, NULL, SW_SHOWNORMAL);
    } else if (cmd == ID_DOWNLOAD_PROPERTIES) {
        if (OpenClipboard(m_hwnd)) {
            EmptyClipboard();
            size_t bytes = (info.url.length() + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (hMem) {
                memcpy(GlobalLock(hMem), info.url.c_str(), bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
    } else if (cmd == ID_DOWNLOAD_UPDATE_URL) {
        std::wstring newUrl;
        if (Dialogs::ShowUpdateUrlDialog(m_hwnd, info.url, newUrl)) {
            if (!newUrl.empty()) {
                DownloadManager::Instance().UpdateTaskUrl(taskId, newUrl);
            }
        }
    } else if (cmd == ID_DOWNLOAD_DELETE) {
        if (m_selectedTaskIds.size() > 1 && m_selectedTaskIds.count(taskId)) {
            std::wstring prompt = std::to_wstring(m_selectedTaskIds.size()) + L" adet seçili görevi listeden silmek istediğinize emin misiniz?";
            int res = MessageBoxW(m_hwnd, prompt.c_str(), LStr(StrId::DlgDeleteTitle), MB_YESNO | MB_ICONQUESTION);
            if (res == IDYES) {
                for (const auto& tid : m_selectedTaskIds) {
                    DownloadManager::Instance().DeleteTask(tid, false);
                }
                m_selectedTaskIds.clear();
                m_selectedTaskId.clear();
            }
        } else {
            int res = MessageBoxW(m_hwnd, LStr(StrId::DlgDeletePrompt), LStr(StrId::DlgDeleteTitle), MB_YESNO | MB_ICONQUESTION);
            if (res == IDYES) {
                DownloadManager::Instance().DeleteTask(taskId, false);
                m_selectedTaskIds.erase(taskId);
                if (m_selectedTaskId == taskId) m_selectedTaskId.clear();
            }
        }
    }
    Invalidate();
}

bool MainWindow::SaveSnapshot(const std::wstring& filePath) {
    std::vector<DownloadTaskInfo> tasks = DownloadManager::Instance().GetAllSnapshots();
    if (tasks.empty()) {
        // Mock sample tasks for screenshot preview
        DownloadTaskInfo t1;
        t1.id = L"task-ubuntu-24";
        t1.filename = L"ubuntu-24.04-desktop-amd64.iso";
        t1.url = L"https://releases.ubuntu.com/24.04/ubuntu-24.04-desktop-amd64.iso";
        t1.fullPath = L"C:\\Downloads\\ubuntu-24.04-desktop-amd64.iso";
        t1.totalBytes = 6116032512ULL;
        t1.downloadedBytes = 4125163520ULL;
        t1.currentSpeedBps = 12.4 * 1024 * 1024;
        t1.splitCount = 10;
        t1.state = DownloadState::Downloading;
        bool isTr = (I18n::Instance().GetCurrentLanguage() == LangId::Turkish);
        t1.category = isTr ? L"Yazılım" : L"Software";
        t1.supportsResume = true;
        t1.addedDate = L"2026-09-07 20:15";

        for (int i = 0; i < 10; ++i) {
            SegmentInfo seg;
            seg.id = i;
            seg.startByte = i * (t1.totalBytes / 10);
            seg.endByte = (i == 9) ? (t1.totalBytes - 1) : ((i + 1) * (t1.totalBytes / 10) - 1);
            seg.currentOffset = seg.startByte + (seg.endByte - seg.startByte) * ((i < 6) ? 1.0 : 0.4);
            seg.downloadedBytes = (seg.endByte - seg.startByte) * ((i < 6) ? 1.0 : 0.4);
            seg.status = (i < 6) ? 3 : 2;
            seg.speedBps = (i < 6) ? 0.0 : 2.1 * 1024 * 1024;
            t1.segments.push_back(seg);
        }

        if (isTr) {
            t1.logs = {
                { L"02:15:01", L"Bağlantı kuruluyor: releases.ubuntu.com:443", 0 },
                { L"02:15:02", L"HTTP 200 OK. Sunucu Range (Resume) desteğini onayladı.", 1 },
                { L"02:15:02", L"Dosya boyutu tespit edildi: 5.64 GB (5,780,275,200 bayt)", 0 },
                { L"02:15:03", L"10 eşzamanlı multi-chunk indirme thread'i başlatıldı.", 1 },
                { L"02:15:10", L"Segment #1 tamamlandı, yeni dinamik aralık birleştiriliyor.", 1 },
                { L"02:15:30", L"Anlık maksimum bant genişliğine ulaşıldı: 14.80 MB/s", 0 }
            };
        } else {
            t1.logs = {
                { L"02:15:01", L"Connecting to: releases.ubuntu.com:443", 0 },
                { L"02:15:02", L"HTTP 200 OK. Server confirmed Range (Resume) support.", 1 },
                { L"02:15:02", L"Content length detected: 5.64 GB (5,780,275,200 bytes)", 0 },
                { L"02:15:03", L"10 parallel multi-chunk streaming threads started.", 1 },
                { L"02:15:10", L"Segment #1 completed, merging dynamic byte range.", 1 },
                { L"02:15:30", L"Peak bandwidth achieved: 14.80 MB/s", 0 }
            };
        }

        DownloadTaskInfo t2;
        t2.id = L"task-vscode";
        t2.url = L"https://update.code.visualstudio.com/latest/win32-x64-user/stable";
        t2.filename = L"VSCodeUserSetup-x64-1.93.1.exe";
        t2.saveDirectory = L"C:\\Downloads\\Programs";
        t2.fullPath = L"C:\\Downloads\\Programs\\VSCodeUserSetup-x64-1.93.1.exe";
        t2.category = isTr ? L"Yazılım" : L"Software";
        t2.totalBytes = 94ULL * 1024 * 1024;
        t2.downloadedBytes = 94ULL * 1024 * 1024;
        t2.state = DownloadState::Completed;
        t2.currentSpeedBps = 0.0;
        t2.splitCount = 5;
        t2.supportsResume = true;
        t2.addedDate = L"2026-09-06 01:40";
        t2.completedDate = L"2026-09-06 01:42";

        DownloadTaskInfo t3;
        t3.id = L"task-music";
        t3.url = L"https://audio.example.com/cyberpunk_ost_lossless.flac";
        t3.filename = L"Cyberpunk_2077_Original_Soundtrack.flac";
        t3.saveDirectory = L"C:\\Downloads\\Music";
        t3.fullPath = L"C:\\Downloads\\Music\\Cyberpunk_2077_Original_Soundtrack.flac";
        t3.category = isTr ? L"Müzik" : L"Music";
        t3.totalBytes = 420ULL * 1024 * 1024;
        t3.downloadedBytes = 115ULL * 1024 * 1024;
        t3.state = DownloadState::Paused;
        t3.currentSpeedBps = 0.0;
        t3.splitCount = 8;
        t3.supportsResume = true;
        DownloadTaskInfo t4;
        t4.id = L"task-hf-mistral";
        t4.url = L"https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf";
        t4.filename = L"mistral-7b-instruct-v0.2.Q4_K_M.gguf";
        t4.saveDirectory = L"C:\\Downloads\\Models";
        t4.fullPath = L"C:\\Downloads\\Models\\mistral-7b-instruct-v0.2.Q4_K_M.gguf";
        t4.category = isTr ? L"Yapay Zeka" : L"AI Models";
        t4.totalBytes = 4370546688ULL; // 4.07 GB
        t4.downloadedBytes = 2306867200ULL; // 2.15 GB
        t4.currentSpeedBps = 18.5 * 1024 * 1024;
        t4.splitCount = 16;
        t4.state = DownloadState::Downloading;
        t4.supportsResume = true;
        t4.addedDate = L"2026-09-07 20:20";

        DownloadTaskInfo t5;
        t5.id = L"task-ollama-nomic";
        t5.url = L"https://registry.ollama.ai/v2/library/nomic-embed-text-v2-moe/blobs/sha256:913mb";
        t5.filename = L"nomic-embed-text-v2-moe.gguf";
        t5.saveDirectory = L"C:\\Downloads\\Models";
        t5.fullPath = L"C:\\Downloads\\Models\\nomic-embed-text-v2-moe.gguf";
        t5.category = isTr ? L"Yapay Zeka" : L"AI Models";
        t5.totalBytes = 957677568ULL; // 913.3 MB
        t5.downloadedBytes = 957677568ULL;
        t5.currentSpeedBps = 0.0;
        t5.splitCount = 10;
        t5.state = DownloadState::Completed;
        t5.supportsResume = true;
        t5.addedDate = L"2026-09-07 19:50";
        t5.completedDate = L"2026-09-07 19:53";

        tasks.push_back(t1);
        tasks.push_back(t4);
        tasks.push_back(t2);
        tasks.push_back(t5);
        tasks.push_back(t3);
        m_selectedTaskId = t1.id;
        m_selectedTaskIds.insert(t1.id);
        m_selectedTaskIds.insert(t4.id);
    }

    if (m_selectedTaskId.empty() && !tasks.empty()) {
        m_selectedTaskId = tasks[0].id;
    }

    return m_renderer.SaveSnapshot(filePath, tasks, m_selectedTaskIds, m_selectedTaskId, m_currentCategory, m_activeTab);
}

void MainWindow::UpdateDropZoneStats() {
    if (!m_dropZone.GetHwnd() || !m_dropZone.IsVisible()) return;

    double totalSpeed = 0.0;
    int activeCount = 0;
    uint64_t totalDownloaded = 0;
    uint64_t totalBytes = 0;

    auto tasks = DownloadManager::Instance().GetAllSnapshots();
    for (const auto& t : tasks) {
        if (t.state == DownloadState::Downloading || t.state == DownloadState::Connecting) {
            activeCount++;
            totalSpeed += t.currentSpeedBps;
        }
        totalDownloaded += t.downloadedBytes;
        totalBytes += t.totalBytes;
    }

    double prog = (totalBytes > 0) ? (double)totalDownloaded / (double)totalBytes : 0.0;
    m_dropZone.UpdateStats(totalSpeed, activeCount, prog);
}

void MainWindow::CheckAutoShutdown() {
    if (!Config::Instance().autoShutdownOnComplete) return;

    static bool s_shutdownTriggered = false;
    if (s_shutdownTriggered) return;

    auto tasks = DownloadManager::Instance().GetAllSnapshots();
    if (tasks.empty()) return;

    bool hasActive = false;
    bool hasCompleted = false;
    for (const auto& t : tasks) {
        if (t.state == DownloadState::Downloading || t.state == DownloadState::Connecting || t.state == DownloadState::Queued) {
            hasActive = true;
            break;
        }
        if (t.state == DownloadState::Completed) {
            hasCompleted = true;
        }
    }

    if (!hasActive && hasCompleted) {
        s_shutdownTriggered = true;
        // Schedule clean Windows shutdown in 60 seconds with warning message
        system("shutdown /s /t 60 /c \"Gety: Tum indirmeler tamamlandi. Bilgisayar 60 saniye icinde kapatilacak. Iptal icin: shutdown /a\"");
    }
}

} // namespace Gety
