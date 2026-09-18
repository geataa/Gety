#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>

namespace Gety {

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    bool Create(HWND hOwner, UINT uCallbackMsg, HICON hIcon, const std::wstring& tip);
    void Remove();
    void SetTooltip(const std::wstring& tip);
    void ShowBalloon(const std::wstring& title, const std::wstring& text);

private:
    HWND m_hOwner = NULL;
    UINT m_uCallbackMsg = 0;
    bool m_installed = false;
    NOTIFYICONDATAW m_nid = { 0 };
};

} // namespace Gety
