#pragma once

#include "DownloadTask.h"
#include <vector>
#include <memory>
#include <mutex>
#include <string>
#include <functional>

namespace Gety {

class DownloadManager {
public:
    using UpdateCallback = std::function<void()>;

    static DownloadManager& Instance();

    void Init(UpdateCallback onUpdate);
    void Shutdown();

    std::wstring AddTask(const std::wstring& url,
                         const std::wstring& saveDir = L"",
                         const std::wstring& category = L"Genel",
                         int splitParts = 10,
                         bool startImmediately = true,
                         const std::wstring& customFilename = L"");

    std::wstring AddMediaTask(const std::wstring& url,
                              const std::wstring& saveDir = L"",
                              const std::wstring& category = L"Video",
                              const std::wstring& title = L"",
                              const std::wstring& formatSelector = L"",
                              bool startImmediately = true);

    void StartTask(const std::wstring& id);
    void PauseTask(const std::wstring& id);
    void StopTask(const std::wstring& id);
    void RedownloadTask(const std::wstring& id);
    void DeleteTask(const std::wstring& id, bool deleteFile);
    bool UpdateTaskUrl(const std::wstring& id, const std::wstring& newUrl);

    void MoveTaskUp(const std::wstring& id);
    void MoveTaskDown(const std::wstring& id);

    void PauseAll();
    void ResumeAll();

    std::vector<DownloadTaskInfo> GetAllSnapshots() const;
    bool GetSnapshot(const std::wstring& id, DownloadTaskInfo& outInfo) const;

    int GetActiveCount() const;
    int GetCompletedCount() const;
    int GetPausedCount() const;
    double GetTotalSpeedBps() const;

    void SaveTasks();
    void LoadTasks();

    void TickQueue();

private:
    DownloadManager();
    ~DownloadManager();

    mutable std::mutex m_mutex;
    std::vector<std::shared_ptr<DownloadTask>> m_tasks;
    UpdateCallback m_onUpdate;

    std::thread m_timerThread;
    std::atomic<bool> m_running = false;
};

} // namespace Gety
