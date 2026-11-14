#pragma once

#include "Types.h"
#include <string>
#include <filesystem>

namespace Gety {

class Config {
public:
    static Config& Instance();

    void Load();
    void Save();

    std::wstring GetAppDir() const { return m_appDir; }
    std::wstring GetPortableDataDir() const { return m_portableDataDir; }
    std::wstring GetIniFilePath() const { return m_iniFilePath; }
    void SetPortableDataDir(const std::wstring& dir) {
        m_portableDataDir = dir;
        m_iniFilePath = dir + L"\\Gety.ini";
        CreateDirectoryW(m_portableDataDir.c_str(), NULL);
    }

    // Settings
    std::wstring defaultSavePath;
    int maxActiveDownloads = 3;
    int defaultSplitParts = 10;
    int maxRetryAttempts = 10;
    
    SpeedMode speedMode = SpeedMode::Unlimited;
    int manualSpeedLimitKbps = 1024;
    int backgroundSpeedLimitKbps = 256;

    bool monitorClipboard = true;
    bool showDropZone = true;
    int dropZoneX = 100;
    int dropZoneY = 100;
    int dropZoneOpacity = 220; // 0-255

    bool playSoundOnComplete = true;
    bool autoShutdownOnComplete = false;
    bool minimizeToTrayOnClose = true;
    bool startWithWindows = false;
    std::wstring language = L"en";
    std::wstring huggingFaceToken;

    bool IsAutoStartEnabled() const;
    void SetAutoStartEnabled(bool enable);
    std::wstring GetAutoStartCommand() const;
    static std::wstring BuildAutoStartCommand(const std::wstring& exePath);

    // Parameterized helpers (default valueName = L"Gety") to support clean unit testing
    bool IsAutoStartValueEnabled(const wchar_t* valueName) const;
    bool SetAutoStartValue(bool enable, const wchar_t* valueName) const;
    std::wstring ReadAutoStartValue(const wchar_t* valueName) const;

    int windowX = 100;
    int windowY = 100;
    int windowW = 1000;
    int windowH = 650;

    int leftPaneWidth = 230;
    int bottomPaneHeight = 440;

private:
    Config();
    ~Config() = default;

    std::wstring m_appDir;
    std::wstring m_portableDataDir;
    std::wstring m_iniFilePath;

    void InitPaths();
};

} // namespace Gety
