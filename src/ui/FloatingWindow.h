#pragma once

#include "../core/Types.h"
#include <windows.h>
#include <string>

namespace Gety {

class FloatingWindow {
public:
    FloatingWindow();
    ~FloatingWindow();

    bool Create(HWND hMainWnd, int x, int y, int opacity);
    void Show(bool show);
    bool IsVisible() const;
    void SetOpacity(int opacity); // 0-255
    HWND GetHwnd() const { return m_hwnd; }

    void UpdateStats(double totalSpeedBps, int activeTasks, double totalProgress);
    void UpdateShape();
    bool SaveSnapshot(const std::wstring& filePath, double speedBps, int activeTasks, double progress);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HDC hdc);
    void ShowContextMenu(int screenX, int screenY);
    void RecreateFonts();

    HWND m_hwnd = NULL;
    HWND m_hMainWnd = NULL;
    int m_baseWidth = 152;  // 19:9 ratio (152 x 72)
    int m_baseHeight = 72;
    int m_width = 152;
    int m_height = 72;
    int m_opacity = 225;
    UINT m_dpi = 96;

    HFONT m_hTitleFont = NULL;
    HFONT m_hSpeedFont = NULL;
    HFONT m_hSubFont = NULL;
    HFONT m_hIdleFont = NULL;

    double m_speedBps = 0.0;
    int m_activeCount = 0;
    double m_progress = 0.0;
    int m_animTick = 0;
};

} // namespace Gety
