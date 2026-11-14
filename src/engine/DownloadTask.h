#pragma once

#include "../core/Types.h"
#include "SegmentWorker.h"
#include <windows.h>
#include <memory>
#include <vector>
#include <deque>
#include <chrono>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace Gety {

class DownloadTask {
public:
    using TaskEventCallback = std::function<void(const std::wstring& taskId)>;

    DownloadTask(const std::wstring& url,
                 const std::wstring& saveDir,
                 const std::wstring& category = L"Genel",
                 int splitParts = 10,
                 const std::wstring& customFilename = L"",
                 bool isMediaTask = false,
                 const std::wstring& mediaFormat = L"");
    ~DownloadTask();

    void Start();
    void Pause();
    void Stop();
    void Redownload();
    void DeleteTask(bool deleteFile);

    DownloadTaskInfo GetSnapshot() const;
    std::wstring GetId() const { return m_info.id; }
    DownloadState GetState() const { return m_state.load(); }
    std::wstring GetUrl() const { return m_info.url; }
    std::wstring GetFilename() const { return m_info.filename; }
    std::wstring GetCategory() const { return m_info.category; }
    void SetCategory(const std::wstring& cat);

    bool IsMediaTask() const { return m_isMediaTask; }
    std::wstring GetMediaFormat() const { return m_mediaFormat; }
    bool IsCustomFilenameSet() const { std::lock_guard<std::recursive_mutex> lock(m_mutex); return m_customFilenameSet; }
    std::wstring GetResolvedUrl() const { std::lock_guard<std::recursive_mutex> lock(m_mutex); return m_resolvedUrl; }

    void SetEventCallback(TaskEventCallback cb) { m_eventCb = std::move(cb); }
    void SetStateChangedCallback(TaskEventCallback cb) { m_stateChangedCb = std::move(cb); }

    void LoadState();
    void SaveState();
    bool UpdateUrl(const std::wstring& newUrl);
    bool Rename(const std::wstring& newFilename);
    void RestoreMetadata(const std::wstring& id,
                         DownloadState state,
                         const std::wstring& addedDate,
                         const std::wstring& completedDate,
                         const std::wstring& category,
                         uint64_t totalBytes = 0,
                         uint64_t downloadedBytes = 0,
                         bool supportsResume = true,
                         bool customFilenameSet = false,
                         const std::wstring& resolvedUrl = L"",
                         const std::wstring& errorMsg = L"",
                         const std::vector<SegmentInfo>& segments = {});
    void ReconcileWithDisk();
    bool CheckScheduledRetryReady();
    void ExecuteScheduledRetry();
    std::vector<TaskLogEntry> GetLogs() const;
    void RestoreLogs(const std::vector<TaskLogEntry>& logs);

private:
    void ProbeAndStart();
    void LaunchWorkers();
    void RunMediaDownload();
    void ApplyMediaDestinationFilename(const std::wstring& cleanFilename, bool allowFormatSuffix);
    void NotifyEvent();
    void NotifyStateChanged();
    void OnSegmentProgress(int segId, uint64_t bytes);
    void OnSegmentFinished(int segId, bool success, const std::wstring& err);
    void CheckAllFinished();
    void AddLog(const std::wstring& msg, int level = 0);
    std::wstring GetTempFilePath() const;
    std::wstring GetStateFilePath() const;
    uint64_t FindMediaPartialBytesLocked() const;

    mutable std::recursive_mutex m_mutex;
    DownloadTaskInfo m_info;
    std::atomic<DownloadState> m_state = DownloadState::Queued;
    std::atomic<bool> m_pauseRequested = false;
    bool m_customFilenameSet = false;
    int m_retryCount = 0;
    std::atomic<bool> m_waitingRetry = false;
    std::chrono::steady_clock::time_point m_retryDueTime{};

    bool m_isMediaTask = false;
    std::wstring m_mediaFormat;
    HANDLE m_hMediaProcess = NULL;
    std::thread m_mediaThread;
    std::atomic<bool> m_isMediaCancelled = false;
    std::wstring m_activeMediaPartPath;
    
    std::wstring m_resolvedUrl;
    HANDLE m_hFile = INVALID_HANDLE_VALUE;
    std::vector<std::unique_ptr<SegmentWorker>> m_workers;
    std::thread m_probeThread;
    std::atomic<HINTERNET> m_hProbeSession = nullptr;
    std::atomic<HINTERNET> m_hProbeRequest = nullptr;
    std::atomic<bool> m_isProbing = false;
    std::atomic<bool> m_isFinishing = false;

    struct SpeedSample {
        std::chrono::steady_clock::time_point time;
        uint64_t downloadedBytes = 0;
    };
    mutable std::deque<SpeedSample> m_speedSamples;
    mutable double m_speed3sBps = 0.0;

    TaskEventCallback m_eventCb;
    TaskEventCallback m_stateChangedCb;
};

} // namespace Gety
