#include "ClipboardWatcher.h"
#include "resource.h"
#include <algorithm>
#include <vector>

namespace Gety {

ClipboardWatcher& ClipboardWatcher::Instance() {
    static ClipboardWatcher instance;
    return instance;
}

ClipboardWatcher::ClipboardWatcher() = default;

ClipboardWatcher::~ClipboardWatcher() {
    Uninit();
}

void ClipboardWatcher::Init(HWND targetHwnd) {
    m_targetHwnd = targetHwnd;
    if (m_targetHwnd) {
        AddClipboardFormatListener(m_targetHwnd);
    }
}

void ClipboardWatcher::Uninit() {
    if (m_targetHwnd) {
        RemoveClipboardFormatListener(m_targetHwnd);
        m_targetHwnd = NULL;
    }
}

bool ClipboardWatcher::IsDownloadableUrl(const std::wstring& text) {
    if (text.empty()) return false;
    
    // Trim both leading and trailing whitespace
    size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return false;
    size_t last = text.find_last_not_of(L" \t\r\n");
    std::wstring s = text.substr(first, (last - first + 1));

    // Check scheme
    bool hasScheme = (s.rfind(L"http://", 0) == 0) || 
                     (s.rfind(L"https://", 0) == 0) || 
                     (s.rfind(L"ftp://", 0) == 0);
    if (!hasScheme) return false;

    // Check if it contains spaces or invalid chars
    if (s.find_first_of(L" \r\n\t\"'<>") != std::wstring::npos) {
        return false;
    }

    return true;
}

void ClipboardWatcher::OnClipboardUpdate() {
    if (!m_enabled || !m_targetHwnd) return;

    if (!OpenClipboard(m_targetHwnd)) return;

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData != NULL) {
        wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pszText != NULL) {
            std::wstring text = pszText;
            GlobalUnlock(hData);

            if (IsDownloadableUrl(text)) {
                // Avoid prompt loop for same url
                if (text != m_lastCapturedUrl) {
                    m_lastCapturedUrl = text;
                    // Allocate heap copy of string for post message
                    wchar_t* pCopy = _wcsdup(text.c_str());
                    PostMessageW(m_targetHwnd, WM_APP_CLIPBOARD_URL, 0, (LPARAM)pCopy);
                }
            }
        }
    }
    CloseClipboard();
}

std::wstring ClipboardWatcher::GetClipboardText(HWND hwndOwner) {
    std::wstring result;
    if (OpenClipboard(hwndOwner)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData != NULL) {
            wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
            if (pszText != NULL) {
                result = pszText;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
    return result;
}

} // namespace Gety
