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

    void SetEventCallback(TaskEventCallback cb) { m_eventCb = cb; }

    void LoadState();
    void SaveState();
    bool UpdateUrl(const std::wstring& newUrl);
    void RestoreMetadata(const std::wstring& id,
                         DownloadState state,
                         const std::wstring& addedDate,
                         const std::wstring& completedDate,
                         const std::wstring& category);
    std::vector<TaskLogEntry> GetLogs() const;
    void RestoreLogs(const std::vector<TaskLogEntry>& logs);

private:
    void ProbeAndStart();
    void LaunchWorkers();
    void RunMediaDownload();
    void OnSegmentProgress(int segId, uint64_t bytes);
    void OnSegmentFinished(int segId, bool success, const std::wstring& err);
    void CheckAllFinished();
    void AddLog(const std::wstring& msg, int level = 0);
    std::wstring GetTempFilePath() const;
    std::wstring GetStateFilePath() const;

    mutable std::recursive_mutex m_mutex;
    DownloadTaskInfo m_info;
    std::atomic<DownloadState> m_state = DownloadState::Queued;
    bool m_customFilenameSet = false;
    int m_retryCount = 0;

    bool m_isMediaTask = false;
    std::wstring m_mediaFormat;
    HANDLE m_hMediaProcess = NULL;
    std::thread m_mediaThread;
    std::atomic<bool> m_isMediaCancelled = false;
    
    std::wstring m_resolvedUrl;
    HANDLE m_hFile = INVALID_HANDLE_VALUE;
    std::vector<std::unique_ptr<SegmentWorker>> m_workers;
    std::thread m_probeThread;
    std::atomic<bool> m_isProbing = false;
    std::atomic<bool> m_isFinishing = false;

    struct SpeedSample {
        std::chrono::steady_clock::time_point time;
        uint64_t downloadedBytes = 0;
    };
    mutable std::deque<SpeedSample> m_speedSamples;
    mutable double m_speed3sBps = 0.0;

    TaskEventCallback m_eventCb;
};

} // namespace Gety
