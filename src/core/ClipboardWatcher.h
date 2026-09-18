#pragma once

#include <windows.h>
#include <string>
#include <functional>

namespace Gety {

class ClipboardWatcher {
public:
    static ClipboardWatcher& Instance();

    void Init(HWND targetHwnd);
    void Uninit();

    void OnClipboardUpdate();
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    static bool IsDownloadableUrl(const std::wstring& text);
    static std::wstring GetClipboardText(HWND hwndOwner = NULL);

private:
    ClipboardWatcher();
    ~ClipboardWatcher();

    HWND m_targetHwnd = NULL;
    bool m_enabled = true;
    std::wstring m_lastCapturedUrl;
};

} // namespace Gety
