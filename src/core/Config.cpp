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
    bool iniStartWithWindows = GetPrivateProfileIntW(L"Automation", L"StartWithWindows", startWithWindows ? 1 : 0, ini) != 0;
    if (iniStartWithWindows) {
        startWithWindows = true;
        // Self-heal registry path in case portable Gety.exe was moved or copied to a new PC
        wchar_t exePath[2048] = { 0 };
        GetModuleFileNameW(NULL, exePath, _countof(exePath));
        std::wstring expectedCmd = BuildAutoStartCommand(exePath);
        if (GetAutoStartCommand() != expectedCmd) {
            SetAutoStartEnabled(true);
        }
    } else {
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

std::wstring Config::BuildAutoStartCommand(const std::wstring& exePath) {
    if (exePath.empty()) return L"";
    return L"\"" + exePath + L"\" --minimized";
}

std::wstring Config::ReadAutoStartValue(const wchar_t* valueName) const {
    if (!valueName || !*valueName) return L"";
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[2048] = { 0 };
        DWORD bufSize = sizeof(buf) - sizeof(wchar_t);
        DWORD type = 0;
        LONG res = RegQueryValueExW(hKey, valueName, NULL, &type, (LPBYTE)buf, &bufSize);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && bufSize > sizeof(wchar_t)) {
            buf[_countof(buf) - 1] = L'\0';
            return std::wstring(buf);
        }
    }
    return L"";
}

bool Config::IsAutoStartValueEnabled(const wchar_t* valueName) const {
    return !ReadAutoStartValue(valueName).empty();
}

bool Config::SetAutoStartValue(bool enable, const wchar_t* valueName) const {
    if (!valueName || !*valueName) return false;
    HKEY hKey = NULL;
    REGSAM access = KEY_SET_VALUE | KEY_QUERY_VALUE;
    LONG openRes = RegOpenKeyExW(HKEY_CURRENT_USER,
                                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                 0, access, &hKey);
    if (openRes != ERROR_SUCCESS && enable) {
        openRes = RegCreateKeyExW(HKEY_CURRENT_USER,
                                  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                  0, NULL, REG_OPTION_NON_VOLATILE, access, NULL, &hKey, NULL);
    }
    if (openRes != ERROR_SUCCESS) return false;

    bool ok = false;
    if (enable) {
        wchar_t exePath[2048] = { 0 };
        DWORD len = GetModuleFileNameW(NULL, exePath, _countof(exePath));
        if (len > 0 && len < _countof(exePath)) {
            std::wstring cmd = BuildAutoStartCommand(std::wstring(exePath, len));
            LONG res = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                                      (const BYTE*)cmd.c_str(),
                                      (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
            ok = (res == ERROR_SUCCESS);
        }
    } else {
        LONG res = RegDeleteValueW(hKey, valueName);
        ok = (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}

std::wstring Config::GetAutoStartCommand() const {
    return ReadAutoStartValue(L"Gety");
}

bool Config::IsAutoStartEnabled() const {
    return IsAutoStartValueEnabled(L"Gety");
}

void Config::SetAutoStartEnabled(bool enable) {
    SetAutoStartValue(enable, L"Gety");
}

} // namespace Gety
