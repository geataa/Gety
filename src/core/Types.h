#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "I18n.h"

namespace Gety {

enum class DownloadState {
    Queued,
    Connecting,
    Downloading,
    Paused,
    Completed,
    Failed,
    Deleted
};

enum class SpeedMode {
    Unlimited = 0,
    ManualLimit = 1,
    Background = 2
};

enum class CategoryFilterType {
    All = 0,
    Unfinished,
    Downloading,
    Paused,
    Downloaded,
    Music,
    Video,
    Software,
    Games,
    Documents,
    Archives,
    AI,
    Trash
};

struct SegmentInfo {
    int id = 0;
    uint64_t startByte = 0;
    uint64_t endByte = 0;
    uint64_t currentOffset = 0;
    uint64_t downloadedBytes = 0;
    int status = 0; // 0=idle, 1=connecting, 2=downloading, 3=done, 4=error
    double speedBps = 0.0;

    uint64_t totalBytes() const {
        return (endByte >= startByte) ? (endByte - startByte + 1) : 0;
    }

    double progress() const {
        uint64_t total = totalBytes();
        if (total == 0) return 0.0;
        return (double)downloadedBytes / (double)total;
    }
};

struct TaskLogEntry {
    std::wstring timestamp;
    std::wstring message;
    int level = 0; // 0=info, 1=success, 2=warning, 3=error
};

struct DownloadTaskInfo {
    std::wstring id;
    std::wstring url;
    std::wstring filename;
    std::wstring saveDirectory;
    std::wstring fullPath;
    std::wstring category;
    
    uint64_t totalBytes = 0;
    uint64_t downloadedBytes = 0;
    DownloadState state = DownloadState::Queued;
    
    double currentSpeedBps = 0.0;
    int splitCount = 10;
    bool supportsResume = false;
    
    std::vector<SegmentInfo> segments;
    std::vector<TaskLogEntry> logs;
    
    std::wstring addedDate;
    std::wstring completedDate;
    std::wstring errorMsg;
    
    std::wstring referer;
    std::wstring userAgent;

    double progress() const {
        if (totalBytes == 0) return 0.0;
        return (double)downloadedBytes / (double)totalBytes;
    }
};

inline std::wstring StateToString(DownloadState state) {
    switch (state) {
        case DownloadState::Queued:      return LStr(StrId::StateQueued);
        case DownloadState::Connecting:  return LStr(StrId::StateConnecting);
        case DownloadState::Downloading: return LStr(StrId::StateDownloading);
        case DownloadState::Paused:      return LStr(StrId::StatePaused);
        case DownloadState::Completed:   return LStr(StrId::StateCompleted);
        case DownloadState::Failed:      return LStr(StrId::StateFailed);
        case DownloadState::Deleted:     return LStr(StrId::StateDeleted);
    }
    return LStr(StrId::StateUnknown);
}

inline std::wstring FormatBytes(uint64_t bytes) {
    wchar_t buf[64];
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        swprintf_s(buf, L"%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024ULL) {
        swprintf_s(buf, L"%.2f MB", (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024ULL) {
        swprintf_s(buf, L"%.1f KB", (double)bytes / 1024.0);
    } else {
        swprintf_s(buf, L"%llu B", bytes);
    }
    return buf;
}

inline std::wstring FormatSpeed(double bytesPerSec) {
    if (bytesPerSec <= 0.0) return L"0 B/s";
    return FormatBytes((uint64_t)bytesPerSec) + L"/s";
}

inline std::wstring FormatEta(uint64_t remainingBytes, double speedBps) {
    if (speedBps <= 10.0 || remainingBytes == 0) return L"--:--:--";
    uint64_t totalSeconds = (uint64_t)(remainingBytes / speedBps);
    uint64_t hours = totalSeconds / 3600;
    uint64_t mins = (totalSeconds % 3600) / 60;
    uint64_t secs = totalSeconds % 60;
    wchar_t buf[64];
    if (hours > 0) {
        swprintf_s(buf, L"%02llu:%02llu:%02llu", hours, mins, secs);
    } else {
        swprintf_s(buf, L"%02llu:%02llu", mins, secs);
    }
    return buf;
}

} // namespace Gety
