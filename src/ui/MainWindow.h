#pragma once

#include "Direct2DRenderer.h"
#include "FloatingWindow.h"
#include "TrayIcon.h"
#include "../core/Types.h"
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Gety {

class MainWindow {
public:
    static MainWindow& Instance();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    HWND GetHwnd() const { return m_hwnd; }
    void RunMessageLoop();
    bool SaveSnapshot(const std::wstring& filePath);

private:
    MainWindow();
    ~MainWindow();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void Render();
    void Invalidate();

    void HandleCommand(int id);
    void ShowSpeedModeMenu(int screenX, int screenY);
    void ShowLanguageMenu(int screenX, int screenY);
    void ShowTrayContextMenu();
    void ShowTaskContextMenu(int screenX, int screenY, const std::wstring& taskId);

    void SetSpeedMode(SpeedMode mode);
    void UpdateDropZoneStats();
    void CheckAutoShutdown();

    HINSTANCE m_hInstance = NULL;
    HWND m_hwnd = NULL;

    Direct2DRenderer m_renderer;
    FloatingWindow m_dropZone;
    TrayIcon m_trayIcon;

    int m_splitterX = 230;
    int m_splitterY = 320;
    bool m_draggingSplitterV = false;
    bool m_draggingSplitterH = false;
    bool m_isHoveringSplitterV = false;
    bool m_isHoveringSplitterH = false;

    CategoryFilterType m_currentCategory = CategoryFilterType::All;
    DetailTab m_activeTab = DetailTab::Matrix;
    std::unordered_set<std::wstring> m_selectedTaskIds;
    std::wstring m_selectedTaskId;

    bool m_forceExit = false;
    bool m_shownBalloonOnce = false;
};

} // namespace Gety
