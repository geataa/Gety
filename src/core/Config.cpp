#include "Config.h"
#include "I18n.h"
#include <shlwapi.h>

namespace Gety {

Config& Config::Instance() {
    static Config instance;
    return instance;
}

Config::Config() {
    InitPaths();
    Load();
}

void Config::InitPaths() {
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    m_appDir = exePath;

    m_portableDataDir = m_appDir + L"\\portable_data";
    m_iniFilePath = m_portableDataDir + L"\\Gety.ini";

    // Ensure portable directory exists
    CreateDirectoryW(m_portableDataDir.c_str(), NULL);

    // Default download path inside AppDir/Downloads
    defaultSavePath = m_appDir + L"\\Downloads";
    CreateDirectoryW(defaultSavePath.c_str(), NULL);
}

void Config::Load() {
    if (!PathFileExistsW(m_iniFilePath.c_str())) {
        Save();
        return;
    }

    const wchar_t* ini = m_iniFilePath.c_str();

    wchar_t buf[MAX_PATH] = { 0 };
    GetPrivateProfileStringW(L"General", L"DefaultSavePath", defaultSavePath.c_str(), buf, MAX_PATH, ini);
    defaultSavePath = buf;

    maxActiveDownloads = GetPrivateProfileIntW(L"General", L"MaxActiveDownloads", maxActiveDownloads, ini);
    defaultSplitParts = GetPrivateProfileIntW(L"General", L"DefaultSplitParts", defaultSplitParts, ini);
    maxRetryAttempts = GetPrivateProfileIntW(L"General", L"MaxRetryAttempts", maxRetryAttempts, ini);
    
    speedMode = (SpeedMode)GetPrivateProfileIntW(L"Speed", L"SpeedMode", (int)speedMode, ini);
    manualSpeedLimitKbps = GetPrivateProfileIntW(L"Speed", L"ManualLimitKbps", manualSpeedLimitKbps, ini);
    backgroundSpeedLimitKbps = GetPrivateProfileIntW(L"Speed", L"BackgroundLimitKbps", backgroundSpeedLimitKbps, ini);

    monitorClipboard = GetPrivateProfileIntW(L"Automation", L"MonitorClipboard", monitorClipboard ? 1 : 0, ini) != 0;
    playSoundOnComplete = GetPrivateProfileIntW(L"Automation", L"PlaySoundOnComplete", playSoundOnComplete ? 1 : 0, ini) != 0;
    autoShutdownOnComplete = GetPrivateProfileIntW(L"Automation", L"AutoShutdownOnComplete", autoShutdownOnComplete ? 1 : 0, ini) != 0;
    minimizeToTrayOnClose = GetPrivateProfileIntW(L"Automation", L"MinimizeToTrayOnClose", minimizeToTrayOnClose ? 1 : 0, ini) != 0;
    startWithWindows = GetPrivateProfileIntW(L"Automation", L"StartWithWindows", startWithWindows ? 1 : 0, ini) != 0;
    if (IsAutoStartEnabled() != startWithWindows) {
        startWithWindows = IsAutoStartEnabled();
    }

    GetPrivateProfileStringW(L"General", L"Language", language.c_str(), buf, MAX_PATH, ini);
    language = buf;
    I18n::Instance().SetLanguageByCode(language);

    wchar_t tokenBuf[1024] = { 0 };
    GetPrivateProfileStringW(L"AI", L"HuggingFaceToken", huggingFaceToken.c_str(), tokenBuf, 1024, ini);
    huggingFaceToken = tokenBuf;

    showDropZone = GetPrivateProfileIntW(L"DropZone", L"Show", showDropZone ? 1 : 0, ini) != 0;
    dropZoneX = GetPrivateProfileIntW(L"DropZone", L"X", dropZoneX, ini);
    dropZoneY = GetPrivateProfileIntW(L"DropZone", L"Y", dropZoneY, ini);
    dropZoneOpacity = GetPrivateProfileIntW(L"DropZone", L"Opacity", dropZoneOpacity, ini);

    windowX = GetPrivateProfileIntW(L"Window", L"X", windowX, ini);
    windowY = GetPrivateProfileIntW(L"Window", L"Y", windowY, ini);
    windowW = GetPrivateProfileIntW(L"Window", L"W", windowW, ini);
    windowH = GetPrivateProfileIntW(L"Window", L"H", windowH, ini);
    leftPaneWidth = GetPrivateProfileIntW(L"Window", L"LeftWidth", leftPaneWidth, ini);
    bottomPaneHeight = GetPrivateProfileIntW(L"Window", L"BottomHeight", bottomPaneHeight, ini);
}

void Config::Save() {
    const wchar_t* ini = m_iniFilePath.c_str();

    WritePrivateProfileStringW(L"General", L"DefaultSavePath", defaultSavePath.c_str(), ini);
    WritePrivateProfileStringW(L"General", L"MaxActiveDownloads", std::to_wstring(maxActiveDownloads).c_str(), ini);
    WritePrivateProfileStringW(L"General", L"DefaultSplitParts", std::to_wstring(defaultSplitParts).c_str(), ini);
    WritePrivateProfileStringW(L"General", L"MaxRetryAttempts", std::to_wstring(maxRetryAttempts).c_str(), ini);
    WritePrivateProfileStringW(L"General", L"Language", language.c_str(), ini);
    WritePrivateProfileStringW(L"AI", L"HuggingFaceToken", huggingFaceToken.c_str(), ini);

    WritePrivateProfileStringW(L"Speed", L"SpeedMode", std::to_wstring((int)speedMode).c_str(), ini);
    WritePrivateProfileStringW(L"Speed", L"ManualLimitKbps", std::to_wstring(manualSpeedLimitKbps).c_str(), ini);
    WritePrivateProfileStringW(L"Speed", L"BackgroundLimitKbps", std::to_wstring(backgroundSpeedLimitKbps).c_str(), ini);

    WritePrivateProfileStringW(L"Automation", L"MonitorClipboard", monitorClipboard ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"Automation", L"PlaySoundOnComplete", playSoundOnComplete ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"Automation", L"AutoShutdownOnComplete", autoShutdownOnComplete ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"Automation", L"MinimizeToTrayOnClose", minimizeToTrayOnClose ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"Automation", L"StartWithWindows", startWithWindows ? L"1" : L"0", ini);
    SetAutoStartEnabled(startWithWindows);

    WritePrivateProfileStringW(L"DropZone", L"Show", showDropZone ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"DropZone", L"X", std::to_wstring(dropZoneX).c_str(), ini);
    WritePrivateProfileStringW(L"DropZone", L"Y", std::to_wstring(dropZoneY).c_str(), ini);
    WritePrivateProfileStringW(L"DropZone", L"Opacity", std::to_wstring(dropZoneOpacity).c_str(), ini);

    WritePrivateProfileStringW(L"Window", L"X", std::to_wstring(windowX).c_str(), ini);
    WritePrivateProfileStringW(L"Window", L"Y", std::to_wstring(windowY).c_str(), ini);
    WritePrivateProfileStringW(L"Window", L"W", std::to_wstring(windowW).c_str(), ini);
    WritePrivateProfileStringW(L"Window", L"H", std::to_wstring(windowH).c_str(), ini);
    WritePrivateProfileStringW(L"Window", L"LeftWidth", std::to_wstring(leftPaneWidth).c_str(), ini);
    WritePrivateProfileStringW(L"Window", L"BottomHeight", std::to_wstring(bottomPaneHeight).c_str(), ini);
}

bool Config::IsAutoStartEnabled() const {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH] = { 0 };
        DWORD bufSize = sizeof(buf);
        DWORD type = 0;
        LONG res = RegQueryValueExW(hKey, L"Gety", NULL, &type, (LPBYTE)buf, &bufSize);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS && type == REG_SZ && bufSize > 0);
    }
    return false;
}

void Config::SetAutoStartEnabled(bool enable) {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH] = { 0 };
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\" --minimized";
            RegSetValueExW(hKey, L"Gety", 0, REG_SZ,
                          (const BYTE*)cmd.c_str(),
                          (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"Gety");
        }
        RegCloseKey(hKey);
    }
}

} // namespace Gety
