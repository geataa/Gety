#pragma once

#include "../core/Types.h"
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <atomic>
#include <functional>
#include <string>

namespace Gety {

class DownloadTask;

class SegmentWorker {
public:
    using LogCallback = std::function<void(const std::wstring& msg, int level)>;
    using ProgressCallback = std::function<void(int segmentId, uint64_t newDownloadedBytes)>;
    using FinishedCallback = std::function<void(int segmentId, bool success, const std::wstring& errMsg)>;

    SegmentWorker(int id,
                  const std::wstring& url,
                  uint64_t startByte,
                  uint64_t endByte,
                  uint64_t alreadyDownloaded,
                  HANDLE hFile,
                  LogCallback logCb,
                  ProgressCallback progCb,
                  FinishedCallback finishCb,
                  bool supportsRange = true,
                  const std::wstring& resolvedUrl = L"");

    ~SegmentWorker();

    void Start();
    void Stop();
    bool IsRunning() const { return m_isRunning.load(); }

    SegmentInfo GetInfo() const;

private:
    void WorkerProc();

    int m_id = 0;
    std::wstring m_url;
    std::wstring m_resolvedUrl;
    bool m_supportsRange = true;
    uint64_t m_startByte = 0;
    uint64_t m_endByte = 0;
    std::atomic<uint64_t> m_downloadedBytes = 0;
    std::atomic<int> m_status = 0; // 0=idle, 1=connecting, 2=downloading, 3=done, 4=error
    std::atomic<double> m_speedBps = 0.0;

    HANDLE m_hFile = INVALID_HANDLE_VALUE;
    std::atomic<HINTERNET> m_hRequest = nullptr;
    std::atomic<HINTERNET> m_hConnect = nullptr;
    std::atomic<HINTERNET> m_hSession = nullptr;
    std::atomic<bool> m_stopRequested = false;
    std::atomic<bool> m_isRunning = false;
    std::thread m_thread;

    LogCallback m_logCb;
    ProgressCallback m_progCb;
    FinishedCallback m_finishCb;
};

} // namespace Gety
