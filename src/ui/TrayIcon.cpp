#include "TrayIcon.h"

namespace Gety {

TrayIcon::TrayIcon() = default;
TrayIcon::~TrayIcon() {
    Remove();
}

bool TrayIcon::Create(HWND hOwner, UINT uCallbackMsg, HICON hIcon, const std::wstring& tip) {
    m_hOwner = hOwner;
    m_uCallbackMsg = uCallbackMsg;

    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hOwner;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = m_uCallbackMsg;
    m_nid.hIcon = hIcon;
    wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), tip.c_str(), _TRUNCATE);

    m_installed = Shell_NotifyIconW(NIM_ADD, &m_nid) != FALSE;
    return m_installed;
}

void TrayIcon::Remove() {
    if (m_installed) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_installed = false;
    }
}

void TrayIcon::SetTooltip(const std::wstring& tip) {
    if (m_installed) {
        m_nid.uFlags = NIF_TIP;
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), tip.c_str(), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& text) {
    if (m_installed) {
        m_nid.uFlags = NIF_INFO;
        m_nid.dwInfoFlags = NIIF_INFO;
        wcsncpy_s(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle), title.c_str(), _TRUNCATE);
        wcsncpy_s(m_nid.szInfo, _countof(m_nid.szInfo), text.c_str(), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
}

} // namespace Gety
